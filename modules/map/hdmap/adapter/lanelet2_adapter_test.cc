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

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "cyber/common/file.h"
#include "modules/common/configs/config_gflags.h"
#include "modules/map/hdmap/hdmap_impl.h"
#include "modules/map/hdmap/hdmap_util.h"

// The maps are written by tools/ros2/lanelet2/make_test_maps.py, which also
// documents their layout. They are drawn in meters around kOriginLat/Lon.
namespace apollo {
namespace hdmap {
namespace adapter {
namespace {

constexpr char kTestData[] = "modules/map/hdmap/adapter/lanelet2/testdata/";
constexpr double kOriginLat = 49.0;
constexpr double kOriginLon = 8.4;

Lanelet2Options MeterOptions() {
  Lanelet2Options options;
  options.has_origin = true;
  options.origin_lat = kOriginLat;
  options.origin_lon = kOriginLon;
  return options;
}

bool Load(const std::string& name, const Lanelet2Options& options, Map* map,
          Lanelet2Report* report = nullptr) {
  return Lanelet2Adapter::LoadData(std::string(kTestData) + name, options, map,
                                   report);
}

const Lane* FindLane(const Map& map, const std::string& id) {
  for (const Lane& lane : map.lane()) {
    if (lane.id().id() == id) {
      return &lane;
    }
  }
  return nullptr;
}

template <typename Ids>
std::vector<std::string> IdList(const Ids& ids) {
  std::vector<std::string> list;
  for (const auto& id : ids) {
    list.push_back(id.id());
  }
  return list;
}

using Strings = std::vector<std::string>;

LaneBoundaryType::Type BoundaryType(const LaneBoundary& boundary) {
  EXPECT_EQ(1, boundary.boundary_type_size());
  EXPECT_EQ(1, boundary.boundary_type(0).types_size());
  return boundary.boundary_type(0).types(0);
}

const apollo::common::PointENU& CenterPoint(const Lane& lane, int index) {
  const auto& points = lane.central_curve().segment(0).line_segment().point();
  return points.Get(index < 0 ? points.size() + index : index);
}

// [start_s, end_s] of the overlap between a lane and another map object.
bool LaneOverlap(const Map& map, const std::string& lane_id,
                 const std::string& object_id, double* start_s, double* end_s,
                 bool* is_merge = nullptr) {
  for (const Overlap& overlap : map.overlap()) {
    const ObjectOverlapInfo* lane = nullptr;
    const ObjectOverlapInfo* object = nullptr;
    for (const ObjectOverlapInfo& info : overlap.object()) {
      if (info.id().id() == lane_id && lane == nullptr) {
        lane = &info;
      } else if (info.id().id() == object_id) {
        object = &info;
      }
    }
    if (lane != nullptr && object != nullptr) {
      *start_s = lane->lane_overlap_info().start_s();
      *end_s = lane->lane_overlap_info().end_s();
      if (is_merge != nullptr) {
        *is_merge = lane->lane_overlap_info().is_merge();
      }
      // every overlap is listed by both of its objects
      bool listed = false;
      for (const Id& id : FindLane(map, lane_id)->overlap_id()) {
        listed |= id.id() == overlap.id().id();
      }
      EXPECT_TRUE(listed) << overlap.id().id();
      return true;
    }
  }
  return false;
}

bool HasOverlap(const Map& map, const std::string& a, const std::string& b) {
  for (const Overlap& overlap : map.overlap()) {
    bool has_a = false;
    bool has_b = false;
    for (const ObjectOverlapInfo& info : overlap.object()) {
      has_a |= info.id().id() == a;
      has_b |= info.id().id() == b;
    }
    if (has_a && has_b) {
      return true;
    }
  }
  return false;
}

// Builds small Lanelet2 maps with local_x / local_y coordinates.
class LocalOsm {
 public:
  int Node(double x, double y) {
    out_ << "<node id='" << ++id_ << "' lat='0' lon='0'><tag k='local_x' v='"
         << x << "'/><tag k='local_y' v='" << y << "'/></node>\n";
    return id_;
  }
  int Way(const std::vector<int>& nodes,
          const std::map<std::string, std::string>& tags) {
    out_ << "<way id='" << ++id_ << "'>";
    for (const int node : nodes) {
      out_ << "<nd ref='" << node << "'/>";
    }
    Tags(tags);
    out_ << "</way>\n";
    return id_;
  }
  int Line(double x0, double y0, double x1, double y1,
           const std::map<std::string, std::string>& tags) {
    return Way({Node(x0, y0), Node(x1, y1)}, tags);
  }
  int Lanelet(int left, int right,
              std::map<std::string, std::string> tags = {}) {
    tags.emplace("type", "lanelet");
    tags.emplace("subtype", "road");
    out_ << "<relation id='" << ++id_ << "'><member type='way' ref='" << left
         << "' role='left'/><member type='way' ref='" << right
         << "' role='right'/>";
    Tags(tags);
    out_ << "</relation>\n";
    return id_;
  }
  // Writes the map to a temporary file and returns its path.
  std::string Write(const std::string& name) const {
    const std::string path = ::testing::TempDir() + "/" + name;
    std::ofstream file(path);
    file << "<?xml version='1.0'?>\n<osm version='0.6'>\n"
         << out_.str() << "</osm>\n";
    return path;
  }

