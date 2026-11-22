/* Copyright 2026 The Apollo Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
=========================================================================*/
#pragma once

#include <string>

#include "modules/common_msgs/map_msgs/map.pb.h"

#include "modules/map/hdmap/adapter/lanelet2/lanelet2_options.h"

namespace apollo {
namespace hdmap {
namespace adapter {

/**
 * @brief Loads a Lanelet2 map (the OSM XML format of Lanelet2 and Autoware,
 * `.osm`) as an Apollo HD map. See docs/ros2/lanelet2.md.
 *
 * The file is read with tinyxml2 and projected with PROJ; the Lanelet2
 * library is not needed.
 */
class Lanelet2Adapter {
 public:
  /// Converts with the options given by the --lanelet2_* flags.
  static bool LoadData(const std::string& filename, apollo::hdmap::Map* pb_map);

  /**
   * @param report optional; receives the element counts and warnings
   * @return false if the file cannot be read or has no usable lanelet
   */
  static bool LoadData(const std::string& filename,
                       const Lanelet2Options& options,
                       apollo::hdmap::Map* pb_map,
                       Lanelet2Report* report = nullptr);
};

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
