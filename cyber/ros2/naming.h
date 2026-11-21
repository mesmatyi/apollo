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

#ifndef CYBER_ROS2_NAMING_H_
#define CYBER_ROS2_NAMING_H_

#include <string>

namespace apollo {
namespace cyber {
namespace ros2 {

/**
 * @brief Maps a Cyber channel / service name onto a valid ROS 2 name.
 *
 * Rules (shared with tools/ros2/apollo_ros2_common.py and
 * cyber/python/cyber_py3/naming.py):
 *  1. characters outside [A-Za-z0-9_/] become '_'
 *  2. a leading '/' is added
 *  3. repeated '/' are collapsed and a trailing '/' removed
 *  4. tokens starting with a digit are prefixed with '_'
 *  5. names ROS 2 reserves for standard types (/tf, /tf_static, /clock,
 *     /rosout, /parameter_events) are moved under /apollo
 */
std::string ChannelToTopic(const std::string& channel);

/// Maps a Cyber node / component name onto a valid ROS 2 node name.
std::string ToNodeName(const std::string& name);

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_ROS2_NAMING_H_
