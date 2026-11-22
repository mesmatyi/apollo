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
#include "modules/map/hdmap/adapter/lanelet2_adapter.h"

#include <vector>

#include "cyber/common/log.h"
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_converter.h"
#include "modules/map/hdmap/adapter/lanelet2/osm_parser.h"

namespace apollo {
namespace hdmap {
namespace adapter {

bool Lanelet2Adapter::LoadData(const std::string& filename,
                               apollo::hdmap::Map* pb_map) {
  return LoadData(filename, Lanelet2Options::FromFlags(), pb_map, nullptr);
}

bool Lanelet2Adapter::LoadData(const std::string& filename,
                               const Lanelet2Options& options,
                               apollo::hdmap::Map* pb_map,
                               Lanelet2Report* report) {
  CHECK_NOTNULL(pb_map);
  Lanelet2Report local_report;
  if (report == nullptr) {
    report = &local_report;
  }

  lanelet2::OsmFile osm;
  std::vector<std::string> errors;
  const bool parsed = lanelet2::ParseOsmFile(filename, &osm, &errors);
  for (const std::string& error : errors) {
    report->Warn("osm_parser", error);
  }
  if (!parsed) {
    AERROR << "fail to load Lanelet2 map " << filename << ": "
           << (errors.empty() ? "unknown error" : errors.back());
    return false;
  }

  const size_t slash = filename.find_last_of('/');
  const std::string district =
      slash == std::string::npos ? filename : filename.substr(slash + 1);

  pb_map->Clear();
  lanelet2::Lanelet2Converter converter(options, report);
  if (!converter.Convert(osm, district, pb_map)) {
    AERROR << "fail to convert Lanelet2 map " << filename << "\n"
           << report->Summary();
    pb_map->Clear();
    return false;
  }
  if (report->TotalWarnings() > 0) {
    AWARN << filename << ": " << report->Summary();
  } else {
    AINFO << filename << ": " << report->Summary();
  }
  return true;
}

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
