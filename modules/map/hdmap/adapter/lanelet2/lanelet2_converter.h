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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "modules/common_msgs/map_msgs/map.pb.h"

#include "modules/map/hdmap/adapter/lanelet2/lanelet2_geometry.h"
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_options.h"
#include "modules/map/hdmap/adapter/lanelet2/osm_parser.h"

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

/**
 * @brief Builds an apollo::hdmap::Map from the primitives of a Lanelet2 OSM
 * file. See docs/ros2/lanelet2.md for the mapping.
 *
 * The converter is single use: construct, Convert(), discard.
 */
class Lanelet2Converter {
 public:
  Lanelet2Converter(const Lanelet2Options& options, Lanelet2Report* report);

  /**
   * @param osm the parsed file
   * @param district written to Map.header.district (the file name)
   * @param map output
   * @return false if the map has no usable lanelet or cannot be projected
   */
  bool Convert(const OsmFile& osm, const std::string& district,
               apollo::hdmap::Map* map);

 private:
  struct LineString {
    OsmId id = 0;
    std::vector<OsmId> node_ids;  // canonical ids, no consecutive duplicates
    Polyline points;
    const OsmTags* tags = nullptr;
  };

  // A bound of a lanelet, oriented in the driving direction.
  struct Bound {
    OsmId way_id = 0;
    bool inverted = false;
    std::vector<OsmId> node_ids;
    Polyline points;
    const OsmTags* tags = nullptr;
  };

  enum class LaneletKind {
    DRIVING,
    BIKING,
    SIDEWALK,
    SHOULDER,
    CROSSWALK,
    IGNORED
  };

  struct Lanelet {
    OsmId id = 0;
    const OsmRelation* relation = nullptr;
    Bound left;
    Bound right;
    Polyline centerline;  // explicit centerline, empty if there is none
    LaneletKind kind = LaneletKind::IGNORED;
    bool one_way = true;
    std::vector<OsmId> regulatory_elements;
    int forward_lane = -1;
    int inverted_lane = -1;
  };

  struct RegulatoryElement {
    OsmId id = 0;
    const OsmRelation* relation = nullptr;
    std::string subtype;
    bool fallback = false;
    std::vector<OsmId> refers;        // ways
    std::vector<OsmId> ref_lines;     // ways
    std::vector<OsmId> yield;         // lanelets
    std::vector<OsmId> right_of_way;  // lanelets
    std::vector<int> lanelets;        // lanelets that reference the element
  };

  struct BoundaryStyle {
    LaneBoundaryType::Type type = LaneBoundaryType::UNKNOWN;
    bool is_virtual = false;
  };

  struct LaneData {
    std::string id;
    int lanelet = -1;
    bool inverted = false;
    Lane::LaneType type = Lane::CITY_DRIVING;
    Lane::LaneDirection direction = Lane::FORWARD;
    Bound left;
    Bound right;
    BoundaryStyle left_style;
    BoundaryStyle right_style;
    Polyline center;
    std::vector<double> center_s;
    double length = 0.0;
    std::vector<double> left_width;
    std::vector<double> right_width;
    std::vector<Point3> left_foot;
    std::vector<Point3> right_foot;
    std::vector<double> left_road_width;
    std::vector<double> right_road_width;
    std::vector<Point3> polygon;
    BoundingBox box;
    double speed_limit = 0.0;
    Lane::LaneTurn turn = Lane::NO_TURN;
    bool has_turn_tag = false;
    bool turn_is_final = false;  // given by an apollo:turn tag
    std::vector<int> successors;
    std::vector<int> predecessors;
    std::vector<int> left_forward;
    std::vector<int> right_forward;
    std::vector<int> left_reverse;
    std::vector<int> right_reverse;
    int self_reverse = -1;
    int road = -1;
    std::string junction_id;
    std::vector<std::string> overlap_ids;
  };

  struct RoadData {
    std::string id;
    std::vector<int> lanes;  // left to right
    bool highway = false;
    std::string junction_id;
  };

  struct JunctionData {
    std::string id;
    std::vector<Point3> polygon;
    std::set<int> member_lanes;
    bool inferred = false;
    std::vector<std::string> overlap_ids;
  };

