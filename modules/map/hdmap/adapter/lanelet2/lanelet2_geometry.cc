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

#include <algorithm>
#include <cmath>

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

namespace {

double Cross(const Point3& o, const Point3& a, const Point3& b) {
  return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Lanelet2's pointIsLeftOf(pSeg1, pSeg2, p).
bool IsLeftOf(const Point3& seg1, const Point3& seg2, const Point3& p) {
  return Cross(seg1, seg2, p) > 0.0;
}

Point3 Lerp(const Point3& a, const Point3& b, double t) {
  Point3 p;
  p.x = a.x + (b.x - a.x) * t;
  p.y = a.y + (b.y - a.y) * t;
  p.z = a.z + (b.z - a.z) * t;
  return p;
}

// Closest point of the segment [a, b] to p; returns the squared distance and
// the segment parameter in [0, 1].
double ClosestOnSegment(const Point3& a, const Point3& b, const Point3& p,
                        double* t) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double length_sq = dx * dx + dy * dy;
  double param = 0.0;
  if (length_sq > 0.0) {
    param = ((p.x - a.x) * dx + (p.y - a.y) * dy) / length_sq;
    param = std::max(0.0, std::min(1.0, param));
  }
  const double fx = a.x + dx * param - p.x;
  const double fy = a.y + dy * param - p.y;
  *t = param;
  return fx * fx + fy * fy;
}

// Intersection of the segments [a1, a2] and [b1, b2]; ta/tb are the segment
// parameters. Parallel (also collinear) segments do not intersect here.
bool IntersectSegments(const Point3& a1, const Point3& a2, const Point3& b1,
                       const Point3& b2, double* ta, double* tb) {
  const double rx = a2.x - a1.x;
  const double ry = a2.y - a1.y;
  const double sx = b2.x - b1.x;
  const double sy = b2.y - b1.y;
  const double denominator = rx * sy - ry * sx;
  const double scale = std::hypot(rx, ry) * std::hypot(sx, sy);
  if (scale <= 0.0 || std::fabs(denominator) <= 1e-12 * scale) {
    return false;
  }
  const double qx = b1.x - a1.x;
  const double qy = b1.y - a1.y;
  const double t = (qx * sy - qy * sx) / denominator;
  const double u = (qx * ry - qy * rx) / denominator;
  constexpr double kEps = 1e-9;
  if (t < -kEps || t > 1.0 + kEps || u < -kEps || u > 1.0 + kEps) {
    return false;
  }
  *ta = std::max(0.0, std::min(1.0, t));
  *tb = std::max(0.0, std::min(1.0, u));
  return true;
}

}  // namespace

void BoundingBox::Add(const Point3& point) {
  min_x = std::min(min_x, point.x);
  min_y = std::min(min_y, point.y);
  max_x = std::max(max_x, point.x);
  max_y = std::max(max_y, point.y);
}

void BoundingBox::Add(const Polyline& points) {
  for (const auto& point : points) {
    Add(point);
  }
}

bool BoundingBox::Overlaps(const BoundingBox& other, double margin) const {
  return min_x <= other.max_x + margin && other.min_x <= max_x + margin &&
         min_y <= other.max_y + margin && other.min_y <= max_y + margin;
}

double Distance2d(const Point3& a, const Point3& b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

void RemoveDuplicatePoints(Polyline* line, double epsilon) {
  size_t count = 0;
  for (const auto& point : *line) {
    if (count == 0 || Distance2d(point, (*line)[count - 1]) > epsilon) {
      (*line)[count++] = point;
    }
  }
  line->resize(count);
}

double PolylineLength(const Polyline& line) {
  double length = 0.0;
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    length += Distance2d(line[i], line[i + 1]);
  }
  return length;
}

std::vector<double> AccumulatedLength(const Polyline& line) {
  std::vector<double> accumulated(line.size(), 0.0);
  for (size_t i = 1; i < line.size(); ++i) {
    accumulated[i] = accumulated[i - 1] + Distance2d(line[i - 1], line[i]);
  }
  return accumulated;
}

Point3 PointAt(const Polyline& line, const std::vector<double>& accumulated,
               double s) {
  if (line.empty()) {
    return Point3();
  }
  if (s <= 0.0 || line.size() == 1) {
    return line.front();
  }
  if (s >= accumulated.back()) {
    return line.back();
  }
  const auto it = std::upper_bound(accumulated.begin(), accumulated.end(), s);
  const size_t i = static_cast<size_t>(it - accumulated.begin());  // > 0
  const double segment = accumulated[i] - accumulated[i - 1];
  const double t = segment > 0.0 ? (s - accumulated[i - 1]) / segment : 0.0;
  return Lerp(line[i - 1], line[i], t);
}

Polyline Reversed(const Polyline& line) {
  return Polyline(line.rbegin(), line.rend());
}

