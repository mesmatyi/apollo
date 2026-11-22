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
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_options.h"

#include <cstdio>
#include <sstream>

#include "cyber/common/log.h"

DEFINE_string(lanelet2_projection, "utm",
              "Projection of Lanelet2 (.osm) maps: \"utm\" (lat/lon to UTM), "
              "\"local\" (local_x/local_y node tags) or a PROJ string "
              "(\"+proj=tmerc ...\").");
DEFINE_int32(lanelet2_utm_zone, 0,
             "UTM zone of Lanelet2 maps for --lanelet2_projection=utm; 0 "
             "takes the zone of the first node of the file.");
DEFINE_string(lanelet2_origin, "",
              "\"lat,lon\": subtract the UTM coordinates of this point, like "
              "lanelet::projection::UtmProjector(Origin) does. Empty: plain "
              "UTM coordinates.");
DEFINE_string(lanelet2_participant, "vehicle",
              "Lanelet2 participant the map is converted for (vehicle, "
              "vehicle:car, vehicle:taxi, vehicle:bus, ...).");
DEFINE_bool(lanelet2_bidirectional_lanes, true,
            "Create a second lane <id>_inv for lanelets that may be driven "
            "in both directions.");
DEFINE_bool(lanelet2_non_driving_lanes, true,
            "Convert bicycle lanes, walkways and shoulders to BIKING, "
            "SIDEWALK and SHOULDER lanes instead of dropping them.");
DEFINE_bool(lanelet2_infer_junctions, true,
            "Create junctions where lanes cross and the Lanelet2 map has no "
            "Autoware intersection_area.");
DEFINE_bool(lanelet2_use_apollo_tags, true,
            "Honor the apollo:id and apollo:turn tags that "
            "apollo_to_lanelet2.py writes.");
DEFINE_double(lanelet2_sample_distance, 0.5,
              "Point spacing of computed lane centerlines, in meters.");
DEFINE_double(lanelet2_node_merge_tolerance, 0.0,
              "Treat Lanelet2 nodes closer than this (meters) as one node "
              "when deriving the lane topology; 0 compares node ids only.");
DEFINE_double(lanelet2_default_speed_urban, 50.0,
              "Speed limit (km/h) of urban road lanelets without one.");
DEFINE_double(lanelet2_default_speed_nonurban, 100.0,
              "Speed limit (km/h) of nonurban road lanelets without one.");
DEFINE_double(lanelet2_default_speed_highway, 130.0,
              "Speed limit (km/h) of highway lanelets without one.");

namespace apollo {
namespace hdmap {
namespace adapter {

namespace {

constexpr size_t kMaxMessagesPerCategory = 5;

std::string JsonEscape(const std::string& text) {
  std::string out;
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
          out += buffer;
        } else {
          out += c;
        }
    }
  }
  return out;
}

void AppendJsonMap(const std::map<std::string, int>& values,
                   std::ostringstream* out) {
  *out << "{";
  bool first = true;
  for (const auto& kv : values) {
    *out << (first ? "" : ", ") << "\"" << JsonEscape(kv.first)
         << "\": " << kv.second;
    first = false;
  }
  *out << "}";
}

}  // namespace

Lanelet2Options Lanelet2Options::FromFlags() {
  Lanelet2Options options;
  options.projection = FLAGS_lanelet2_projection;
  options.utm_zone = FLAGS_lanelet2_utm_zone;
  if (!FLAGS_lanelet2_origin.empty()) {
    double lat = 0.0;
    double lon = 0.0;
    if (std::sscanf(FLAGS_lanelet2_origin.c_str(), "%lf,%lf", &lat, &lon) ==
        2) {
      options.has_origin = true;
      options.origin_lat = lat;
      options.origin_lon = lon;
    } else {
      AERROR << "--lanelet2_origin must be \"lat,lon\", got \""
             << FLAGS_lanelet2_origin << "\"; ignored.";
    }
  }
  options.participant = FLAGS_lanelet2_participant;
  options.bidirectional_lanes = FLAGS_lanelet2_bidirectional_lanes;
  options.non_driving_lanes = FLAGS_lanelet2_non_driving_lanes;
  options.infer_junctions = FLAGS_lanelet2_infer_junctions;
  options.use_apollo_tags = FLAGS_lanelet2_use_apollo_tags;
  options.sample_distance = FLAGS_lanelet2_sample_distance;
  options.node_merge_tolerance = FLAGS_lanelet2_node_merge_tolerance;
  options.default_speed_urban = FLAGS_lanelet2_default_speed_urban / 3.6;
  options.default_speed_nonurban = FLAGS_lanelet2_default_speed_nonurban / 3.6;
  options.default_speed_highway = FLAGS_lanelet2_default_speed_highway / 3.6;
  return options;
}

void Lanelet2Report::Count(const std::string& name, int n) {
  counts[name] += n;
}

void Lanelet2Report::Warn(const std::string& category,
                          const std::string& message) {
  const int seen = warning_counts[category]++;
  if (static_cast<size_t>(seen) < kMaxMessagesPerCategory) {
    warnings.push_back(category + ": " + message);
  }
}

void Lanelet2Report::UnknownTag(const std::string& what) {
  ++unknown_tags[what];
}

int Lanelet2Report::TotalWarnings() const {
  int total = 0;
  for (const auto& kv : warning_counts) {
    total += kv.second;
  }
  for (const auto& kv : unknown_tags) {
    total += kv.second;
  }
  return total;
}

std::string Lanelet2Report::Summary() const {
  std::ostringstream out;
  out << "Lanelet2 conversion";
  if (!projection.empty()) {
    out << " (projection: " << projection << ")";
  }
  out << "\n  ";
  bool first = true;
  for (const auto& kv : counts) {
    out << (first ? "" : ", ") << kv.first << "=" << kv.second;
    first = false;
  }
  out << "\n  warnings: " << TotalWarnings();
  for (const auto& kv : warning_counts) {
    out << "\n    " << kv.first << ": " << kv.second;
  }
  for (const auto& kv : unknown_tags) {
    out << "\n    unknown tag " << kv.first << ": " << kv.second;
  }
  for (const auto& message : warnings) {
    out << "\n    - " << message;
  }
  return out.str();
}

std::string Lanelet2Report::ToJson() const {
  std::ostringstream out;
  out << "{\n  \"projection\": \"" << JsonEscape(projection) << "\",\n"
      << "  \"utm_zone\": " << utm_zone << ",\n  \"counts\": ";
  AppendJsonMap(counts, &out);
  out << ",\n  \"total_warnings\": " << TotalWarnings()
      << ",\n  \"warning_counts\": ";
  AppendJsonMap(warning_counts, &out);
  out << ",\n  \"unknown_tags\": ";
  AppendJsonMap(unknown_tags, &out);
  out << ",\n  \"warnings\": [";
  for (size_t i = 0; i < warnings.size(); ++i) {
    out << (i == 0 ? "" : ", ") << "\"" << JsonEscape(warnings[i]) << "\"";
  }
  out << "]\n}\n";
  return out.str();
}

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