  // Where a lane is controlled by a signal / stop sign / yield sign.
  struct ControlPoint {
    int lane = -1;
    double s = 0.0;
    std::string object_id;
    int kind = 0;  // ObjectKind
  };

  enum ObjectKind { kSignal = 0, kStopSign, kYieldSign, kCrosswalk, kJunction };

  bool BuildPoints(const OsmFile& osm);
  void BuildLineStrings(const OsmFile& osm);
  void BuildRegulatoryElements(const OsmFile& osm);
  void BuildLanelets(const OsmFile& osm);
  void BuildLanes();
  void BuildTopology();
  void BuildRoads();
  void BuildCrosswalks();
  void BuildTrafficControls();
  void BuildExplicitJunctions(const OsmFile& osm);
  void BuildLaneConflicts();
  void BuildJunctionOverlaps();
  void LinkControlsToJunctions();
  void InferTurns();
  void WriteMap(const OsmFile& osm, const std::string& district,
                apollo::hdmap::Map* map);

  // Lanelet2 traffic rules.
  bool CanPass(const OsmTags& tags, const std::string& participant) const;
  bool IsOneWay(const OsmTags& tags, const std::string& participant) const;
  LaneletKind Classify(const OsmTags& tags);
  double SpeedLimit(const Lanelet& lanelet, LaneletKind kind);
  BoundaryStyle ClassifyBound(const Bound& bound, bool is_left);

  bool MakeBound(const OsmRelation& relation, const std::string& role,
                 Bound* bound);
  bool FinishLaneGeometry(LaneData* lane) const;
  int AddLane(int lanelet_index, bool inverted, Lane::LaneType type,
              const std::string& id);
  std::string UniqueId(const OsmTags* tags, const std::string& fallback);

  // Part of a lane whose cross sections (left to right bound) touch a
  // polygon.
  bool CrossSectionRange(const LaneData& lane,
                         const std::vector<Point3>& polygon, double* begin,
                         double* end) const;
  // Stop position of a lane for a set of stop lines (or its end).
  double StopPosition(const LaneData& lane, const std::vector<OsmId>& ref_lines,
                      OsmId element_id, Polyline* stop_line);
  static void AppendStopLine(const Polyline& line,
                             google::protobuf::RepeatedPtrField<Curve>* out);

  std::string NextOverlapId();
  void AddLaneObjectOverlap(int lane, double start_s, double end_s,
                            const std::string& object_id, ObjectKind kind);
  void AddObjectOverlapId(const std::string& object_id, ObjectKind kind,
                          const std::string& overlap_id);
  static void SetObjectInfo(ObjectKind kind, ObjectOverlapInfo* info);

  const LineString* FindLineString(OsmId id) const;
  static void FillCurve(const Polyline& points, Curve* curve);

  const Lanelet2Options options_;
  Lanelet2Report* report_;

  std::map<OsmId, Point3> points_;
  std::map<OsmId, OsmId> canonical_;  // node id -> representative node id
  std::map<OsmId, LineString> line_strings_;
  std::map<OsmId, RegulatoryElement> regulatory_elements_;
  std::vector<Lanelet> lanelets_;
  std::map<OsmId, int> lanelet_index_;
  std::vector<LaneData> lanes_;
  std::vector<RoadData> roads_;
  std::vector<JunctionData> junctions_;
  std::vector<ControlPoint> control_points_;
  // lane -> (junction index, start_s, end_s)
  std::map<int, std::vector<std::pair<int, std::pair<double, double>>>>
      lane_junctions_;

  std::map<std::string, Signal> signals_;
  std::map<std::string, StopSign> stop_signs_;
  std::map<std::string, YieldSign> yield_signs_;
  std::map<std::string, Crosswalk> crosswalks_;
  std::map<std::string, std::vector<Point3>> crosswalk_polygons_;
  std::vector<Overlap> overlaps_;
  std::set<std::pair<int, std::string>> lane_object_overlaps_;
  std::set<std::string> used_ids_;
  std::map<std::string, int> apollo_id_uses_;
  std::string projection_;
};

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
