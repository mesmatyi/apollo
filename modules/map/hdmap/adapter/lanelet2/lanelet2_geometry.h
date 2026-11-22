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

#include <utility>
#include <vector>

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

/**
 * Small 2D polyline/polygon toolbox of the Lanelet2 adapter. All predicates
 * work in the x/y plane; z is only carried along.
 */
struct Point3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

using Polyline = std::vector<Point3>;

struct BoundingBox {
  double min_x = 1e300;
  double min_y = 1e300;
  double max_x = -1e300;
  double max_y = -1e300;

  void Add(const Point3& point);
  void Add(const Polyline& points);
  bool Overlaps(const BoundingBox& other, double margin = 0.0) const;
};

double Distance2d(const Point3& a, const Point3& b);

/// Drops consecutive points closer than `epsilon`.
void RemoveDuplicatePoints(Polyline* line, double epsilon = 1e-6);

double PolylineLength(const Polyline& line);

/// accumulated[i] is the arc length from line[0] to line[i].
std::vector<double> AccumulatedLength(const Polyline& line);

/// Point at arc length `s` (clamped to the line).
Point3 PointAt(const Polyline& line, const std::vector<double>& accumulated,
               double s);

Polyline Reversed(const Polyline& line);

/// Inserts points so that no segment is longer than `max_spacing`; the
/// original vertices are kept.
Polyline Densify(const Polyline& line, double max_spacing);

/**
 * @brief Signed distance of a point to a polyline with the semantics of
 * lanelet::geometry::signedDistance: positive when the point is left of the
 * line (seen in its direction), negative when it is right of it.
 */
double SignedDistance(const Polyline& line, const Point3& point);

/**
 * @brief Closest point of a polyline.
 * @return the distance; `s` receives the arc length of the closest point and
 *         `foot` the closest point itself (both may be nullptr).
 */
double ProjectToPolyline(const Polyline& line,
                         const std::vector<double>& accumulated,
                         const Point3& point, double* s, Point3* foot);

struct PolylineIntersection {
  double s_a = 0.0;
  double s_b = 0.0;
  Point3 point;
};

/// All proper intersections of two polylines, ordered by s_a.
std::vector<PolylineIntersection> IntersectPolylines(
    const Polyline& a, const std::vector<double>& accumulated_a,
    const Polyline& b, const std::vector<double>& accumulated_b);

/// Signed area (positive for counter-clockwise polygons).
double SignedArea(const std::vector<Point3>& polygon);

/// Even-odd rule; points on the border count as inside.
bool PointInPolygon(const std::vector<Point3>& polygon, const Point3& point);

/// Distance of a point to the border of a polygon.
double DistanceToPolygonBorder(const std::vector<Point3>& polygon,
                               const Point3& point);

/// Convex hull (counter-clockwise, no repeated last point).
std::vector<Point3> ConvexHull(std::vector<Point3> points);

/// Drops polygon corners closer than `tolerance` to the line through their
/// neighbors.
void RemoveCollinearPoints(std::vector<Point3>* polygon, double tolerance);

/**
 * @brief Parts of a polyline that lie inside a polygon.
 * @return [s_begin, s_end] intervals in increasing order
 */
std::vector<std::pair<double, double>> RangesInsidePolygon(
    const Polyline& line, const std::vector<double>& accumulated,
    const std::vector<Point3>& polygon);

/// True if the segment [a, b] touches or crosses the polygon border or lies
/// inside the polygon.
bool SegmentTouchesPolygon(const Point3& a, const Point3& b,
                           const std::vector<Point3>& polygon);

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
