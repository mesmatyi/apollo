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
#include <vector>

#include "modules/map/hdmap/adapter/lanelet2/lanelet2_options.h"

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

/**
 * @brief Projects WGS84 latitude/longitude to map coordinates with PROJ.
 *
 * Unlike CoordinateConvertTool this is not a process-wide singleton, so
 * loading a Lanelet2 map never disturbs the OpenDRIVE adapter.
 */
class Lanelet2Projector {
 public:
  Lanelet2Projector() = default;
  ~Lanelet2Projector();
  Lanelet2Projector(const Lanelet2Projector&) = delete;
  Lanelet2Projector& operator=(const Lanelet2Projector&) = delete;

  /**
   * @param options projection, utm_zone and origin are used
   * @param reference_lat, reference_lon a point of the map; picks the UTM
   *        zone (and hemisphere) when options.utm_zone is 0
   */
  bool Init(const Lanelet2Options& options, double reference_lat,
            double reference_lon, std::string* error);

  /// True when coordinates come from local_x / local_y tags (no projection).
  bool is_local() const { return local_; }

  /**
   * @brief Projects in place: on input x holds longitudes and y latitudes
   * (degrees), on output map x and y (meters). Points PROJ rejects become
   * NaN.
   * @return false if the projector is unusable or no point could be
   *         projected
   */
  bool Forward(std::vector<double>* x, std::vector<double>* y,
               std::string* error) const;

  /// PROJ string of the map frame (what goes into Map.header.projection).
  const std::string& proj_string() const { return proj_string_; }
  /// UTM zone in use, 0 if the projection is not UTM.
  int utm_zone() const { return utm_zone_; }

  /// UTM zone of a longitude (1..60).
  static int UtmZone(double longitude);

 private:
  void Reset();

  bool local_ = false;
  int utm_zone_ = 0;
  std::string proj_string_;
  void* source_ = nullptr;  // projPJ
  void* target_ = nullptr;  // projPJ
};

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
