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
#include <cstring>
#include <fstream>

#include "cyber/common/log.h"
#include "cyber/ros2/naming.h"
#include "cyber/ros2/parameters.h"
#include "cyber/ros2/transport.h"

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

/// The command line of this process. Apollo binaries call cyber::Init() with
/// their name only, so the ROS 2 arguments are read from here.
std::vector<std::string> ProcessCommandLine() {
  std::vector<std::string> args;
  std::ifstream in("/proc/self/cmdline", std::ios::binary);
  std::string arg;
  while (std::getline(in, arg, '\0')) {
    args.push_back(arg);
  }
  return args;
}

/// True for a rule that renames the node: "__node:=x" or "__name:=x".
bool IsNodeNameRemap(const std::string& rule, std::string* name) {
  for (const char* prefix : {"__node:=", "__name:="}) {
    const size_t n = std::strlen(prefix);
    if (rule.compare(0, n, prefix) == 0) {
      *name = rule.substr(n);
      return true;
    }
  }
  return false;
}

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
  ParseProcessArguments(argc, argv);
  if (!rclcpp::ok()) {
    // Only the ROS 2 arguments go to rclcpp; Apollo binaries parse the rest
    // of their command line themselves (gflags, getopt).
    std::vector<const char*> args;
    args.push_back(argc > 0 && argv != nullptr ? argv[0] : "apollo");
    for (const auto& arg : ros_arguments_) {
      args.push_back(arg.c_str());
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

void Runtime::ParseProcessArguments(int argc, const char* const* argv) {
  if (arguments_parsed_) {
    return;
  }
  arguments_parsed_ = true;
  SplitRosArguments(argc, argv, nullptr, &ros_arguments_);
  if (ros_arguments_.empty()) {
    const auto command_line = ProcessCommandLine();
    std::vector<const char*> pointers;
    for (const auto& arg : command_line) {
      pointers.push_back(arg.c_str());
    }
    SplitRosArguments(static_cast<int>(pointers.size()), pointers.data(),
                      nullptr, &ros_arguments_);
  }
  for (size_t i = 0; i < ros_arguments_.size(); ++i) {
    const std::string& arg = ros_arguments_[i];
    std::string name;
    if ((arg == "-r" || arg == "--remap") && i + 1 < ros_arguments_.size() &&
        IsNodeNameRemap(ros_arguments_[i + 1], &name)) {
      process_node_name_ = name;
      ++i;
      continue;
    }
    node_arguments_.push_back(arg);
  }
  if (node_arguments_.size() == 1) {
    node_arguments_.clear();   // nothing but "--ros-args"
  }
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
  DetachProcessNode();
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
  // `-r __node:=<name>` names the process. When the process is somebody
  // else's (a component container, whose own node has that name) the process
  // node is called <name>_cyber.
  std::string name = "apollo_cyber_" + std::to_string(getpid());
  if (!process_node_name_.empty()) {
    name = owns_rclcpp_ ? process_node_name_ : process_node_name_ + "_cyber";
  }
  auto aux = NodeHandle::CreateProcessNode(name);
  bool created = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!aux_node_) {
      aux_node_ = aux;
      created = true;
    }
    aux = aux_node_;
  }
  if (created) {
    AttachProcessNode(aux->ros_node());
  }
  return aux;
}

rclcpp::NodeOptions Runtime::DefaultNodeOptions() const {
  rclcpp::NodeOptions options;
  // The global arguments would give every node of the process the same name
  // (`-r __node:=x`, which launch_ros adds for each executable); pass the
  // other rules -- topic remaps, namespace, parameters, parameter files --
  // as node arguments instead.
  options.use_global_arguments(false);
  options.arguments(node_arguments_);
  // Parameters given on the command line or in a launch file are visible
  // (`ros2 param list`) without the node having to declare each of them.
  options.automatically_declare_parameters_from_overrides(true);
  return options;
}

rclcpp::NodeOptions Runtime::ProcessNodeOptions() const {
  rclcpp::NodeOptions options = DefaultNodeOptions();
  // The process node declares one parameter per gflag (thousands): keep that
  // burst off /parameter_events, which every node of the graph listens to.
  options.start_parameter_event_publisher(false);
  return options;
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
