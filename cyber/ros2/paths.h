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

#ifndef CYBER_ROS2_PATHS_H_
#define CYBER_ROS2_PATHS_H_

#include <string>
#include <vector>

namespace apollo {
namespace cyber {
namespace ros2 {

/**
 * @brief Derives Apollo / Cyber search paths from the ament environment.
 *
 * For every variable that is not already set, a default is computed from
 * AMENT_PREFIX_PATH (and APOLLO_ROOT when set):
 *   CYBER_PATH                     <prefix>/share/cyber
 *   APOLLO_PLUGIN_INDEX_PATH       <prefix>/share/cyber_plugin_index
 *   APOLLO_PLUGIN_DESCRIPTION_PATH <prefix>/share/apollo
 *   APOLLO_PLUGIN_LIB_PATH         <prefix>/lib/apollo
 *   APOLLO_LIB_PATH                <prefix>/lib/apollo:<prefix>/lib
 *   APOLLO_CONF_PATH, APOLLO_FLAG_PATH, APOLLO_DAG_PATH
 *                                  $APOLLO_ROOT:<prefix>/share/apollo
 * Runs automatically when libcyber is loaded; calling it again is harmless.
 */
void SetDefaultSearchPaths();

/// Prefixes listed in AMENT_PREFIX_PATH.
std::vector<std::string> AmentPrefixes();

/// Maps "/apollo/<x>" to "$APOLLO_ROOT/<x>" when the former does not exist
/// and the latter does; returns `path` unchanged otherwise.
std::string ResolveApolloPath(const std::string& path);

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_ROS2_PATHS_H_