 private:
  void Tags(const std::map<std::string, std::string>& tags) {
    for (const auto& tag : tags) {
      out_ << "<tag k='" << tag.first << "' v='" << tag.second << "'/>";
    }
  }
  std::ostringstream out_;
  int id_ = 0;
};

Lanelet2Options LocalOptions() {
  Lanelet2Options options;
  options.projection = "local";
  return options;
}

}  // namespace

TEST(Lanelet2AdapterTest, TwoLaneRoad) {
  Map map;
  Lanelet2Report report;
  ASSERT_TRUE(Load("two_lane_road.osm", MeterOptions(), &map, &report));
  EXPECT_EQ(0, report.TotalWarnings()) << report.Summary();
  ASSERT_EQ(4, map.lane_size());
  const Lane* right1 = FindLane(map, "100");
  const Lane* left1 = FindLane(map, "101");
  const Lane* right2 = FindLane(map, "102");
  const Lane* left2 = FindLane(map, "103");
  ASSERT_NE(nullptr, right1);
  ASSERT_NE(nullptr, left1);
  ASSERT_NE(nullptr, right2);
  ASSERT_NE(nullptr, left2);

  // geometry: the centerline is the middle of the bounds, 0.5 m spacing
  EXPECT_NEAR(60.0, right1->length(), 1e-3);
  EXPECT_NEAR(60.0, left1->length(), 1e-3);
  EXPECT_NEAR(40.0, right2->length(), 1e-3);
  EXPECT_NEAR(0.0, CenterPoint(*right1, 0).x(), 1e-3);
  EXPECT_NEAR(1.75, CenterPoint(*right1, 0).y(), 1e-3);
  EXPECT_NEAR(60.0, CenterPoint(*right1, -1).x(), 1e-3);
  EXPECT_NEAR(5.25, CenterPoint(*left1, -1).y(), 1e-3);
  const auto& center = right1->central_curve().segment(0);
  EXPECT_EQ(121, center.line_segment().point_size());
  EXPECT_NEAR(60.0, center.length(), 1e-3);
  EXPECT_NEAR(0.0, center.heading(), 1e-6);
  ASSERT_EQ(121, right1->left_sample_size());
  ASSERT_EQ(121, right1->right_sample_size());
  for (const auto& sample : right1->left_sample()) {
    EXPECT_NEAR(1.75, sample.width(), 1e-3);
  }
  EXPECT_NEAR(60.0, right1->left_sample(120).s(), 1e-3);
  EXPECT_NEAR(1.75, right1->right_sample(60).width(), 1e-3);

  // The dashed divider is stored against the driving direction: as left
  // boundary of lane 100 it runs with the lane.
  const auto& divider =
      right1->left_boundary().curve().segment(0).line_segment();
  ASSERT_EQ(3, divider.point_size());
  EXPECT_NEAR(0.0, divider.point(0).x(), 1e-3);
  EXPECT_NEAR(3.5, divider.point(0).y(), 1e-3);
  EXPECT_NEAR(60.0, divider.point(2).x(), 1e-3);
  EXPECT_NEAR(60.0, right1->left_boundary().length(), 1e-3);
  EXPECT_NEAR(
      0.0,
      right1->right_boundary().curve().segment(0).line_segment().point(0).y(),
      1e-3);

  // boundary types
  EXPECT_EQ(LaneBoundaryType::DOTTED_WHITE,
            BoundaryType(right1->left_boundary()));
  EXPECT_EQ(LaneBoundaryType::CURB, BoundaryType(right1->right_boundary()));
  EXPECT_EQ(LaneBoundaryType::SOLID_YELLOW,
            BoundaryType(left1->left_boundary()));
  EXPECT_EQ(LaneBoundaryType::DOTTED_WHITE,
            BoundaryType(left1->right_boundary()));
  EXPECT_FALSE(right1->left_boundary().virtual_());

  // topology
  EXPECT_EQ(Strings{"102"}, IdList(right1->successor_id()));
  EXPECT_EQ(Strings{"103"}, IdList(left1->successor_id()));
  EXPECT_EQ(Strings{"100"}, IdList(right2->predecessor_id()));
  EXPECT_EQ(Strings{"101"}, IdList(left2->predecessor_id()));
  EXPECT_TRUE(right1->predecessor_id().empty());
  EXPECT_TRUE(right2->successor_id().empty());
  EXPECT_EQ(Strings{"101"}, IdList(right1->left_neighbor_forward_lane_id()));
  EXPECT_TRUE(right1->right_neighbor_forward_lane_id().empty());
  EXPECT_EQ(Strings{"100"}, IdList(left1->right_neighbor_forward_lane_id()));
  EXPECT_TRUE(left1->left_neighbor_forward_lane_id().empty());
  EXPECT_EQ(Strings{"103"}, IdList(right2->left_neighbor_forward_lane_id()));
  EXPECT_TRUE(right1->left_neighbor_reverse_lane_id().empty());

  // attributes
  EXPECT_EQ(Lane::CITY_DRIVING, right1->type());
  EXPECT_EQ(Lane::NO_TURN, right1->turn());
  EXPECT_EQ(Lane::FORWARD, right1->direction());
  EXPECT_FALSE(right1->has_junction_id());

  // speed limits: "30", "40 km/h", a 60 km/h sign that wins over the tag
  // "10 mps", and the default of a nonurban road
  EXPECT_NEAR(30.0 / 3.6, right1->speed_limit(), 1e-6);
  EXPECT_NEAR(40.0 / 3.6, left1->speed_limit(), 1e-6);
  EXPECT_NEAR(60.0 / 3.6, right2->speed_limit(), 1e-6);
  EXPECT_NEAR(100.0 / 3.6, left2->speed_limit(), 1e-6);

  // lane 103 has an explicit centerline at y = 5.0 (the middle is 5.25)
  EXPECT_NEAR(40.0, left2->length(), 1e-3);
  EXPECT_EQ(81, left2->central_curve().segment(0).line_segment().point_size());
  EXPECT_NEAR(5.0, CenterPoint(*left2, 0).y(), 1e-3);
  EXPECT_NEAR(5.0, CenterPoint(*left2, 40).y(), 1e-3);
  EXPECT_NEAR(2.0, left2->left_sample(10).width(), 1e-3);
  EXPECT_NEAR(1.5, left2->right_sample(10).width(), 1e-3);

  // roads: one per section, lanes from left to right, with both edges
  ASSERT_EQ(2, map.road_size());
  const Road& road = map.road(0);
  EXPECT_EQ("road_101", road.id().id());
  EXPECT_EQ(Road::CITY_ROAD, road.type());
  EXPECT_FALSE(road.has_junction_id());
  ASSERT_EQ(1, road.section_size());
  EXPECT_EQ((Strings{"101", "100"}), IdList(road.section(0).lane_id()));
  const auto& edges = road.section(0).boundary().outer_polygon();
  ASSERT_EQ(2, edges.edge_size());
  EXPECT_EQ(BoundaryEdge::LEFT_BOUNDARY, edges.edge(0).type());
  EXPECT_NEAR(7.0, edges.edge(0).curve().segment(0).line_segment().point(0).y(),
              1e-3);
  EXPECT_EQ(BoundaryEdge::RIGHT_BOUNDARY, edges.edge(1).type());
  EXPECT_NEAR(0.0, edges.edge(1).curve().segment(0).line_segment().point(0).y(),
              1e-3);
  EXPECT_EQ((Strings{"103", "102"}), IdList(map.road(1).section(0).lane_id()));
  // distance of the centerline to the edges of the road
  EXPECT_NEAR(5.25, right1->left_road_sample(0).width(), 1e-3);
  EXPECT_NEAR(1.75, right1->right_road_sample(0).width(), 1e-3);
  EXPECT_NEAR(1.75, left1->left_road_sample(0).width(), 1e-3);
  EXPECT_NEAR(5.25, left1->right_road_sample(0).width(), 1e-3);

  // header: the frame is UTM zone 32 shifted to the origin
  EXPECT_NE(std::string::npos,
            map.header().projection().proj().find("+proj=tmerc"));
  EXPECT_NE(std::string::npos,
            map.header().projection().proj().find("+lon_0=9.0 +k=0.9996"));
  EXPECT_EQ("two_lane_road.osm", map.header().district());
  EXPECT_NEAR(kOriginLon, map.header().left(), 1e-3);
  EXPECT_NEAR(kOriginLat, map.header().bottom(), 1e-3);
  EXPECT_EQ(4, report.counts["lanes"]);
  EXPECT_EQ(2, report.counts["roads"]);
}

TEST(Lanelet2AdapterTest, UtmProjection) {
  // Without origin the coordinates are plain UTM; the zone comes from the
  // first node of the file.
  Map map;
  Lanelet2Report report;
  ASSERT_TRUE(Load("two_lane_road.osm", Lanelet2Options(), &map, &report));
  EXPECT_EQ("+proj=utm +zone=32 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
            map.header().projection().proj());
  EXPECT_EQ(32, report.utm_zone);
  const Lane* lane = FindLane(map, "100");
  ASSERT_NE(nullptr, lane);
  // UTM 32N of (49.0 N, 8.4 E) is (456114.596, 5427629.204)
  EXPECT_NEAR(456114.596, CenterPoint(*lane, 0).x(), 2e-3);
  EXPECT_NEAR(5427629.204 + 1.75, CenterPoint(*lane, 0).y(), 2e-3);
  EXPECT_NEAR(60.0, lane->length(), 1e-3);

  // A given zone is used as is: zone 31 lies west of the map, x grows.
  Lanelet2Options zone31;
  zone31.utm_zone = 31;
  ASSERT_TRUE(Load("two_lane_road.osm", zone31, &map));
  EXPECT_NE(std::string::npos,
            map.header().projection().proj().find("+zone=31 "));
  EXPECT_GT(CenterPoint(*FindLane(map, "100"), 0).x(), 800000.0);

  // Any PROJ string works as well.
  Lanelet2Options custom;
  custom.projection =
      "+proj=tmerc +lat_0=49 +lon_0=8.4 +k=1 +x_0=1000 +y_0=2000 "
      "+ellps=WGS84 +units=m +no_defs";
  ASSERT_TRUE(Load("two_lane_road.osm", custom, &map));
  EXPECT_EQ(custom.projection, map.header().projection().proj());
  EXPECT_NEAR(1000.0, CenterPoint(*FindLane(map, "100"), 0).x(), 0.05);
  EXPECT_NEAR(2001.75, CenterPoint(*FindLane(map, "100"), 0).y(), 0.05);

  Lanelet2Options broken;
  broken.projection = "mercator";
  EXPECT_FALSE(Load("two_lane_road.osm", broken, &map, &report));
  EXPECT_EQ(0, map.lane_size());
}

TEST(Lanelet2AdapterTest, Curve) {
  Map map;
  ASSERT_TRUE(Load("curve.osm", MeterOptions(), &map));
  ASSERT_EQ(3, map.lane_size());
  const Lane* bend = FindLane(map, "111");
  ASSERT_NE(nullptr, bend);
  // quarter circle of radius 20 m, drawn with 5 degree chords
  EXPECT_NEAR(M_PI_2 * 20.0, bend->length(), 0.03);
  for (const auto& sample : bend->left_sample()) {
    EXPECT_NEAR(1.75, sample.width(), 0.02);
  }
  for (const auto& sample : bend->right_sample()) {
    EXPECT_NEAR(1.75, sample.width(), 0.02);
  }
  for (const auto& point :
       bend->central_curve().segment(0).line_segment().point()) {
    EXPECT_NEAR(20.0, std::hypot(point.x(), point.y() - 20.0), 0.02);
  }
  EXPECT_NEAR(0.0, CenterPoint(*bend, 0).x(), 1e-3);
  EXPECT_NEAR(0.0, CenterPoint(*bend, 0).y(), 1e-3);
  EXPECT_NEAR(20.0, CenterPoint(*bend, -1).x(), 1e-3);
  EXPECT_NEAR(20.0, CenterPoint(*bend, -1).y(), 1e-3);
  // inner bound shorter than the outer one
  EXPECT_NEAR(M_PI_2 * 18.25, bend->left_boundary().length(), 0.03);
  EXPECT_NEAR(M_PI_2 * 21.75, bend->right_boundary().length(), 0.03);
  EXPECT_EQ(LaneBoundaryType::SOLID_WHITE, BoundaryType(bend->left_boundary()));
  EXPECT_EQ(LaneBoundaryType::CURB, BoundaryType(bend->right_boundary()));
  // a road that bends is not a turn, and nothing here is a junction
  EXPECT_EQ(Lane::NO_TURN, bend->turn());
  EXPECT_EQ(0, map.junction_size());
  EXPECT_EQ(Strings{"110"}, IdList(bend->predecessor_id()));
  EXPECT_EQ(Strings{"112"}, IdList(bend->successor_id()));
  // the centerlines of consecutive lanes meet
  const Lane* next = FindLane(map, "112");
  EXPECT_NEAR(CenterPoint(*bend, -1).x(), CenterPoint(*next, 0).x(), 1e-6);
  EXPECT_NEAR(CenterPoint(*bend, -1).y(), CenterPoint(*next, 0).y(), 1e-6);
  EXPECT_NEAR(20.0, next->length(), 1e-3);
}

TEST(Lanelet2AdapterTest, IntersectionTopology) {
  Map map;
  Lanelet2Report report;
  ASSERT_TRUE(Load("intersection.osm", MeterOptions(), &map, &report));
  EXPECT_EQ(0, report.TotalWarnings()) << report.Summary();
  // 8 arm lanes and 8 connectors; the crosswalk lanelet is no lane
  ASSERT_EQ(16, map.lane_size());
  EXPECT_EQ(nullptr, FindLane(map, "140"));
  ASSERT_EQ(16, map.road_size());

  const Lane* west_in = FindLane(map, "120");
  const Lane* west_out = FindLane(map, "121");
  ASSERT_NE(nullptr, west_in);
  ASSERT_NE(nullptr, west_out);
  EXPECT_NEAR(43.0, west_in->length(), 1e-3);
  // both directions share the divider: oncoming neighbors
  EXPECT_EQ(Strings{"121"}, IdList(west_in->left_neighbor_reverse_lane_id()));
  EXPECT_EQ(Strings{"120"}, IdList(west_out->left_neighbor_reverse_lane_id()));
  EXPECT_TRUE(west_in->left_neighbor_forward_lane_id().empty());
  // lane 121 drives against the direction its ways are stored in
  EXPECT_NEAR(-7.0, CenterPoint(*west_out, 0).x(), 1e-3);
  EXPECT_NEAR(1.75, CenterPoint(*west_out, 0).y(), 1e-3);
  EXPECT_NEAR(-50.0, CenterPoint(*west_out, -1).x(), 1e-3);
  EXPECT_EQ(LaneBoundaryType::SOLID_WHITE,
            BoundaryType(west_out->left_boundary()));
  EXPECT_EQ(LaneBoundaryType::CURB, BoundaryType(west_out->right_boundary()));

  EXPECT_EQ((Strings{"130", "131", "132"}), IdList(west_in->successor_id()));
  EXPECT_EQ((Strings{"134", "136"}), IdList(west_out->predecessor_id()));
  EXPECT_EQ((Strings{"133", "134", "135"}),
            IdList(FindLane(map, "124")->successor_id()));
  EXPECT_EQ((Strings{"131", "133"}),
            IdList(FindLane(map, "127")->predecessor_id()));

  // connectors: virtual bounds, turn from the geometry, junction
  const std::map<std::string, Lane::LaneTurn> turns = {
      {"130", Lane::NO_TURN},    {"131", Lane::LEFT_TURN},
      {"132", Lane::RIGHT_TURN}, {"133", Lane::NO_TURN},
      {"134", Lane::LEFT_TURN},  {"135", Lane::RIGHT_TURN},
      {"136", Lane::NO_TURN},    {"137", Lane::NO_TURN}};
  ASSERT_EQ(1, map.junction_size());
  const Junction& junction = map.junction(0);
  EXPECT_EQ("junction_130", junction.id().id());
  EXPECT_EQ(1, report.counts["junctions_inferred"]);
  for (const auto& turn : turns) {
    const Lane* lane = FindLane(map, turn.first);
    ASSERT_NE(nullptr, lane) << turn.first;
    EXPECT_EQ(turn.second, lane->turn()) << turn.first;
    EXPECT_EQ("junction_130", lane->junction_id().id()) << turn.first;
    EXPECT_TRUE(lane->left_boundary().virtual_());
    EXPECT_TRUE(lane->right_boundary().virtual_());
    EXPECT_EQ(LaneBoundaryType::UNKNOWN, BoundaryType(lane->left_boundary()));
    double start_s = 0.0;
    double end_s = 0.0;
    ASSERT_TRUE(LaneOverlap(map, turn.first, "junction_130", &start_s, &end_s));
    EXPECT_NEAR(0.0, start_s, 1e-3) << turn.first;
    EXPECT_NEAR(lane->length(), end_s, 1e-3) << turn.first;
  }
  EXPECT_NEAR(14.0, FindLane(map, "130")->length(), 1e-3);
  EXPECT_NEAR(M_PI_2 * 8.75, FindLane(map, "131")->length(), 0.06);
  EXPECT_NEAR(M_PI_2 * 5.25, FindLane(map, "132")->length(), 0.06);
  // the arms end where the junction begins
  for (const char* arm :
       {"120", "121", "122", "123", "124", "125", "126", "127"}) {
    EXPECT_FALSE(FindLane(map, arm)->has_junction_id()) << arm;
    EXPECT_EQ(Lane::NO_TURN, FindLane(map, arm)->turn()) << arm;
    EXPECT_FALSE(HasOverlap(map, arm, "junction_130")) << arm;
  }
  // The junction polygon is the hull of the connectors: the box
  // [-7, 7] x [-7, 7] without its corners.
  double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
  for (const auto& point : junction.polygon().point()) {
    min_x = std::min(min_x, point.x());
    max_x = std::max(max_x, point.x());
    min_y = std::min(min_y, point.y());
    max_y = std::max(max_y, point.y());
  }
  EXPECT_NEAR(-7.0, min_x, 1e-3);
  EXPECT_NEAR(7.0, max_x, 1e-3);
  EXPECT_NEAR(-7.0, min_y, 1e-3);
  EXPECT_NEAR(7.0, max_y, 1e-3);
  EXPECT_EQ(8, junction.polygon().point_size());
  // roads of the connectors belong to the junction
  int junction_roads = 0;
  for (const Road& road : map.road()) {
    if (road.has_junction_id()) {
      EXPECT_EQ("junction_130", road.junction_id().id());
      ++junction_roads;
    }
  }
  EXPECT_EQ(8, junction_roads);

  // lane/lane overlaps where connectors cross, merge or diverge
  double start_s = 0.0;
  double end_s = 0.0;
  bool is_merge = true;
  // west->east crosses south->north in x = 0..3.5, i.e. s = 7..10.5
  ASSERT_TRUE(LaneOverlap(map, "130", "133", &start_s, &end_s, &is_merge));
  EXPECT_NEAR(7.0, start_s, 0.51);
  EXPECT_NEAR(10.5, end_s, 0.51);
  EXPECT_FALSE(is_merge);
  ASSERT_TRUE(LaneOverlap(map, "133", "130", &start_s, &end_s));
  EXPECT_NEAR(3.5, start_s, 0.51);
  EXPECT_NEAR(7.0, end_s, 0.51);
  // west->east and south->east merge into lane 123
  ASSERT_TRUE(LaneOverlap(map, "130", "135", &start_s, &end_s, &is_merge));
  EXPECT_TRUE(is_merge);
  EXPECT_NEAR(14.0, end_s, 1e-3);
  // neighbors, consecutive lanes and oncoming lanes do not overlap
  EXPECT_FALSE(HasOverlap(map, "130", "136"));
  EXPECT_FALSE(HasOverlap(map, "120", "130"));
  EXPECT_FALSE(HasOverlap(map, "120", "121"));
  EXPECT_EQ(Strings{"136"},
            IdList(FindLane(map, "130")->left_neighbor_reverse_lane_id()));
}

TEST(Lanelet2AdapterTest, IntersectionTrafficControl) {
  Map map;
  Lanelet2Report report;
  ASSERT_TRUE(Load("intersection.osm", MeterOptions(), &map, &report));

  // traffic light with a stop line 2 m before the end of lane 120
  ASSERT_EQ(2, map.signal_size());
  const Signal& signal = map.signal(0);
  EXPECT_EQ("signal_1061", signal.id().id());
  EXPECT_EQ(Signal::MIX_3_VERTICAL, signal.type());
  ASSERT_EQ(3, signal.subsignal_size());
  EXPECT_EQ(Subsignal::CIRCLE, signal.subsignal(0).type());
  EXPECT_GT(signal.subsignal(0).location().z(),
            signal.subsignal(2).location().z());
  ASSERT_EQ(4, signal.boundary().point_size());
  EXPECT_NEAR(8.0, signal.boundary().point(0).x(), 1e-3);
  EXPECT_NEAR(-5.0, signal.boundary().point(0).y(), 1e-3);
  EXPECT_NEAR(6.2, signal.boundary().point(0).z(), 1e-3);
  EXPECT_NEAR(5.0, signal.boundary().point(2).z(), 1e-3);
  ASSERT_EQ(1, signal.stop_line_size());
  const auto& stop_line = signal.stop_line(0).segment(0).line_segment();
  ASSERT_EQ(2, stop_line.point_size());
  EXPECT_NEAR(-9.0, stop_line.point(0).x(), 1e-3);
  EXPECT_NEAR(-3.5, stop_line.point(0).y(), 1e-3);
  EXPECT_NEAR(0.0, stop_line.point(1).y(), 1e-3);
  double start_s = 0.0;
  double end_s = 0.0;
  ASSERT_TRUE(LaneOverlap(map, "120", "signal_1061", &start_s, &end_s));
  EXPECT_NEAR(41.0, start_s, 1e-3);
  EXPECT_NEAR(41.0, end_s, 1e-3);
  // the signal lists its overlaps: the lane and the junction behind it
  EXPECT_EQ(2, signal.overlap_id_size());
  EXPECT_TRUE(HasOverlap(map, "signal_1061", "junction_130"));
  // only lane 120 is controlled by it
  EXPECT_FALSE(HasOverlap(map, "130", "signal_1061"));
  EXPECT_FALSE(HasOverlap(map, "121", "signal_1061"));

  // traffic light without stop line: the vehicle stops at the lane end
  const Signal& second = map.signal(1);
  EXPECT_EQ("signal_1062", second.id().id());
  ASSERT_EQ(1, second.stop_line_size());
  EXPECT_NEAR(-7.0, second.stop_line(0).segment(0).line_segment().point(0).y(),
              1e-3);
  ASSERT_TRUE(LaneOverlap(map, "124", "signal_1062", &start_s, &end_s));
  EXPECT_NEAR(43.0, start_s, 0.011);
  EXPECT_LT(start_s, 43.0);

  // stop sign (traffic_sign with subtype stop_sign) on lane 122
  ASSERT_EQ(1, map.stop_sign_size());
  EXPECT_EQ("stop_sign_212", map.stop_sign(0).id().id());
  ASSERT_EQ(1, map.stop_sign(0).stop_line_size());
  EXPECT_NEAR(
      9.0, map.stop_sign(0).stop_line(0).segment(0).line_segment().point(0).x(),
      1e-3);
  ASSERT_TRUE(LaneOverlap(map, "122", "stop_sign_212", &start_s, &end_s));
  EXPECT_NEAR(41.0, start_s, 1e-3);
  EXPECT_TRUE(HasOverlap(map, "stop_sign_212", "junction_130"));

  // right_of_way: lane 126 yields
  ASSERT_EQ(1, map.yield_size());
  EXPECT_EQ("yield_213", map.yield(0).id().id());
  ASSERT_TRUE(LaneOverlap(map, "126", "yield_213", &start_s, &end_s));
  EXPECT_NEAR(41.0, start_s, 1e-3);
  EXPECT_FALSE(HasOverlap(map, "130", "yield_213"));

  // crosswalk across the east arm at x = 20..23
  ASSERT_EQ(1, map.crosswalk_size());
  const Crosswalk& crosswalk = map.crosswalk(0);
  EXPECT_EQ("crosswalk_140", crosswalk.id().id());
  ASSERT_EQ(4, crosswalk.polygon().point_size());
  EXPECT_EQ(2, crosswalk.overlap_id_size());
  // eastbound lane 123 starts at x = 7, westbound lane 122 at x = 50
  ASSERT_TRUE(LaneOverlap(map, "123", "crosswalk_140", &start_s, &end_s));
  EXPECT_NEAR(13.0, start_s, 0.02);
  EXPECT_NEAR(16.0, end_s, 0.02);
  ASSERT_TRUE(LaneOverlap(map, "122", "crosswalk_140", &start_s, &end_s));
  EXPECT_NEAR(27.0, start_s, 0.02);
  EXPECT_NEAR(30.0, end_s, 0.02);
  EXPECT_FALSE(HasOverlap(map, "120", "crosswalk_140"));

  EXPECT_EQ(2, report.counts["signals"]);
  EXPECT_EQ(1, report.counts["stop_signs"]);
  EXPECT_EQ(1, report.counts["yield_signs"]);
  EXPECT_EQ(1, report.counts["crosswalks"]);
  EXPECT_EQ(map.overlap_size(), report.counts["overlaps"]);
}

TEST(Lanelet2AdapterTest, AutowareLocalMap) {
  Map map;
  Lanelet2Report report;
  ASSERT_TRUE(Load("autoware_local.osm", LocalOptions(), &map, &report));
  EXPECT_EQ(0, report.TotalWarnings()) << report.Summary();
  // local_x / local_y are the coordinates, there is no projection
  EXPECT_FALSE(map.header().has_projection());
  EXPECT_EQ("7", map.header().version());
  const Lane* first = FindLane(map, "150");
  const Lane* turn = FindLane(map, "151");
  const Lane* last = FindLane(map, "152");
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, turn);
  ASSERT_NE(nullptr, last);
  EXPECT_NEAR(0.0, CenterPoint(*first, 0).x(), 1e-6);
  EXPECT_NEAR(1.5, CenterPoint(*first, 0).y(), 1e-6);
  EXPECT_NEAR(12.5, CenterPoint(*first, 0).z(), 1e-6);
  EXPECT_NEAR(20.0, first->length(), 1e-6);
  EXPECT_NEAR(1.5, first->left_sample(0).width(), 1e-6);
  EXPECT_EQ(Strings{"151"}, IdList(first->successor_id()));
  EXPECT_EQ(Strings{"152"}, IdList(turn->successor_id()));
  // tags: speed_limit in km/h, turn_direction, participant:vehicle
  EXPECT_EQ(Lane::CITY_DRIVING, first->type());
  EXPECT_NEAR(30.0 / 3.6, first->speed_limit(), 1e-6);
  EXPECT_NEAR(20.0 / 3.6, turn->speed_limit(), 1e-6);
  EXPECT_EQ(Lane::LEFT_TURN, turn->turn());
  EXPECT_EQ(Lane::NO_TURN, first->turn());

