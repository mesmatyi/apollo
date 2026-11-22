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
 * @file runtime.h
 * @brief Process-wide ROS 2 runtime used by the Cyber API implementation.
 *
 * This header pulls in rclcpp. Apollo module code normally does not need it;
 * it is the escape hatch for code that wants to mix Cyber API objects with
 * native rclcpp entities in the same process.
 */

#ifndef CYBER_ROS2_RUNTIME_H_
#define CYBER_ROS2_RUNTIME_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"

namespace apollo {
namespace cyber {
namespace ros2 {

class NodeHandle;

/**
 * @class Runtime
 * @brief Owns the rclcpp context integration and a background
 * MultiThreadedExecutor that spins every node created through the Cyber API
 * (except component nodes handed to an external rclcpp_components container,
 * which that container spins).
 */
class Runtime {
 public:
  static Runtime* Instance();

  /// Initializes rclcpp if nobody did yet and starts the background executor.
  /// Safe to call many times and from inside a component container.
  bool Init(int argc = 0, const char* const* argv = nullptr);

  /// True once Init() succeeded and until Shutdown().
  bool IsInitialized() const { return initialized_.load(); }

  /// True while the rclcpp context is valid.
  bool Ok() const;

  /// Stops the executor; shuts rclcpp down if this runtime initialized it.
  void Shutdown();

  /// Adds a node to the background executor.
  void AddNode(const rclcpp::Node::SharedPtr& node);
  /// Removes a node from the background executor.
  void RemoveNode(const rclcpp::Node::SharedPtr& node);

  /// The process node, spun by the background executor: used for service
  /// clients, standalone timers and graph queries, and the ROS 2 face of the
  /// process (its gflags as parameters, /rosout, /clock). Named
  /// `apollo_cyber_<pid>` unless the command line has `-r __node:=<name>`.
  std::shared_ptr<NodeHandle> AuxNode();

  /// NodeOptions of nodes created by the Cyber API: ordinary ROS 2 nodes
  /// (parameter services, parameter overrides, remapping rules and namespace
  /// from the command line of the process). A `__node:=` remap on the command
  /// line names the process node only; it must not rename every node.
  rclcpp::NodeOptions DefaultNodeOptions() const;

  /// NodeOptions of the process node (see AuxNode()).
  rclcpp::NodeOptions ProcessNodeOptions() const;

  /// The ROS 2 arguments of the process (starting with --ros-args) without
  /// node-name remaps.
  const std::vector<std::string>& NodeArguments() const {
    return node_arguments_;
  }

  rclcpp::Executor* Executor() { return executor_.get(); }

  size_t ExecutorThreads() const { return executor_threads_; }

 private:
  Runtime() = default;
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  void StartExecutor();
  void StopExecutor();
  void ParseProcessArguments(int argc, const char* const* argv);

  std::atomic<bool> initialized_{false};
  bool owns_rclcpp_ = false;
  size_t executor_threads_ = 0;
  std::mutex mutex_;
  std::shared_ptr<rclcpp::executors::MultiThreadedExecutor> executor_;
  std::thread spin_thread_;
  std::shared_ptr<NodeHandle> aux_node_;
  bool arguments_parsed_ = false;
  std::vector<std::string> ros_arguments_;    ///< as given on the command line
  std::vector<std::string> node_arguments_;   ///< without __node remaps
  std::string process_node_name_;
};

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_ROS2_RUNTIME_H_
