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
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_geometry.h"

#include <cmath>
#include <string>

#include "gtest/gtest.h"

#include "modules/map/hdmap/adapter/lanelet2/lanelet2_projector.h"

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {
namespace {

Point3 P(double x, double y, double z = 0.0) {
  Point3 point;
  point.x = x;
  point.y = y;
  point.z = z;
  return point;
}

}  // namespace

TEST(Lanelet2GeometryTest, Polyline) {
  Polyline line = {P(0, 0), P(0, 0), P(3, 0), P(3, 4, 2.0)};
  RemoveDuplicatePoints(&line);
  ASSERT_EQ(3U, line.size());
  EXPECT_DOUBLE_EQ(7.0, PolylineLength(line));
  const std::vector<double> s = AccumulatedLength(line);
  EXPECT_DOUBLE_EQ(0.0, s[0]);
  EXPECT_DOUBLE_EQ(3.0, s[1]);
  EXPECT_DOUBLE_EQ(7.0, s[2]);
  const Point3 middle = PointAt(line, s, 5.0);
  EXPECT_DOUBLE_EQ(3.0, middle.x);
  EXPECT_DOUBLE_EQ(2.0, middle.y);
  EXPECT_DOUBLE_EQ(1.0, middle.z);
  EXPECT_DOUBLE_EQ(0.0, PointAt(line, s, -1.0).x);
  EXPECT_DOUBLE_EQ(4.0, PointAt(line, s, 100.0).y);

  const Polyline dense = Densify(line, 0.5);
  EXPECT_EQ(15U, dense.size());  // 6 + 8 pieces
  EXPECT_NEAR(7.0, PolylineLength(dense), 1e-12);
  EXPECT_DOUBLE_EQ(3.0, dense[6].x);  // the vertices are kept
  EXPECT_DOUBLE_EQ(0.0, dense[6].y);
  EXPECT_DOUBLE_EQ(3.0, Reversed(line).front().x);
}

TEST(Lanelet2GeometryTest, SignedDistance) {
  // lanelet2: positive on the left of the line, negative on the right
  const Polyline line = {P(0, 0), P(10, 0), P(10, 10)};
  EXPECT_DOUBLE_EQ(2.0, SignedDistance(line, P(5, 2)));
  EXPECT_DOUBLE_EQ(-2.0, SignedDistance(line, P(5, -2)));
  EXPECT_DOUBLE_EQ(3.0, SignedDistance(line, P(7, 5)));
  EXPECT_DOUBLE_EQ(-1.0, SignedDistance(line, P(11, 5)));
  // beyond the corner, on its outer (right) side
  EXPECT_NEAR(-std::sqrt(2.0), SignedDistance(line, P(11, -1)), 1e-12);
  // before the start
  EXPECT_NEAR(std::hypot(1.0, 1.0), SignedDistance(line, P(-1, 1)), 1e-12);

  double s = 0.0;
  Point3 foot;
  const std::vector<double> accumulated = AccumulatedLength(line);
  EXPECT_DOUBLE_EQ(2.0,
                   ProjectToPolyline(line, accumulated, P(8, 5), &s, &foot));
  EXPECT_DOUBLE_EQ(15.0, s);
  EXPECT_DOUBLE_EQ(10.0, foot.x);
  EXPECT_DOUBLE_EQ(5.0, foot.y);
}

TEST(Lanelet2GeometryTest, Intersections) {
  const Polyline a = {P(0, 0), P(10, 0), P(20, 0)};
  const Polyline b = {P(4, -5), P(4, 5), P(16, 5), P(16, -5)};
  const auto hits =
      IntersectPolylines(a, AccumulatedLength(a), b, AccumulatedLength(b));
  ASSERT_EQ(2U, hits.size());
  EXPECT_DOUBLE_EQ(4.0, hits[0].s_a);
  EXPECT_DOUBLE_EQ(5.0, hits[0].s_b);
  EXPECT_DOUBLE_EQ(16.0, hits[1].s_a);
  EXPECT_DOUBLE_EQ(27.0, hits[1].s_b);
  // touching at a vertex of `a` is one hit, parallel lines are none
  const Polyline c = {P(10, -1), P(10, 1)};
  EXPECT_EQ(1U,
            IntersectPolylines(a, AccumulatedLength(a), c, AccumulatedLength(c))
                .size());
  const Polyline d = {P(0, 1), P(20, 1)};
  EXPECT_TRUE(
      IntersectPolylines(a, AccumulatedLength(a), d, AccumulatedLength(d))
          .empty());
}

TEST(Lanelet2GeometryTest, Polygon) {
  // an L-shaped (concave) polygon
  const std::vector<Point3> polygon = {P(0, 0), P(10, 0), P(10, 4),
                                       P(4, 4), P(4, 10), P(0, 10)};
  EXPECT_DOUBLE_EQ(64.0, SignedArea(polygon));
  EXPECT_TRUE(PointInPolygon(polygon, P(2, 8)));
  EXPECT_TRUE(PointInPolygon(polygon, P(8, 2)));
  EXPECT_FALSE(PointInPolygon(polygon, P(8, 8)));
  EXPECT_TRUE(PointInPolygon(polygon, P(10, 2)));  // on the border
  EXPECT_DOUBLE_EQ(2.0, DistanceToPolygonBorder(polygon, P(2, 5)));

  // a line that enters the polygon twice
  const Polyline line = {P(-5, 7), P(7, 7), P(7, -5)};
  const auto ranges =
      RangesInsidePolygon(line, AccumulatedLength(line), polygon);
  ASSERT_EQ(2U, ranges.size());
  EXPECT_DOUBLE_EQ(5.0, ranges[0].first);
  EXPECT_DOUBLE_EQ(9.0, ranges[0].second);
  EXPECT_DOUBLE_EQ(15.0, ranges[1].first);
  EXPECT_DOUBLE_EQ(19.0, ranges[1].second);

  EXPECT_TRUE(SegmentTouchesPolygon(P(-1, 1), P(1, 1), polygon));
  EXPECT_TRUE(SegmentTouchesPolygon(P(1, 1), P(2, 2), polygon));
  EXPECT_FALSE(SegmentTouchesPolygon(P(6, 12), P(12, 6), polygon));
  EXPECT_TRUE(SegmentTouchesPolygon(P(5, -1), P(5, 11), polygon));

  std::vector<Point3> hull = ConvexHull(polygon);
  ASSERT_EQ(5U, hull.size());  // the inner corner (4, 4) is gone
  EXPECT_DOUBLE_EQ(82.0, SignedArea(hull));
  hull.insert(hull.begin() + 1, P(5.0, 0.001));  // nearly on an edge
  RemoveCollinearPoints(&hull, 0.01);
  EXPECT_EQ(5U, hull.size());
  EXPECT_NEAR(82.0, SignedArea(hull), 1e-9);

  BoundingBox box;
  box.Add(polygon);
  BoundingBox other;
  other.Add(P(11, 11));
  EXPECT_FALSE(box.Overlaps(other));
  EXPECT_TRUE(box.Overlaps(other, 1.5));
}

TEST(Lanelet2ProjectorTest, Utm) {
  EXPECT_EQ(32, Lanelet2Projector::UtmZone(8.4));
  EXPECT_EQ(10, Lanelet2Projector::UtmZone(-122.0));
  EXPECT_EQ(54, Lanelet2Projector::UtmZone(139.7));
  EXPECT_EQ(1, Lanelet2Projector::UtmZone(-180.0));
  EXPECT_EQ(60, Lanelet2Projector::UtmZone(180.0));

  Lanelet2Options options;
  Lanelet2Projector projector;
  std::string error;
  ASSERT_TRUE(projector.Init(options, 37.41, -122.01, &error)) << error;
  EXPECT_EQ(10, projector.utm_zone());
  EXPECT_FALSE(projector.is_local());
  EXPECT_EQ("+proj=utm +zone=10 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
            projector.proj_string());
  // Sunnyvale, the area of Apollo's demo map (computed with PROJ's cs2cs)
  std::vector<double> x = {-122.013332};
  std::vector<double> y = {37.413082};
  ASSERT_TRUE(projector.Forward(&x, &y, &error)) << error;
  EXPECT_NEAR(587313.787, x[0], 2e-3);
  EXPECT_NEAR(4141155.286, y[0], 2e-3);

  // southern hemisphere
  ASSERT_TRUE(projector.Init(options, -33.86, 151.21, &error)) << error;
  EXPECT_NE(std::string::npos, projector.proj_string().find("+south"));
  EXPECT_EQ(56, projector.utm_zone());

  // An origin moves the frame, like lanelet2's UtmProjector(Origin).
  options.has_origin = true;
  options.origin_lat = 37.413082;
  options.origin_lon = -122.013332;
  ASSERT_TRUE(projector.Init(options, 0.0, 0.0, &error)) << error;
  x = {-122.013332, -122.013332};
  y = {37.413082, 37.414082};
  ASSERT_TRUE(projector.Forward(&x, &y, &error)) << error;
  EXPECT_NEAR(0.0, x[0], 1e-3);
  EXPECT_NEAR(0.0, y[0], 1e-3);
  EXPECT_NEAR(-1.161, x[1], 2e-3);  // 0.001 degree north: grid convergence
  EXPECT_NEAR(110.945, y[1], 2e-3);

  options = Lanelet2Options();
  options.projection = "local";
  ASSERT_TRUE(projector.Init(options, 0.0, 0.0, &error));
  EXPECT_TRUE(projector.is_local());
  EXPECT_TRUE(projector.proj_string().empty());
  options.projection = "+proj=nonsense";
  EXPECT_FALSE(projector.Init(options, 0.0, 0.0, &error));
  options.projection = "utm";
  options.utm_zone = 61;
  EXPECT_FALSE(projector.Init(options, 0.0, 0.0, &error));
}

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
