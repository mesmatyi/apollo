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

#include "cyber/ros2/runtime.h"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>

#include "cyber/common/log.h"
#include "cyber/ros2/naming.h"
#include "cyber/ros2/transport.h"

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

size_t ThreadsFromEnv() {
  const char* env = std::getenv("APOLLO_EXECUTOR_THREADS");
  if (env != nullptr) {
    int n = std::atoi(env);
    if (n > 0) {
      return static_cast<size_t>(n);
    }
  }
  // Cyber's default scheduler used up to 16 processors; mirror that but stay
  // within the hardware.
  size_t hw = std::max(2u, std::thread::hardware_concurrency());
  return std::min<size_t>(hw, 16);
}

}  // namespace

Runtime* Runtime::Instance() {
  // Intentionally leaked: nodes and endpoints may still reference the
  // runtime from static destructors of plugin libraries.
  static Runtime* instance = new Runtime();
  return instance;
}

Runtime::~Runtime() { Shutdown(); }

bool Runtime::Init(int argc, const char* const* argv) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_.load()) {
    return true;
  }
  if (!rclcpp::ok()) {
    // Only forward ROS arguments when the caller passed --ros-args
    // explicitly; Apollo binaries have their own gflags.
    std::vector<const char*> args;
    args.push_back(argc > 0 && argv != nullptr ? argv[0] : "apollo");
    bool ros_args = false;
    for (int i = 1; i < argc && argv != nullptr; ++i) {
      if (std::string(argv[i]) == "--ros-args") {
        ros_args = true;
      }
      if (ros_args) {
        args.push_back(argv[i]);
      }
    }
    rclcpp::InitOptions options;
    options.shutdown_on_signal = true;
    rclcpp::init(static_cast<int>(args.size()), args.data(), options,
                 rclcpp::SignalHandlerOptions::All);
    owns_rclcpp_ = true;
  }
  StartExecutor();
  initialized_.store(true);
  return true;
}

bool Runtime::Ok() const { return rclcpp::ok(); }

void Runtime::StartExecutor() {
  executor_threads_ = ThreadsFromEnv();
  rclcpp::ExecutorOptions options;
  executor_ = std::make_shared<rclcpp::executors::MultiThreadedExecutor>(
      options, executor_threads_);
  auto executor = executor_;
  spin_thread_ = std::thread([executor]() {
    try {
      executor->spin();
    } catch (const std::exception& e) {
      AERROR << "cyber ros2 executor stopped: " << e.what();
    }
  });
}

void Runtime::StopExecutor() {
  if (executor_) {
    executor_->cancel();
  }
  if (spin_thread_.joinable()) {
    if (spin_thread_.get_id() == std::this_thread::get_id()) {
      spin_thread_.detach();
    } else {
      spin_thread_.join();
    }
  }
}

void Runtime::Shutdown() {
  std::shared_ptr<NodeHandle> aux;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_.exchange(false)) {
      return;
    }
    aux = std::move(aux_node_);
  }
  aux.reset();
  StopExecutor();
  executor_.reset();
  if (owns_rclcpp_ && rclcpp::ok()) {
    rclcpp::shutdown();
  }
  owns_rclcpp_ = false;
}

void Runtime::AddNode(const rclcpp::Node::SharedPtr& node) {
  if (!initialized_.load()) {
    Init();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (executor_) {
    executor_->add_node(node, true);
  }
}

void Runtime::RemoveNode(const rclcpp::Node::SharedPtr& node) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (executor_) {
    try {
      executor_->remove_node(node, true);
    } catch (const std::exception& e) {
      ADEBUG << "remove_node: " << e.what();
    }
  }
}

std::shared_ptr<NodeHandle> Runtime::AuxNode() {
  if (!initialized_.load()) {
    Init();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (aux_node_) {
      return aux_node_;
    }
  }
  auto aux = NodeHandle::Create("apollo_cyber_" + std::to_string(getpid()), "");
  std::lock_guard<std::mutex> lock(mutex_);
  if (!aux_node_) {
    aux_node_ = aux;
  }
  return aux_node_;
}

rclcpp::NodeOptions Runtime::DefaultNodeOptions() const {
  rclcpp::NodeOptions options;
  // Cyber nodes are created programmatically; do not let global ROS
  // arguments (e.g. __node:= remaps) rename every node of the process.
  options.use_global_arguments(false);
  options.start_parameter_services(false);
  options.start_parameter_event_publisher(false);
  return options;
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
