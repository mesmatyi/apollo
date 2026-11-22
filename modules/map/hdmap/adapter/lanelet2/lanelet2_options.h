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

#include <map>
#include <string>
#include <vector>

#include "gflags/gflags.h"

DECLARE_string(lanelet2_projection);
DECLARE_int32(lanelet2_utm_zone);
DECLARE_string(lanelet2_origin);
DECLARE_string(lanelet2_participant);
DECLARE_bool(lanelet2_bidirectional_lanes);
DECLARE_bool(lanelet2_non_driving_lanes);
DECLARE_bool(lanelet2_infer_junctions);
DECLARE_bool(lanelet2_use_apollo_tags);
DECLARE_double(lanelet2_sample_distance);
DECLARE_double(lanelet2_node_merge_tolerance);
DECLARE_double(lanelet2_default_speed_urban);
DECLARE_double(lanelet2_default_speed_nonurban);
DECLARE_double(lanelet2_default_speed_highway);

namespace apollo {
namespace hdmap {
namespace adapter {

/**
 * @brief How a Lanelet2 map is turned into an apollo::hdmap::Map.
 *
 * HDMapImpl::LoadMapFromFile() has no room for options, so the defaults come
 * from the --lanelet2_* flags (FromFlags()); tools and tests may fill the
 * struct directly.
 */
struct Lanelet2Options {
  /**
   * "utm":   project lat/lon to UTM. The zone is utm_zone, or the zone of the
   *          first node of the file when utm_zone is 0.
   * "local": take the local_x / local_y node tags (Autoware local maps).
   * anything starting with '+': a PROJ string, lat/lon is projected with it.
   */
  std::string projection = "utm";
  int utm_zone = 0;
  /**
   * With "utm": subtract the projected origin, which gives the coordinates
   * of lanelet::projection::UtmProjector(Origin(lat, lon)).
   */
  bool has_origin = false;
  double origin_lat = 0.0;
  double origin_lon = 0.0;

  /// Lanelet2 traffic participant the map is converted for; decides which
  /// lanelets become CITY_DRIVING lanes ("vehicle", "vehicle:taxi", ...).
  std::string participant = "vehicle";
  /// Lanelets that may be driven in both directions get a second lane
  /// "<id>_inv" (linked by self_reverse_lane_id).
  bool bidirectional_lanes = true;
  /// Keep bicycle lanes, walkways and shoulders as BIKING / SIDEWALK /
  /// SHOULDER lanes.
  bool non_driving_lanes = true;
  /// Create junctions where lanes cross and no Autoware intersection_area
  /// describes the intersection.
  bool infer_junctions = true;
  /// Honor the apollo:* tags written by tools/ros2/lanelet2/
  /// apollo_to_lanelet2.py: apollo:id (object ids) and apollo:turn.
  bool use_apollo_tags = true;

  /// Spacing of the points of computed centerlines, in meters.
  double sample_distance = 0.5;
  /// Nodes closer than this are treated as one (0: only identical ids).
  double node_merge_tolerance = 0.0;

  /// Speed limits (m/s) of lanelets without speed_limit tag or regulatory
  /// element.
  double default_speed_urban = 50.0 / 3.6;
  double default_speed_nonurban = 100.0 / 3.6;
  double default_speed_highway = 130.0 / 3.6;

  static Lanelet2Options FromFlags();
};

/**
 * @brief What the adapter did: element counts and everything it had to
 * skip or guess. Nothing in here is fatal.
 */
struct Lanelet2Report {
  /// "lanes", "junctions_inferred", "signals", ... -> count.
  std::map<std::string, int> counts;
  /// Warning category -> number of occurrences.
  std::map<std::string, int> warning_counts;
  /// The first messages of every category.
  std::vector<std::string> warnings;
  /// Tag values the adapter does not know ("way type=foo" -> count).
  std::map<std::string, int> unknown_tags;
  /// Projection that was applied (PROJ string, empty for "local").
  std::string projection;
  int utm_zone = 0;

  void Count(const std::string& name, int n = 1);
  void Warn(const std::string& category, const std::string& message);
  void UnknownTag(const std::string& what);
  int TotalWarnings() const;
  /// Multi-line summary for logs and the converter tool.
  std::string Summary() const;
  /// The same content as JSON (used by tools/ros2/lanelet2).
  std::string ToJson() const;
};

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
