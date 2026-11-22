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
 * @file parameters.h
 * @brief ROS 2 parameters, simulation time and /rosout for Apollo processes.
 *
 * gflags: Apollo modules are configured with gflags. Every flag of a process
 * is a ROS 2 parameter `gflags.<flag name>`:
 *   - given to any Apollo node at start (launch file, `--ros-args -p
 *     gflags.map_dir:=...`, a YAML parameter file) it sets the flag and wins
 *     over flag files;
 *   - on the process node (`apollo_cyber_<pid>`, or the name given with
 *     `-r __node:=`) every flag can be listed, read and changed with
 *     `ros2 param list|get|set|dump`.
 * Flags belong to the process, not to a node: in a component container all
 * components share them.
 *
 * Simulation time: when `use_sim_time` is true on an Apollo node, Cyber's
 * Clock (apollo::cyber::Clock::Now()) follows /clock
 * (rosgraph_msgs/msg/Clock) instead of the system clock.
 *
 * /rosout: glog messages of level WARNING and above (APOLLO_ROSOUT_LEVEL =
 * DEBUG | INFO | WARN | ERROR | FATAL | OFF) are also published on /rosout.
 *
 * This header is free of rclcpp includes so component headers can use it.
 */

#ifndef CYBER_ROS2_PARAMETERS_H_
#define CYBER_ROS2_PARAMETERS_H_

#include <memory>
#include <string>
#include <vector>

namespace rclcpp {
class Node;
}  // namespace rclcpp

namespace apollo {
namespace cyber {
namespace ros2 {

/// Prefix of the parameters that stand for gflags ("gflags.").
extern const char kFlagParameterPrefix[];

/// Applies (again) the gflags that were given as ROS 2 parameters. Call it
/// after loading a flag file so parameters keep precedence.
void ApplyFlagOverrides();

/// Hooks a node up: applies its `gflags.*` parameter overrides and follows
/// its `use_sim_time` parameter. Called for every node the Cyber API creates
/// or wraps.
void AttachNode(const std::shared_ptr<rclcpp::Node>& node);

/// Makes `node` the process node: every gflag becomes a parameter of it
/// (`ros2 param set` changes the flag), it subscribes to /clock when
/// simulation time is on, and it publishes glog messages on /rosout.
void AttachProcessNode(const std::shared_ptr<rclcpp::Node>& node);

/// Releases everything that references the process node (before it dies).
void DetachProcessNode();

/// Tells the process node that flags may have been added or changed (a
/// library was loaded, a flag file was read); it catches up shortly after.
void NotifyFlagsChanged();

/// Declares new flags as parameters and refreshes the values of the others
/// right now. NotifyFlagsChanged() is the cheap way to ask for this.
void SyncFlagParameters();

/// Switches Cyber's Clock between the system clock and /clock.
void SetSimTime(bool enabled);
bool SimTimeEnabled();

/// Splits a command line into the arguments for the program and the ROS 2
/// arguments (the `--ros-args ... [--]` sections, including the flag).
void SplitRosArguments(int argc, const char* const* argv,
                       std::vector<std::string>* program_args,
                       std::vector<std::string>* ros_args);

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_ROS2_PARAMETERS_H_