  // intersection_area: an explicit junction with the tagged lanelet
  ASSERT_EQ(1, map.junction_size());
  EXPECT_EQ("junction_1090", map.junction(0).id().id());
  EXPECT_EQ(4, map.junction(0).polygon().point_size());
  EXPECT_EQ(1, report.counts["junctions_explicit"]);
  EXPECT_EQ(0, report.counts["junctions_inferred"]);
  EXPECT_EQ("junction_1090", turn->junction_id().id());
  EXPECT_FALSE(first->has_junction_id());
  double start_s = 0.0;
  double end_s = 0.0;
  ASSERT_TRUE(LaneOverlap(map, "151", "junction_1090", &start_s, &end_s));
  EXPECT_NEAR(0.0, start_s, 1e-6);
  EXPECT_NEAR(20.0, end_s, 1e-6);

  // road_marking with a stop line at x = 18 on lane 150
  ASSERT_EQ(1, map.stop_sign_size());
  EXPECT_EQ("stop_sign_220", map.stop_sign(0).id().id());
  ASSERT_TRUE(LaneOverlap(map, "150", "stop_sign_220", &start_s, &end_s));
  EXPECT_NEAR(18.0, start_s, 1e-6);
  EXPECT_TRUE(HasOverlap(map, "stop_sign_220", "junction_1090"));