Polyline Densify(const Polyline& line, double max_spacing) {
  if (line.size() < 2 || max_spacing <= 0.0) {
    return line;
  }
  Polyline result;
  result.push_back(line.front());
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    const double length = Distance2d(line[i], line[i + 1]);
    const int pieces =
        std::max(1, static_cast<int>(std::ceil(length / max_spacing - 1e-6)));
    for (int k = 1; k <= pieces; ++k) {
      result.push_back(k == pieces ? line[i + 1]
                                   : Lerp(line[i], line[i + 1],
                                          static_cast<double>(k) / pieces));
    }
  }
  return result;
}

double SignedDistance(const Polyline& line, const Point3& point) {
  if (line.empty()) {
    return 0.0;
  }
  if (line.size() == 1) {
    return Distance2d(line.front(), point);
  }
  // The first segment with the smallest distance wins, as in Lanelet2.
  size_t best = 0;
  double best_sq = -1.0;
  double best_t = 0.0;
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    double t = 0.0;
    const double dist_sq = ClosestOnSegment(line[i], line[i + 1], point, &t);
    if (best_sq < 0.0 || dist_sq < best_sq) {
      best_sq = dist_sq;
      best = i;
      best_t = t;
    }
  }
  bool is_left = IsLeftOf(line[best], line[best + 1], point);
  // The closest point is the end of the segment: the following segment
  // decides at convex/concave corners (see Lanelet2's isLeftOf).
  if (best_t >= 1.0 && best + 2 < line.size()) {
    const Point3& next = line[best + 2];
    if (is_left != IsLeftOf(line[best + 1], next, point) &&
        is_left == IsLeftOf(line[best], line[best + 1], next)) {
      is_left = !is_left;
    }
  }
  const double distance = std::sqrt(best_sq);
  return is_left ? distance : -distance;
}

double ProjectToPolyline(const Polyline& line,
                         const std::vector<double>& accumulated,
                         const Point3& point, double* s, Point3* foot) {
  if (line.empty()) {
    return 0.0;
  }
  if (line.size() == 1) {
    if (s != nullptr) {
      *s = 0.0;
    }
    if (foot != nullptr) {
      *foot = line.front();
    }
    return Distance2d(line.front(), point);
  }
  size_t best = 0;
  double best_sq = -1.0;
  double best_t = 0.0;
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    double t = 0.0;
    const double dist_sq = ClosestOnSegment(line[i], line[i + 1], point, &t);
    if (best_sq < 0.0 || dist_sq < best_sq) {
      best_sq = dist_sq;
      best = i;
      best_t = t;
    }
  }
  if (s != nullptr) {
    *s = accumulated[best] +
         (accumulated[best + 1] - accumulated[best]) * best_t;
  }
  if (foot != nullptr) {
    *foot = Lerp(line[best], line[best + 1], best_t);
  }
  return std::sqrt(best_sq);
}

std::vector<PolylineIntersection> IntersectPolylines(
    const Polyline& a, const std::vector<double>& accumulated_a,
    const Polyline& b, const std::vector<double>& accumulated_b) {
  std::vector<PolylineIntersection> result;
  for (size_t i = 0; i + 1 < a.size(); ++i) {
    BoundingBox box_a;
    box_a.Add(a[i]);
    box_a.Add(a[i + 1]);
    for (size_t j = 0; j + 1 < b.size(); ++j) {
      BoundingBox box_b;
      box_b.Add(b[j]);
      box_b.Add(b[j + 1]);
      if (!box_a.Overlaps(box_b, 1e-9)) {
        continue;
      }
      double ta = 0.0;
      double tb = 0.0;
      if (!IntersectSegments(a[i], a[i + 1], b[j], b[j + 1], &ta, &tb)) {
        continue;
      }
      PolylineIntersection hit;
      hit.s_a =
          accumulated_a[i] + (accumulated_a[i + 1] - accumulated_a[i]) * ta;
      hit.s_b =
          accumulated_b[j] + (accumulated_b[j + 1] - accumulated_b[j]) * tb;
      hit.point = Lerp(a[i], a[i + 1], ta);
      // A crossing at a shared vertex is reported by both segments.
      if (!result.empty() && std::fabs(result.back().s_a - hit.s_a) < 1e-6 &&
          std::fabs(result.back().s_b - hit.s_b) < 1e-6) {
        continue;
      }
      result.push_back(hit);
    }
  }
  std::stable_sort(result.begin(), result.end(),
                   [](const PolylineIntersection& l,
                      const PolylineIntersection& r) { return l.s_a < r.s_a; });
  return result;
}

double SignedArea(const std::vector<Point3>& polygon) {
  double area = 0.0;
  for (size_t i = 0; i < polygon.size(); ++i) {
    const Point3& p = polygon[i];
    const Point3& q = polygon[(i + 1) % polygon.size()];
    area += p.x * q.y - q.x * p.y;
  }
  return 0.5 * area;
}

bool PointInPolygon(const std::vector<Point3>& polygon, const Point3& point) {
  if (polygon.size() < 3) {
    return false;
  }
  bool inside = false;
  for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const Point3& p = polygon[i];
    const Point3& q = polygon[j];
    // On the border?
    double t = 0.0;
    if (ClosestOnSegment(p, q, point, &t) < 1e-18) {
      return true;
    }
    if ((p.y > point.y) != (q.y > point.y)) {
      const double x = p.x + (point.y - p.y) / (q.y - p.y) * (q.x - p.x);
      if (point.x < x) {
        inside = !inside;
      }
    }
  }
  return inside;
}

