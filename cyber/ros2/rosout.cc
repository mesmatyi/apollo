/******************************************************************************
 * Copyright 2026 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * Forwards glog messages (AINFO/AWARN/AERROR/AFATAL of every Apollo module)
 * to /rosout, the standard log topic of ROS 2, so rqt_console, `ros2 topic
 * echo /rosout` and log aggregators see Apollo next to every other node.
 * glog keeps writing its own files; nothing is printed twice on a console.
 */

#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "glog/logging.h"
#include "rcl/logging.h"
#include "rcl_interfaces/msg/log.hpp"
#include "rclcpp/rclcpp.hpp"

#include "cyber/binary.h"
#include "cyber/ros2/parameters.h"

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

/// Lowest glog severity that is forwarded; -1 forwards nothing.
int ThresholdFromEnv() {
  const char* env = std::getenv("APOLLO_ROSOUT_LEVEL");
  if (env == nullptr) {
    return google::GLOG_WARNING;
  }
  std::string level(env);
  for (auto& c : level) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  if (level == "DEBUG" || level == "INFO") {
    return google::GLOG_INFO;
  }
  if (level == "WARN" || level == "WARNING") {
    return google::GLOG_WARNING;
  }
  if (level == "ERROR") {
    return google::GLOG_ERROR;
  }
  if (level == "FATAL") {
    return google::GLOG_FATAL;
  }
  if (level == "OFF" || level == "NONE" || level == "0") {
    return -1;
  }
  return google::GLOG_WARNING;
}

uint8_t ToRosLevel(google::LogSeverity severity) {
  using Log = rcl_interfaces::msg::Log;
  switch (severity) {
    case google::GLOG_INFO:
      return Log::INFO;
    case google::GLOG_WARNING:
      return Log::WARN;
    case google::GLOG_ERROR:
      return Log::ERROR;
    default:
      return Log::FATAL;
  }
}

class RosoutSink : public google::LogSink {
 public:
  RosoutSink(const rclcpp::Node::SharedPtr& node, int threshold)
      : node_(node), threshold_(threshold) {
    rclcpp::PublisherOptions options;
    options.use_intra_process_comm = rclcpp::IntraProcessSetting::Disable;
    publisher_ = node->create_publisher<rcl_interfaces::msg::Log>(
        "/rosout", rclcpp::RosoutQoS(), options);
    thread_ = std::thread([this]() { Run(); });
  }

  ~RosoutSink() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  /// Called by glog with its own lock held: only queue the message here.
  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line,
            const google::LogMessageTime& time, const char* message,
            size_t message_len) override {
    (void)base_filename;
    (void)time;
    if (severity < threshold_) {
      return;
    }
    rcl_interfaces::msg::Log log;
    log.level = ToRosLevel(severity);
    log.file = full_filename == nullptr ? "" : full_filename;
    log.line = static_cast<uint32_t>(line < 0 ? 0 : line);
    // Cyber prefixes every message with "[module]".
    std::string text(message, message_len);
    std::string name = binary::GetName();
    if (!text.empty() && text.front() == '[') {
      const size_t end = text.find(']');
      if (end != std::string::npos && end > 1) {
        name = text.substr(1, end - 1);
        text.erase(0, end + 1);
      }
    }
    log.name = name.empty() ? "apollo" : name;
    log.msg = std::move(text);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stop_) {
        return;
      }
      if (queue_.size() >= kMaxQueue) {
        queue_.pop_front();   // keep the most recent messages
      }
      queue_.push_back(std::move(log));
    }
    cv_.notify_one();
  }

 private:
  static constexpr size_t kMaxQueue = 1000;

  void Run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      cv_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
      if (queue_.empty()) {
        if (stop_) {
          return;
        }
        continue;
      }
      rcl_interfaces::msg::Log log = std::move(queue_.front());
      queue_.pop_front();
      lock.unlock();
      try {
        if (rclcpp::ok()) {
          log.stamp = node_->get_clock()->now();
          publisher_->publish(log);
        }
      } catch (const std::exception&) {
        // The context is shutting down; drop the message.
      }
      lock.lock();
    }
  }

  rclcpp::Node::SharedPtr node_;
  int threshold_;
  rclcpp::Publisher<rcl_interfaces::msg::Log>::SharedPtr publisher_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<rcl_interfaces::msg::Log> queue_;
  bool stop_ = false;
  std::thread thread_;
};

std::mutex& SinkMutex() {
  static std::mutex* mutex = new std::mutex();
  return *mutex;
}

RosoutSink*& Sink() {
  static RosoutSink* sink = nullptr;
  return sink;
}

}  // namespace

void StartRosout(const std::shared_ptr<rclcpp::Node>& node) {
  const int threshold = ThresholdFromEnv();
  std::lock_guard<std::mutex> lock(SinkMutex());
  if (Sink() != nullptr || threshold < 0 || !node) {
    return;
  }
  // `--ros-args --disable-rosout-logs` switches /rosout off for the process.
  if (!rcl_logging_rosout_enabled() ||
      !node->get_node_options().enable_rosout()) {
    return;
  }
  try {
    Sink() = new RosoutSink(node, threshold);
    google::AddLogSink(Sink());
  } catch (const std::exception& e) {
    LOG(WARNING) << "cannot publish logs on /rosout: " << e.what();
    Sink() = nullptr;
  }
}

void StopRosout() {
  std::lock_guard<std::mutex> lock(SinkMutex());
  if (Sink() == nullptr) {
    return;
  }
  google::RemoveLogSink(Sink());
  delete Sink();
  Sink() = nullptr;
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