  // one_way=no: a second lane for the other direction
  const Lane* two_way = FindLane(map, "153");
  const Lane* inverted = FindLane(map, "153_inv");
  ASSERT_NE(nullptr, two_way);
  ASSERT_NE(nullptr, inverted);
  EXPECT_EQ(Strings{"153_inv"}, IdList(two_way->self_reverse_lane_id()));
  EXPECT_EQ(Strings{"153"}, IdList(inverted->self_reverse_lane_id()));
  EXPECT_NEAR(0.0, CenterPoint(*two_way, 0).x(), 1e-6);
  EXPECT_NEAR(30.0, CenterPoint(*inverted, 0).x(), 1e-6);
  EXPECT_NEAR(11.5, CenterPoint(*inverted, 0).y(), 1e-6);
  EXPECT_NEAR(0.0, CenterPoint(*inverted, -1).x(), 1e-6);
  EXPECT_NEAR(30.0, inverted->length(), 1e-6);
  EXPECT_NEAR(15.0 / 3.6, inverted->speed_limit(), 1e-6);
  // the left boundary of the inverted lane is the right one of the lanelet
  EXPECT_NEAR(
      10.0,
      inverted->left_boundary().curve().segment(0).line_segment().point(0).y(),
      1e-6);
  EXPECT_FALSE(HasOverlap(map, "153", "153_inv"));
  EXPECT_EQ(1, report.counts["lanes_inverted"]);