double DistanceToPolygonBorder(const std::vector<Point3>& polygon,
                               const Point3& point) {
  double best_sq = -1.0;
  for (size_t i = 0; i < polygon.size(); ++i) {
    double t = 0.0;
    const double dist_sq = ClosestOnSegment(
        polygon[i], polygon[(i + 1) % polygon.size()], point, &t);
    if (best_sq < 0.0 || dist_sq < best_sq) {
      best_sq = dist_sq;
    }
  }
  return best_sq < 0.0 ? 0.0 : std::sqrt(best_sq);
}

std::vector<Point3> ConvexHull(std::vector<Point3> points) {
  std::sort(points.begin(), points.end(), [](const Point3& a, const Point3& b) {
    return a.x < b.x || (a.x == b.x && a.y < b.y);
  });
  points.erase(std::unique(points.begin(), points.end(),
                           [](const Point3& a, const Point3& b) {
                             return a.x == b.x && a.y == b.y;
                           }),
               points.end());
  if (points.size() < 3) {
    return points;
  }
  std::vector<Point3> hull(2 * points.size());
  size_t k = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    while (k >= 2 && Cross(hull[k - 2], hull[k - 1], points[i]) <= 0.0) {
      --k;
    }
    hull[k++] = points[i];
  }
  const size_t lower = k + 1;
  for (size_t i = points.size() - 1; i > 0; --i) {
    while (k >= lower &&
           Cross(hull[k - 2], hull[k - 1], points[i - 1]) <= 0.0) {
      --k;
    }
    hull[k++] = points[i - 1];
  }
  hull.resize(k - 1);
  return hull;
}

void RemoveCollinearPoints(std::vector<Point3>* polygon, double tolerance) {
  bool removed = true;
  while (removed && polygon->size() > 3) {
    removed = false;
    for (size_t i = 0; i < polygon->size(); ++i) {
      const Point3& previous =
          (*polygon)[(i + polygon->size() - 1) % polygon->size()];
      const Point3& next = (*polygon)[(i + 1) % polygon->size()];
      double t = 0.0;
      if (ClosestOnSegment(previous, next, (*polygon)[i], &t) <
          tolerance * tolerance) {
        polygon->erase(polygon->begin() + i);
        removed = true;
        break;
      }
    }
  }
}

std::vector<std::pair<double, double>> RangesInsidePolygon(
    const Polyline& line, const std::vector<double>& accumulated,
    const std::vector<Point3>& polygon) {
  std::vector<std::pair<double, double>> ranges;
  if (line.size() < 2 || polygon.size() < 3) {
    return ranges;
  }
  BoundingBox polygon_box;
  polygon_box.Add(polygon);
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    BoundingBox segment_box;
    segment_box.Add(line[i]);
    segment_box.Add(line[i + 1]);
    if (!segment_box.Overlaps(polygon_box, 1e-6)) {
      continue;
    }
    // Cut the segment at every crossing of the polygon border and classify
    // each piece by its middle point.
    std::vector<double> cuts = {0.0, 1.0};
    for (size_t j = 0; j < polygon.size(); ++j) {
      double ta = 0.0;
      double tb = 0.0;
      if (IntersectSegments(line[i], line[i + 1], polygon[j],
                            polygon[(j + 1) % polygon.size()], &ta, &tb)) {
        cuts.push_back(ta);
      }
    }
    std::sort(cuts.begin(), cuts.end());
    const double length = accumulated[i + 1] - accumulated[i];
    for (size_t k = 0; k + 1 < cuts.size(); ++k) {
      if (cuts[k + 1] - cuts[k] < 1e-12) {
        continue;
      }
      const Point3 middle =
          Lerp(line[i], line[i + 1], 0.5 * (cuts[k] + cuts[k + 1]));
      if (!PointInPolygon(polygon, middle)) {
        continue;
      }
      const double begin = accumulated[i] + length * cuts[k];
      const double end = accumulated[i] + length * cuts[k + 1];
      if (!ranges.empty() && begin - ranges.back().second < 1e-6) {
        ranges.back().second = end;
      } else {
        ranges.emplace_back(begin, end);
      }
    }
  }
  return ranges;
}

bool SegmentTouchesPolygon(const Point3& a, const Point3& b,
                           const std::vector<Point3>& polygon) {
  if (polygon.size() < 3) {
    return false;
  }
  if (PointInPolygon(polygon, a) || PointInPolygon(polygon, b)) {
    return true;
  }
  for (size_t j = 0; j < polygon.size(); ++j) {
    double ta = 0.0;
    double tb = 0.0;
    if (IntersectSegments(a, b, polygon[j], polygon[(j + 1) % polygon.size()],
                          &ta, &tb)) {
      return true;
    }
  }
  return false;
}

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