  // road_shoulder next to lane 150: a SHOULDER lane, but no neighbor (the
  // line between them is solid)
  const Lane* shoulder = FindLane(map, "154");
  ASSERT_NE(nullptr, shoulder);
  EXPECT_EQ(Lane::SHOULDER, shoulder->type());
  EXPECT_TRUE(first->right_neighbor_forward_lane_id().empty());
  EXPECT_TRUE(shoulder->left_neighbor_forward_lane_id().empty());
  EXPECT_EQ(6, map.lane_size());
  // every lane belongs to exactly one road
  int lanes_in_roads = 0;
  for (const Road& road : map.road()) {
    lanes_in_roads += road.section(0).lane_id_size();
  }
  EXPECT_EQ(6, lanes_in_roads);

  // options: keep two-way lanelets as one lane, drop non-driving lanes
  Lanelet2Options options = LocalOptions();
  options.bidirectional_lanes = false;
  options.non_driving_lanes = false;
  ASSERT_TRUE(Load("autoware_local.osm", options, &map));
  EXPECT_EQ(4, map.lane_size());
  EXPECT_EQ(nullptr, FindLane(map, "153_inv"));
  EXPECT_EQ(nullptr, FindLane(map, "154"));
  EXPECT_EQ(Lane::BIDIRECTION, FindLane(map, "153")->direction());

  // The same file without local projection: all nodes are at (0, 0).
  EXPECT_FALSE(Load("autoware_local.osm", MeterOptions(), &map));
}

TEST(Lanelet2AdapterTest, MalformedInput) {
  Map map;
  Lanelet2Report report;
  // Broken lanelets are skipped and reported, the rest is converted.
  ASSERT_TRUE(Load("malformed.osm", MeterOptions(), &map, &report));
  ASSERT_EQ(2, map.lane_size());
  EXPECT_NE(nullptr, FindLane(map, "160"));
  EXPECT_NE(nullptr, FindLane(map, "164"));
  EXPECT_EQ(6, report.counts["lanelets"]);
  EXPECT_EQ(3, report.warning_counts["lanelet_skipped"]);
  EXPECT_EQ(1, report.counts["lanelets_ignored"]);
  EXPECT_EQ(1, report.warning_counts["regulatory_element_skipped"]);
  EXPECT_EQ(1, report.warning_counts["way_with_missing_node"]);
  EXPECT_EQ(1, report.unknown_tags["way type=laser_fence"]);
  EXPECT_EQ(1, report.unknown_tags["lanelet subtype=hovercraft_lane"]);
  EXPECT_EQ(1, report.unknown_tags["relation type=route"]);
  EXPECT_EQ(1, report.unknown_tags["regulatory_element subtype=teleporter"]);
  EXPECT_EQ(9, report.TotalWarnings());
  EXPECT_NE(std::string::npos, report.Summary().find("lanelet 161"));
  EXPECT_NE(std::string::npos, report.ToJson().find("\"lanelet_skipped\": 3"));
  // an unknown line type is no lane marking Apollo knows
  EXPECT_EQ(LaneBoundaryType::UNKNOWN,
            BoundaryType(FindLane(map, "164")->right_boundary()));
  EXPECT_EQ(0, map.signal_size());
  // the result is a map the HD map engine accepts
  HDMapImpl hdmap;
  EXPECT_EQ(0, hdmap.LoadMapFromProto(map));

  // Unreadable files and maps without a usable lanelet are errors.
  EXPECT_FALSE(Load("does_not_exist.osm", MeterOptions(), &map, &report));
  const std::string not_xml = ::testing::TempDir() + "/not_xml.osm";
  std::ofstream(not_xml) << "this is not an OSM file";
  EXPECT_FALSE(Lanelet2Adapter::LoadData(not_xml, MeterOptions(), &map));
  const std::string no_osm = ::testing::TempDir() + "/no_osm.osm";
  std::ofstream(no_osm) << "<?xml version='1.0'?><gpx/>";
  EXPECT_FALSE(Lanelet2Adapter::LoadData(no_osm, MeterOptions(), &map));
  LocalOsm empty;
  empty.Line(0, 0, 10, 0, {{"type", "line_thin"}});
  Lanelet2Report empty_report;
  EXPECT_FALSE(Lanelet2Adapter::LoadData(empty.Write("no_lanelet.osm"),
                                         LocalOptions(), &map, &empty_report));
  EXPECT_EQ(1, empty_report.warning_counts["no_lanes"]);
  EXPECT_EQ(0, map.lane_size());
}

TEST(Lanelet2AdapterTest, LaneChangeRules) {
  // Two lanes side by side (heading +x); the line between them decides
  // who may change lanes, as in lanelet2's traffic rules.
  struct Case {
    std::map<std::string, std::string> tags;
    bool inverted;  // the shared way is stored against the driving direction
    LaneBoundaryType::Type right_lane_left;  // seen from the right lane
    LaneBoundaryType::Type left_lane_right;  // seen from the left lane
  };
  const LaneBoundaryType::Type dotted = LaneBoundaryType::DOTTED_WHITE;
  const LaneBoundaryType::Type solid = LaneBoundaryType::SOLID_WHITE;
  const std::vector<Case> cases = {
      {{{"type", "line_thin"}, {"subtype", "dashed"}}, false, dotted, dotted},
      {{{"type", "line_thick"}, {"subtype", "dashed"}}, true, dotted, dotted},
      {{{"type", "line_thin"}, {"subtype", "solid"}}, false, solid, solid},
      {{{"type", "line_thin"}}, false, solid, solid},
      {{{"type", "line_thin"}, {"subtype", "solid_solid"}},
       false,
       LaneBoundaryType::DOUBLE_YELLOW,
       LaneBoundaryType::DOUBLE_YELLOW},
      // dashed_solid: dashed for the lane left of the line
      {{{"type", "line_thin"}, {"subtype", "dashed_solid"}},
       false,
       solid,
       dotted},
      {{{"type", "line_thin"}, {"subtype", "dashed_solid"}},
       true,
       dotted,
       solid},
      {{{"type", "line_thin"}, {"subtype", "solid_dashed"}},
       false,
       dotted,
       solid},
      {{{"type", "line_thin"}, {"subtype", "solid_dashed"}},
       true,
       solid,
       dotted},
      {{{"type", "line_thin"}, {"subtype", "dashed"}, {"color", "yellow"}},
       false,
       LaneBoundaryType::DOTTED_YELLOW,
       LaneBoundaryType::DOTTED_YELLOW},
      {{{"type", "line_thin"}, {"subtype", "solid"}, {"color", "yellow"}},
       false,
       LaneBoundaryType::SOLID_YELLOW,
       LaneBoundaryType::SOLID_YELLOW},
      // explicit lane_change tags override the line type
      {{{"type", "line_thin"}, {"subtype", "solid"}, {"lane_change", "yes"}},
       false,
       dotted,
       dotted},
      {{{"type", "line_thin"}, {"subtype", "dashed"}, {"lane_change", "no"}},
       false,
       solid,
       solid},
      {{{"type", "line_thin"},
        {"subtype", "solid"},
        {"lane_change:left", "yes"}},
       false,
       dotted,
       solid},
      {{{"type", "line_thin"},
        {"subtype", "solid"},
        {"lane_change:right", "yes"}},
       false,
       solid,
       dotted},
      {{{"type", "line_thin"},
        {"subtype", "solid"},
        {"lane_change:left", "yes"}},
       true,
       solid,
       dotted},
      {{{"type", "curbstone"}, {"subtype", "low"}},
       false,
       LaneBoundaryType::CURB,
       LaneBoundaryType::CURB},
      {{{"type", "guard_rail"}},
       false,
       LaneBoundaryType::CURB,
       LaneBoundaryType::CURB},
      {{{"type", "virtual"}},
       false,
       LaneBoundaryType::UNKNOWN,
       LaneBoundaryType::UNKNOWN},
      {{{"type", "virtual"}, {"lane_change", "yes"}}, false, dotted, dotted},
      {{{"type", "bike_marking"}}, false, solid, solid},
  };
  for (size_t i = 0; i < cases.size(); ++i) {
    const Case& c = cases[i];
    LocalOsm osm;
    const int top = osm.Line(0, 6, 30, 6, {{"type", "road_border"}});
    const int middle = c.inverted ? osm.Line(30, 3, 0, 3, c.tags)
                                  : osm.Line(0, 3, 30, 3, c.tags);
    const int bottom = osm.Line(0, 0, 30, 0, {{"type", "road_border"}});
    const std::string left_id = std::to_string(osm.Lanelet(top, middle));
    const std::string right_id = std::to_string(osm.Lanelet(middle, bottom));
    Map map;
    ASSERT_TRUE(Lanelet2Adapter::LoadData(osm.Write("lane_change.osm"),
                                          LocalOptions(), &map));
    const Lane* left = FindLane(map, left_id);
    const Lane* right = FindLane(map, right_id);
    ASSERT_NE(nullptr, left);
    ASSERT_NE(nullptr, right);
    EXPECT_EQ(c.right_lane_left, BoundaryType(right->left_boundary()))
        << "case " << i;
    EXPECT_EQ(c.left_lane_right, BoundaryType(left->right_boundary()))
        << "case " << i;
    EXPECT_EQ(c.tags.at("type") == "virtual", right->left_boundary().virtual_())
        << "case " << i;
    // adjacent either way; topo_creator makes a lane change edge only
    // across a dotted boundary
    EXPECT_EQ(Strings{left_id}, IdList(right->left_neighbor_forward_lane_id()))
        << "case " << i;
    EXPECT_EQ(Strings{right_id}, IdList(left->right_neighbor_forward_lane_id()))
        << "case " << i;
    EXPECT_NEAR(0.0, CenterPoint(*right, 0).x(), 1e-6) << "case " << i;
    EXPECT_NEAR(1.5, CenterPoint(*right, 0).y(), 1e-6) << "case " << i;
    EXPECT_NEAR(30.0, CenterPoint(*left, -1).x(), 1e-6) << "case " << i;
  }
}

TEST(Lanelet2AdapterTest, ParticipantsAndLaneTypes) {
  LocalOsm osm;
  std::vector<int> lines;
  for (int i = 0; i <= 8; ++i) {
    lines.push_back(osm.Line(0, 3.0 * i, 20, 3.0 * i,
                             {{"type", "line_thin"}, {"subtype", "solid"}}));
  }
  auto id = [&](int index, std::map<std::string, std::string> tags) {
    return std::to_string(osm.Lanelet(lines[index + 1], lines[index], tags));
  };
  const std::string road = id(0, {{"subtype", "road"}});
  const std::string highway = id(1, {{"subtype", "highway"}});
  const std::string bus = id(2, {{"subtype", "bus_lane"}});
  const std::string bicycle = id(3, {{"subtype", "bicycle_lane"}});
  const std::string walkway = id(4, {{"subtype", "walkway"}});
  const std::string bikes_only =
      id(5, {{"subtype", "road"}, {"participant:bicycle", "yes"}});
  const std::string no_vehicles =
      id(6, {{"subtype", "road"}, {"participant:vehicle", "no"}});
  const std::string rail = id(7, {{"subtype", "rail"}});
  const std::string path = osm.Write("participants.osm");

  Map map;
  Lanelet2Report report;
  ASSERT_TRUE(Lanelet2Adapter::LoadData(path, LocalOptions(), &map, &report));
  EXPECT_EQ(Lane::CITY_DRIVING, FindLane(map, road)->type());
  EXPECT_EQ(Lane::CITY_DRIVING, FindLane(map, highway)->type());
  EXPECT_NEAR(130.0 / 3.6, FindLane(map, highway)->speed_limit(), 1e-6);
  EXPECT_EQ(Lane::BIKING, FindLane(map, bicycle)->type());
  EXPECT_EQ(Lane::SIDEWALK, FindLane(map, walkway)->type());
  EXPECT_EQ(Lane::BIDIRECTION, FindLane(map, walkway)->direction());
  EXPECT_EQ(Lane::BIKING, FindLane(map, bikes_only)->type());
  // nothing a car may use and nothing Apollo has a lane type for
  EXPECT_EQ(nullptr, FindLane(map, bus));
  EXPECT_EQ(nullptr, FindLane(map, no_vehicles));
  EXPECT_EQ(nullptr, FindLane(map, rail));
  EXPECT_EQ(3, report.counts["lanelets_ignored"]);
  // lanes of different types are no neighbors across a solid line
  EXPECT_EQ(Strings{highway},
            IdList(FindLane(map, road)->left_neighbor_forward_lane_id()));
  EXPECT_TRUE(FindLane(map, bicycle)->left_neighbor_forward_lane_id().empty());
  EXPECT_TRUE(FindLane(map, walkway)->right_neighbor_forward_lane_id().empty());
  // the highway lane makes its road a highway
  for (const Road& r : map.road()) {
    for (const Id& lane : r.section(0).lane_id()) {
      if (lane.id() == highway) {
        EXPECT_EQ(Road::HIGHWAY, r.type());
      }
    }
  }

  // A taxi may use the bus lane.
  Lanelet2Options taxi = LocalOptions();
  taxi.participant = "vehicle:taxi";
  ASSERT_TRUE(Lanelet2Adapter::LoadData(path, taxi, &map));
  ASSERT_NE(nullptr, FindLane(map, bus));
  EXPECT_EQ(Lane::CITY_DRIVING, FindLane(map, bus)->type());
  EXPECT_EQ(Strings{bus},
            IdList(FindLane(map, highway)->left_neighbor_forward_lane_id()));
}

TEST(Lanelet2AdapterTest, NodeMergeTolerance) {
  // Two consecutive lanelets drawn with their own nodes, 1 mm apart.
  LocalOsm osm;
  const int left1 = osm.Line(0, 3, 10, 3, {{"type", "line_thin"}});
  const int right1 = osm.Line(0, 0, 10, 0, {{"type", "line_thin"}});
  const int left2 = osm.Line(10.001, 3, 20, 3, {{"type", "line_thin"}});
  const int right2 = osm.Line(10.001, 0, 20, 0, {{"type", "line_thin"}});
  const std::string first = std::to_string(osm.Lanelet(left1, right1));
  const std::string second = std::to_string(osm.Lanelet(left2, right2));
  const std::string path = osm.Write("duplicated_nodes.osm");

  Map map;
  ASSERT_TRUE(Lanelet2Adapter::LoadData(path, LocalOptions(), &map));
  // Lanelet2 semantics: no shared nodes, no successor
  EXPECT_TRUE(FindLane(map, first)->successor_id().empty());

  Lanelet2Options options = LocalOptions();
  options.node_merge_tolerance = 0.01;
  Lanelet2Report report;
  ASSERT_TRUE(Lanelet2Adapter::LoadData(path, options, &map, &report));
  EXPECT_EQ(Strings{second}, IdList(FindLane(map, first)->successor_id()));
  EXPECT_EQ(Strings{first}, IdList(FindLane(map, second)->predecessor_id()));
  EXPECT_EQ(2, report.counts["nodes_merged"]);
}

TEST(Lanelet2AdapterTest, ApolloTags) {
  // apollo_to_lanelet2.py writes the Apollo ids and u-turns as tags.
  LocalOsm osm;
  const int top = osm.Line(0, 6, 30, 6, {{"type", "line_thin"}});
  const int middle = osm.Line(0, 3, 30, 3, {{"type", "line_thin"}});
  const int bottom = osm.Line(0, 0, 30, 0, {{"type", "line_thin"}});
  osm.Lanelet(top, middle,
              {{"apollo:id", "lane_a"},
               {"turn_direction", "left"},
               {"apollo:turn", "U_TURN"}});
  const std::string plain = std::to_string(
      osm.Lanelet(middle, bottom, {{"turn_direction", "right"}}));
  const std::string path = osm.Write("apollo_tags.osm");

  Map map;
  ASSERT_TRUE(Lanelet2Adapter::LoadData(path, LocalOptions(), &map));
  ASSERT_NE(nullptr, FindLane(map, "lane_a"));
  EXPECT_EQ(Lane::U_TURN, FindLane(map, "lane_a")->turn());
  EXPECT_EQ(Lane::RIGHT_TURN, FindLane(map, plain)->turn());
  EXPECT_EQ(Strings{"lane_a"},
            IdList(FindLane(map, plain)->left_neighbor_forward_lane_id()));
  EXPECT_EQ("road_lane_a", map.road(0).id().id());

  Lanelet2Options options = LocalOptions();
  options.use_apollo_tags = false;
  ASSERT_TRUE(Lanelet2Adapter::LoadData(path, options, &map));
  EXPECT_EQ(nullptr, FindLane(map, "lane_a"));
  EXPECT_EQ(Lane::LEFT_TURN, map.lane(0).turn());
}

// A Lanelet2 file is a base map like any other for the HD map engine.
TEST(Lanelet2AdapterTest, LoadThroughHDMap) {
  gflags::FlagSaver saver;
  FLAGS_lanelet2_origin = "49.0,8.4";
  const std::string osm_file = std::string(kTestData) + "intersection.osm";
  HDMapImpl hdmap;
  ASSERT_EQ(0, hdmap.LoadMapFromFile(osm_file));

  const LaneInfoConstPtr lane = hdmap.GetLaneById(MakeMapId("120"));
  ASSERT_NE(nullptr, lane);
  EXPECT_NEAR(43.0, lane->total_length(), 1e-3);
  EXPECT_NEAR(3.5, lane->GetWidth(10.0), 1e-3);
  EXPECT_EQ("road_120", lane->road_id().id());
  ASSERT_EQ(1U, lane->signals().size());
  EXPECT_TRUE(lane->cross_lanes().empty());
  EXPECT_EQ(7U, hdmap.GetLaneById(MakeMapId("130"))->cross_lanes().size() +
                    hdmap.GetLaneById(MakeMapId("130"))->junctions().size());
  ASSERT_EQ(1U, hdmap.GetLaneById(MakeMapId("122"))->stop_signs().size());
  ASSERT_EQ(1U, hdmap.GetLaneById(MakeMapId("122"))->crosswalks().size());
  ASSERT_EQ(1U, hdmap.GetLaneById(MakeMapId("126"))->yield_signs().size());

  // nearest lane of a pose on the eastbound lane of the west arm
  apollo::common::PointENU point;
  point.set_x(-20.0);
  point.set_y(-1.0);
  LaneInfoConstPtr nearest;
  double s = 0.0;
  double l = 0.0;
  ASSERT_EQ(0, hdmap.GetNearestLaneWithHeading(point, 5.0, 0.0, 1.0, &nearest,
                                               &s, &l));
  EXPECT_EQ("120", nearest->id().id());
  EXPECT_NEAR(30.0, s, 1e-3);
  EXPECT_NEAR(0.75, l, 1e-3);
  // heading west, the oncoming lane is found instead
  ASSERT_EQ(0, hdmap.GetNearestLaneWithHeading(point, 5.0, M_PI, 1.0, &nearest,
                                               &s, &l));
  EXPECT_EQ("121", nearest->id().id());

  std::vector<SignalInfoConstPtr> signals;
  ASSERT_EQ(0, hdmap.GetSignals(point, 20.0, &signals));
  ASSERT_EQ(1U, signals.size());
  EXPECT_EQ("signal_1061", signals[0]->id().id());
  ASSERT_EQ(0, hdmap.GetForwardNearestSignalsOnLane(point, 50.0, &signals));
  ASSERT_EQ(1U, signals.size());
  EXPECT_EQ("signal_1061", signals[0]->id().id());

  std::vector<JunctionInfoConstPtr> junctions;
  point.set_x(0.0);
  point.set_y(0.0);
  ASSERT_EQ(0, hdmap.GetJunctions(point, 1.0, &junctions));
  ASSERT_EQ(1U, junctions.size());
  EXPECT_EQ("junction_130", junctions[0]->id().id());
  std::vector<CrosswalkInfoConstPtr> crosswalks;
  point.set_x(21.0);
  ASSERT_EQ(0, hdmap.GetCrosswalks(point, 1.0, &crosswalks));
  EXPECT_EQ(1U, crosswalks.size());
  // the stop sign knows its junction (and through it the other stop signs)
  const StopSignInfoConstPtr stop_sign =
      hdmap.GetStopSignById(MakeMapId("stop_sign_212"));
  ASSERT_NE(nullptr, stop_sign);
  ASSERT_EQ(1U, stop_sign->OverlapJunctionIds().size());
  EXPECT_EQ("junction_130", stop_sign->OverlapJunctionIds()[0].id());

  // road boundaries next to the west arm, the junction inside the box
  std::vector<RoadROIBoundaryPtr> road_boundaries;
  std::vector<JunctionBoundaryPtr> junction_boundaries;
  point.set_x(-20.0);
  point.set_y(-1.0);
  ASSERT_EQ(0, hdmap.GetRoadBoundaries(point, 5.0, &road_boundaries,
                                       &junction_boundaries));
  EXPECT_EQ(2U, road_boundaries.size());  // one road per direction
  EXPECT_TRUE(junction_boundaries.empty());
  point.set_x(0.0);
  ASSERT_EQ(0, hdmap.GetRoadBoundaries(point, 2.0, &road_boundaries,
                                       &junction_boundaries));
  EXPECT_EQ(1U, junction_boundaries.size());

  // The .osm and the base_map.bin made from it are the same map.
  Map from_osm;
  ASSERT_TRUE(Lanelet2Adapter::LoadData(osm_file, &from_osm));
  const std::string bin_file = ::testing::TempDir() + "/lanelet2_base_map.bin";
  ASSERT_TRUE(cyber::common::SetProtoToBinaryFile(from_osm, bin_file));
  HDMapImpl from_bin;
  ASSERT_EQ(0, from_bin.LoadMapFromFile(bin_file));
  for (const Lane& pb_lane : from_osm.lane()) {
    const LaneInfoConstPtr a = hdmap.GetLaneById(pb_lane.id());
    const LaneInfoConstPtr b = from_bin.GetLaneById(pb_lane.id());
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);
    EXPECT_DOUBLE_EQ(a->total_length(), b->total_length());
    EXPECT_EQ(a->lane().DebugString(), b->lane().DebugString());
  }
}

// --map_dir with a Lanelet2 file under one of the default base map names.
TEST(Lanelet2AdapterTest, LoadThroughHDMapUtil) {
  gflags::FlagSaver saver;
  const std::string map_dir = ::testing::TempDir() + "/lanelet2_map_dir";
  ASSERT_TRUE(cyber::common::EnsureDirectory(map_dir));
  ASSERT_TRUE(cyber::common::CopyFile(std::string(kTestData) + "curve.osm",
                                      map_dir + "/lanelet2_map.osm"));
  FLAGS_map_dir = map_dir;
  FLAGS_test_base_map_filename = "";
  FLAGS_lanelet2_origin = "49.0,8.4";
  EXPECT_EQ(map_dir + "/lanelet2_map.osm", BaseMapFile());
  ASSERT_TRUE(HDMapUtil::ReloadBaseMap());
  const HDMap* hdmap = HDMapUtil::BaseMapPtr();
  ASSERT_NE(nullptr, hdmap);
  const LaneInfoConstPtr bend = hdmap->GetLaneById(MakeMapId("111"));
  ASSERT_NE(nullptr, bend);
  EXPECT_NEAR(M_PI_2 * 20.0, bend->total_length(), 0.03);
  EXPECT_NEAR(M_PI_4, bend->Heading(0.5 * bend->total_length()), 0.05);
  apollo::common::PointENU point;
  point.set_x(-10.0);
  point.set_y(0.3);
  LaneInfoConstPtr nearest;
  double s = 0.0;
  double l = 0.0;
  ASSERT_EQ(0, hdmap->GetNearestLane(point, &nearest, &s, &l));
  EXPECT_EQ("110", nearest->id().id());
  EXPECT_NEAR(10.0, s, 1e-3);
  EXPECT_NEAR(0.3, l, 1e-3);
}

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
