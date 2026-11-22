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
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_converter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>

#include "cyber/common/log.h"
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_projector.h"

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

namespace {

constexpr char kApolloIdTag[] = "apollo:id";
constexpr char kApolloTurnTag[] = "apollo:turn";
// A stop line further away than this from a lane does not belong to it.
constexpr double kStopLineReach = 5.0;
// Lane/polygon overlaps shorter than this are contacts, not overlaps.
constexpr double kMinOverlapLength = 0.3;
// A centerline point this deep inside another lane is a conflict.
constexpr double kConflictMargin = 0.2;
// Share of a lane inside a junction polygon that makes it a junction lane.
constexpr double kJunctionLaneRatio = 0.8;
// Heading change that makes a junction lane a turn / a u-turn.
constexpr double kTurnAngle = 30.0 * M_PI / 180.0;
constexpr double kUTurnAngle = 150.0 * M_PI / 180.0;
constexpr double kDefaultSignalHeight = 1.0;

const std::string& EmptyString() {
  static const std::string empty;
  return empty;
}

bool StartsWith(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

std::string Trim(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

// lanelet::Attribute::asBool().
bool ParseBool(const std::string& text, bool fallback) {
  if (text == "true" || text == "yes" || text == "1") {
    return true;
  }
  if (text == "false" || text == "no" || text == "0") {
    return false;
  }
  return fallback;
}

bool TagBool(const OsmTags& tags, const std::string& key, bool fallback) {
  const auto it = tags.find(key);
  return it == tags.end() ? fallback : ParseBool(it->second, fallback);
}

bool ParseNumber(const std::string& text, double* value) {
  if (text.empty()) {
    return false;
  }
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(v)) {
    return false;
  }
  *value = v;
  return true;
}

// lanelet::Attribute::asVelocity(), but tolerant of blanks and case:
// "50", "50 km/h", "50kmh", "13.9 m/s", "10mps", "30 mph". No unit: km/h.
bool ParseVelocity(const std::string& raw, double* mps) {
  const std::string text = ToLower(Trim(raw));
  if (text.empty()) {
    return false;
  }
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || !std::isfinite(value) || value <= 0.0) {
    return false;
  }
  const std::string unit = Trim(end);
  if (unit.empty() || unit == "km/h" || unit == "kmh" || unit == "kph") {
    *mps = value / 3.6;
  } else if (unit == "m/s" || unit == "mps") {
    *mps = value;
  } else if (unit == "mph" || unit == "m/h") {
    *mps = value * 0.44704;
  } else {
    return false;
  }
  return true;
}

// Speed of the German traffic signs Lanelet2 knows ("de274-60"), or a plain
// velocity ("60", "60 km/h").
bool SignVelocity(const std::string& sign, double* mps) {
  if (sign == "de274" || sign == "de274_1") {
    *mps = 30.0 / 3.6;
    return true;
  }
  if (sign == "de310") {
    *mps = 50.0 / 3.6;
    return true;
  }
  for (const char* prefix : {"de274-", "de274_1-"}) {
    if (StartsWith(sign, prefix)) {
      double kmh = 0.0;
      if (ParseNumber(sign.substr(std::string(prefix).size()), &kmh) &&
          kmh > 0.0) {
        *mps = kmh / 3.6;
        return true;
      }
      return false;
    }
  }
  return ParseVelocity(sign, mps);
}

bool IsStopSign(const std::string& sign) {
  const std::string type = ToLower(sign);
  return type == "stop_sign" || type == "stop" || type == "de206" ||
         type == "usr1-1";
}

bool IsYieldSign(const std::string& sign) {
  const std::string type = ToLower(sign);
  return type == "yield" || type == "yield_sign" || type == "de205" ||
         type == "usr1-2";
}

// lanelet::traffic_rules::hasOverride().
bool HasOverride(const OsmTags& tags, const std::string& key) {
  const auto it = tags.lower_bound(key);
  return it != tags.end() && StartsWith(it->first, key);
}

// lanelet::traffic_rules::getOverride(): the first attribute (in key order)
// that starts with `prefix` and is a prefix of `prefix + participant`.
const std::string* GetOverride(const OsmTags& tags, const std::string& prefix,
                               const std::string& participant) {
  const std::string full = prefix + participant;
  for (auto it = tags.lower_bound(prefix); it != tags.end(); ++it) {
    if (!StartsWith(it->first, prefix)) {
      break;
    }
    if (StartsWith(full, it->first)) {
      return &it->second;
    }
  }
  return nullptr;
}

double NormalizeAngle(double angle) {
  return std::atan2(std::sin(angle), std::cos(angle));
}

Point3 Middle(const Point3& a, const Point3& b) {
  Point3 m;
  m.x = 0.5 * (a.x + b.x);
  m.y = 0.5 * (a.y + b.y);
  m.z = 0.5 * (a.z + b.z);
  return m;
}

// The point Lanelet2's align() looks at.
Point3 MiddlePoint(const Polyline& line) {
  return line.size() > 2 ? line[line.size() / 2]
                         : Middle(line.front(), line.back());
}

void SetPoint(const Point3& from, apollo::common::PointENU* to) {
  to->set_x(from.x);
  to->set_y(from.y);
  if (from.z != 0.0) {
    to->set_z(from.z);
  }
}

template <typename T>
void SortUnique(std::vector<T>* values) {
  std::sort(values->begin(), values->end());
  values->erase(std::unique(values->begin(), values->end()), values->end());
}

// Disjoint sets over [0, n).
class UnionFind {
 public:
  explicit UnionFind(size_t n) : parent_(n) {
    for (size_t i = 0; i < n; ++i) {
      parent_[i] = static_cast<int>(i);
    }
  }
  int Find(int x) {
    while (parent_[x] != x) {
      parent_[x] = parent_[parent_[x]];
      x = parent_[x];
    }
    return x;
  }
  void Union(int a, int b) {
    a = Find(a);
    b = Find(b);
    if (a != b) {
      parent_[std::max(a, b)] = std::min(a, b);  // smallest index is the root
    }
  }

 private:
  std::vector<int> parent_;
};

}  // namespace

Lanelet2Converter::Lanelet2Converter(const Lanelet2Options& options,
                                     Lanelet2Report* report)
    : options_(options), report_(report) {}

bool Lanelet2Converter::Convert(const OsmFile& osm, const std::string& district,
                                apollo::hdmap::Map* map) {
  report_->Count("osm_nodes", static_cast<int>(osm.nodes.size()));
  report_->Count("osm_ways", static_cast<int>(osm.ways.size()));
  report_->Count("osm_relations", static_cast<int>(osm.relations.size()));

  if (!BuildPoints(osm)) {
    return false;
  }
  // apollo:id tags are honored only where they are unique.
  if (options_.use_apollo_tags) {
    for (const auto& kv : osm.ways) {
      const auto it = kv.second.tags.find(kApolloIdTag);
      if (it != kv.second.tags.end() && !it->second.empty()) {
        ++apollo_id_uses_[it->second];
      }
    }
    for (const auto& kv : osm.relations) {
      const auto it = kv.second.tags.find(kApolloIdTag);
      if (it != kv.second.tags.end() && !it->second.empty()) {
        ++apollo_id_uses_[it->second];
      }
    }
    for (const auto& kv : apollo_id_uses_) {
      if (kv.second > 1) {
        report_->Warn("duplicate_apollo_id",
                      "apollo:id \"" + kv.first + "\" is used " +
                          std::to_string(kv.second) + " times and is ignored");
      }
    }
  }

  BuildLineStrings(osm);
  BuildRegulatoryElements(osm);
  BuildLanelets(osm);
  BuildLanes();
  if (lanes_.empty()) {
    report_->Warn("no_lanes", "the map has no usable lanelet");
    return false;
  }
  BuildTopology();
  BuildRoads();
  BuildCrosswalks();
  BuildTrafficControls();
  BuildExplicitJunctions(osm);
  BuildLaneConflicts();
  BuildJunctionOverlaps();
  LinkControlsToJunctions();
  InferTurns();
  WriteMap(osm, district, map);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Points and line strings

bool Lanelet2Converter::BuildPoints(const OsmFile& osm) {
  if (osm.nodes.empty()) {
    report_->Warn("no_nodes", "the file has no nodes");
    return false;
  }
  Lanelet2Projector projector;
  std::string error;
  const OsmNode& first = osm.nodes.at(osm.first_node_id);
  if (!projector.Init(options_, first.lat, first.lon, &error)) {
    report_->Warn("projection", error);
    return false;
  }

  if (projector.is_local()) {
    for (const auto& kv : osm.nodes) {
      Point3 point;
      if (!ParseNumber(TagOr(kv.second.tags, "local_x", EmptyString()),
                       &point.x) ||
          !ParseNumber(TagOr(kv.second.tags, "local_y", EmptyString()),
                       &point.y)) {
        report_->Warn("node_without_local_xy",
                      "node " + std::to_string(kv.first) +
                          " has no local_x/local_y tags");
        continue;
      }
      points_[kv.first] = point;
    }
  } else {
    std::vector<double> x;
    std::vector<double> y;
    x.reserve(osm.nodes.size());
    y.reserve(osm.nodes.size());
    for (const auto& kv : osm.nodes) {
      x.push_back(kv.second.lon);
      y.push_back(kv.second.lat);
    }
    if (!projector.Forward(&x, &y, &error)) {
      report_->Warn("projection", error);
      return false;
    }
    size_t i = 0;
    for (const auto& kv : osm.nodes) {
      if (std::isfinite(x[i]) && std::isfinite(y[i])) {
        Point3 point;
        point.x = x[i];
        point.y = y[i];
        points_[kv.first] = point;
      } else {
        report_->Warn("node_not_projected", "node " + std::to_string(kv.first) +
                                                " cannot be projected");
      }
      ++i;
    }
  }
  for (auto& kv : points_) {
    double ele = 0.0;
    if (ParseNumber(TagOr(osm.nodes.at(kv.first).tags, "ele", EmptyString()),
                    &ele)) {
      kv.second.z = ele;
    }
  }
  if (points_.empty()) {
    report_->Warn("no_nodes", "no node has usable coordinates");
    return false;
  }
  projection_ = projector.proj_string();
  report_->projection = projection_;
  report_->utm_zone = projector.utm_zone();

  // Optionally merge nodes that lie on top of each other.
  const double tolerance = options_.node_merge_tolerance;
  if (tolerance > 0.0) {
    std::vector<OsmId> ids;
    ids.reserve(points_.size());
    std::map<std::pair<int64_t, int64_t>, std::vector<int>> grid;
    for (const auto& kv : points_) {
      const int index = static_cast<int>(ids.size());
      ids.push_back(kv.first);
      grid[{static_cast<int64_t>(std::floor(kv.second.x / tolerance)),
            static_cast<int64_t>(std::floor(kv.second.y / tolerance))}]
          .push_back(index);
    }
    UnionFind sets(ids.size());
    for (const auto& cell : grid) {
      for (int64_t dx = -1; dx <= 1; ++dx) {
        for (int64_t dy = -1; dy <= 1; ++dy) {
          const auto other =
              grid.find({cell.first.first + dx, cell.first.second + dy});
          if (other == grid.end()) {
            continue;
          }
          for (const int a : cell.second) {
            for (const int b : other->second) {
              if (a < b &&
                  Distance2d(points_[ids[a]], points_[ids[b]]) <= tolerance) {
                sets.Union(a, b);
              }
            }
          }
        }
      }
    }
    int merged = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
      const int root = sets.Find(static_cast<int>(i));
      if (root != static_cast<int>(i)) {
        canonical_[ids[i]] = ids[root];
        ++merged;
      }
    }
    report_->Count("nodes_merged", merged);
  }
  return true;
}

void Lanelet2Converter::BuildLineStrings(const OsmFile& osm) {
  for (const auto& kv : osm.ways) {
    LineString line;
    line.id = kv.first;
    line.tags = &kv.second.tags;
    bool missing = false;
    for (OsmId node_id : kv.second.node_ids) {
      const auto canonical = canonical_.find(node_id);
      if (canonical != canonical_.end()) {
        node_id = canonical->second;
      }
      const auto point = points_.find(node_id);
      if (point == points_.end()) {
        missing = true;
        continue;
      }
      if (!line.node_ids.empty() &&
          (line.node_ids.back() == node_id ||
           Distance2d(line.points.back(), point->second) < 1e-6)) {
        continue;
      }
      line.node_ids.push_back(node_id);
      line.points.push_back(point->second);
    }
    if (missing) {
      report_->Warn("way_with_missing_node",
                    "way " + std::to_string(kv.first) +
                        " references nodes without coordinates");
    }
    line_strings_[kv.first] = std::move(line);
  }
}

const Lanelet2Converter::LineString* Lanelet2Converter::FindLineString(
    OsmId id) const {
  const auto it = line_strings_.find(id);
  return it != line_strings_.end() && it->second.points.size() >= 2
             ? &it->second
             : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Lanelets

void Lanelet2Converter::BuildRegulatoryElements(const OsmFile& osm) {
  for (const auto& kv : osm.relations) {
    const OsmRelation& relation = kv.second;
    if (TagOr(relation.tags, "type", EmptyString()) != "regulatory_element") {
      continue;
    }
    RegulatoryElement element;
    element.id = kv.first;
    element.relation = &relation;
    element.subtype = TagOr(relation.tags, "subtype", EmptyString());
    element.fallback = TagBool(relation.tags, "fallback", false);
    for (const OsmMember& member : relation.members) {
      if (member.type == OsmMember::Type::WAY) {
        if (member.role == "refers") {
          element.refers.push_back(member.ref);
        } else if (member.role == "ref_line") {
          element.ref_lines.push_back(member.ref);
        }
      } else if (member.type == OsmMember::Type::RELATION) {
        if (member.role == "yield") {
          element.yield.push_back(member.ref);
        } else if (member.role == "right_of_way") {
          element.right_of_way.push_back(member.ref);
        }
      }
    }
    report_->Count("regulatory_elements");
    regulatory_elements_[kv.first] = std::move(element);
  }
}

bool Lanelet2Converter::MakeBound(const OsmRelation& relation,
                                  const std::string& role, Bound* bound) {
  const std::string name = "lanelet " + std::to_string(relation.id);
  const OsmMember* found = nullptr;
  int count = 0;
  for (const OsmMember& member : relation.members) {
    if (member.role == role) {
      ++count;
      found = &member;
    }
  }
  if (count != 1) {
    report_->Warn("lanelet_skipped", name + " has " + std::to_string(count) +
                                         " " + role + " bounds instead of one");
    return false;
  }
  if (found->type != OsmMember::Type::WAY) {
    report_->Warn("lanelet_skipped",
                  name + " has a " + role + " bound that is not a way");
    return false;
  }
  const LineString* line = FindLineString(found->ref);
  if (line == nullptr) {
    report_->Warn("lanelet_skipped",
                  name + ": " + role + " bound " + std::to_string(found->ref) +
                      " is missing or has fewer than two points");
    return false;
  }
  bound->way_id = line->id;
  bound->inverted = false;
  bound->node_ids = line->node_ids;
  bound->points = line->points;
  bound->tags = line->tags;
  return true;
}

void Lanelet2Converter::BuildLanelets(const OsmFile& osm) {
  auto invert = [](Bound* bound) {
    std::reverse(bound->node_ids.begin(), bound->node_ids.end());
    std::reverse(bound->points.begin(), bound->points.end());
    bound->inverted = !bound->inverted;
  };

  for (const auto& kv : osm.relations) {
    const OsmRelation& relation = kv.second;
    const std::string& type = TagOr(relation.tags, "type", EmptyString());
    if (type == "regulatory_element") {
      continue;
    }
    if (type == "multipolygon") {
      report_->Count("areas_ignored");
      continue;
    }
    if (type != "lanelet") {
      report_->UnknownTag("relation type=" + type);
      continue;
    }
    report_->Count("lanelets");

    Lanelet lanelet;
    lanelet.id = kv.first;
    lanelet.relation = &relation;
    if (!MakeBound(relation, "left", &lanelet.left) ||
        !MakeBound(relation, "right", &lanelet.right)) {
      continue;
    }
    if (lanelet.left.way_id == lanelet.right.way_id) {
      report_->Warn("lanelet_skipped",
                    "lanelet " + std::to_string(kv.first) +
                        " uses the same way as left and right bound");
      continue;
    }
    // lanelet::geometry::align(): a bound stored against the driving
    // direction is inverted so that the right bound is right of the left.
    if (!(SignedDistance(lanelet.left.points,
                         MiddlePoint(lanelet.right.points)) < 0.0)) {
      invert(&lanelet.left);
    }
    if (!(SignedDistance(lanelet.right.points,
                         MiddlePoint(lanelet.left.points)) > 0.0)) {
      invert(&lanelet.right);
    }

    for (const OsmMember& member : relation.members) {
      if (member.role == "centerline" && member.type == OsmMember::Type::WAY) {
        const LineString* line = FindLineString(member.ref);
        if (line == nullptr) {
          report_->Warn("centerline_ignored",
                        "lanelet " + std::to_string(kv.first) +
                            ": centerline way is missing or degenerate");
        } else {
          lanelet.centerline = line->points;
        }
      } else if (member.role == "regulatory_element" &&
                 member.type == OsmMember::Type::RELATION) {
        lanelet.regulatory_elements.push_back(member.ref);
      }
    }

    lanelet.kind = Classify(relation.tags);
    const int index = static_cast<int>(lanelets_.size());
    for (const OsmId element_id : lanelet.regulatory_elements) {
      const auto element = regulatory_elements_.find(element_id);
      if (element == regulatory_elements_.end()) {
        report_->Warn("missing_regulatory_element",
                      "lanelet " + std::to_string(kv.first) +
                          " references the unknown regulatory element " +
                          std::to_string(element_id));
      } else {
        element->second.lanelets.push_back(index);
      }
    }
    lanelet_index_[kv.first] = index;
    lanelets_.push_back(std::move(lanelet));
  }
}

bool Lanelet2Converter::CanPass(const OsmTags& tags,
                                const std::string& participant) const {
  // Explicit participant:* tags replace what the subtype implies.
  if (HasOverride(tags, "participant")) {
    const std::string* value = GetOverride(tags, "participant:", participant);
    return value != nullptr && ParseBool(*value, false);
  }
  static const std::map<std::string, std::vector<std::string>> kParticipants = {
      {"", {"vehicle"}},
      {"road", {"vehicle", "bicycle"}},
      {"highway", {"vehicle"}},
      {"play_street", {"pedestrian", "bicycle", "vehicle"}},
      {"emergency_lane", {"vehicle:emergency"}},
      {"bus_lane", {"vehicle:bus", "vehicle:emergency", "vehicle:taxi"}},
      {"bicycle_lane", {"bicycle"}},
      {"exit", {"pedestrian", "bicycle", "vehicle"}},
      {"walkway", {"pedestrian"}},
      {"shared_walkway", {"pedestrian", "bicycle"}},
      {"crosswalk", {"pedestrian"}},
      {"stairs", {"pedestrian"}},
  };
  const auto it = kParticipants.find(TagOr(tags, "subtype", EmptyString()));
  if (it == kParticipants.end()) {
    return false;
  }
  for (const std::string& allowed : it->second) {
    if (StartsWith(participant, allowed)) {
      return true;
    }
  }
  return false;
}

bool Lanelet2Converter::IsOneWay(const OsmTags& tags,
                                 const std::string& participant) const {
  const auto plain = tags.find("one_way");
  if (plain != tags.end()) {
    return ParseBool(plain->second, true);
  }
  if (const std::string* value = GetOverride(tags, "one_way:", participant)) {
    return ParseBool(*value, true);
  }
  // Pedestrians may walk both ways unless told otherwise.
  return !StartsWith(participant, "pedestrian");
}

Lanelet2Converter::LaneletKind Lanelet2Converter::Classify(
    const OsmTags& tags) {
  const std::string& subtype = TagOr(tags, "subtype", EmptyString());
  if (CanPass(tags, options_.participant)) {
    return LaneletKind::DRIVING;
  }
  if (subtype == "crosswalk") {
    return LaneletKind::CROSSWALK;
  }
  if (subtype == "road_shoulder" || subtype == "emergency_lane") {
    return LaneletKind::SHOULDER;
  }
  if (CanPass(tags, "pedestrian")) {
    return LaneletKind::SIDEWALK;
  }
  if (CanPass(tags, "bicycle")) {
    return LaneletKind::BIKING;
  }
  static const std::set<std::string> kKnown = {
      "",          "road",         "highway",        "play_street",
      "bus_lane",  "exit",         "bicycle_lane",   "walkway",
      "stairs",    "rail",         "shared_walkway", "emergency_lane",
      "crosswalk", "road_shoulder"};
  if (kKnown.count(subtype) == 0) {
    report_->UnknownTag("lanelet subtype=" + subtype);
  }
  return LaneletKind::IGNORED;
}

double Lanelet2Converter::SpeedLimit(const Lanelet& lanelet, LaneletKind kind) {
  const OsmTags& tags = lanelet.relation->tags;
  const std::string name = "lanelet " + std::to_string(lanelet.id);

  // 1. speed_limit regulatory elements (the lowest one wins)
  double best = -1.0;
  for (const OsmId element_id : lanelet.regulatory_elements) {
    const auto it = regulatory_elements_.find(element_id);
    if (it == regulatory_elements_.end() ||
        it->second.subtype != "speed_limit" || it->second.fallback) {
      continue;
    }
    std::string sign;
    for (const OsmId way_id : it->second.refers) {
      const auto way = line_strings_.find(way_id);
      if (way != line_strings_.end()) {
        sign = TagOr(*way->second.tags, "subtype", EmptyString());
        if (!sign.empty()) {
          break;
        }
      }
    }
    if (sign.empty()) {
      sign = TagOr(it->second.relation->tags, "sign_type", EmptyString());
    }
    double mps = 0.0;
    if (SignVelocity(sign, &mps)) {
      best = best < 0.0 ? mps : std::min(best, mps);
    } else {
      report_->Warn("speed_limit_not_understood",
                    name + ": speed limit sign \"" + sign +
                        "\" of regulatory element " +
                        std::to_string(element_id));
    }
  }
  if (best > 0.0) {
    return best;
  }

  // 2. speed_limit tag of the lanelet
  const std::string* text = nullptr;
  const auto plain = tags.find("speed_limit");
  if (plain != tags.end()) {
    text = &plain->second;
  } else {
    text = GetOverride(tags, "speed_limit:", options_.participant);
  }
  if (text != nullptr) {
    double mps = 0.0;
    if (ParseVelocity(*text, &mps)) {
      return mps;
    }
    report_->Warn("speed_limit_not_understood",
                  name + ": speed_limit=\"" + *text + "\"");
  }

  // 3. defaults
  switch (kind) {
    case LaneletKind::BIKING:
      return 20.0 / 3.6;
    case LaneletKind::SIDEWALK:
      return 5.0 / 3.6;
    default:
      break;
  }
  const std::string& subtype = TagOr(tags, "subtype", EmptyString());
  const std::string& location = TagOr(tags, "location", EmptyString());
  if (subtype == "play_street") {
    return 7.0 / 3.6;
  }
  if (location == "private") {
    return 20.0 / 3.6;
  }
  if (subtype == "highway") {
    return options_.default_speed_highway;
  }
  if (location == "nonurban") {
    return options_.default_speed_nonurban;
  }
  if (!location.empty() && location != "urban") {
    report_->UnknownTag("lanelet location=" + location);
  }
  return options_.default_speed_urban;
}

Lanelet2Converter::BoundaryStyle Lanelet2Converter::ClassifyBound(
    const Bound& bound, bool is_left) {
  const OsmTags& tags = *bound.tags;
  const std::string& type = TagOr(tags, "type", EmptyString());
  const std::string& subtype = TagOr(tags, "subtype", EmptyString());
  const bool line = type == "line_thin" || type == "line_thick";

  // GenericTrafficRules::laneChangeType() for vehicles.
  enum { kNone, kBoth, kToLeft, kToRight } change = kNone;
  if (tags.count("lane_change") > 0) {
    change = TagBool(tags, "lane_change", false) ? kBoth : kNone;
  } else if (TagBool(tags, "lane_change:left", false)) {
    change = TagBool(tags, "lane_change:right", false) ? kBoth : kToLeft;
  } else if (tags.count("lane_change:right") > 0) {
    change = TagBool(tags, "lane_change:right", false) ? kToRight : kNone;
  } else if (line) {
    if (subtype == "dashed") {
      change = kBoth;
    } else if (subtype == "dashed_solid") {
      change = kToRight;
    } else if (subtype == "solid_dashed") {
      change = kToLeft;
    }
  }
  if (bound.inverted) {
    change = change == kToLeft    ? kToRight
             : change == kToRight ? kToLeft
                                  : change;
  }
  const bool crossable =
      change == kBoth || change == (is_left ? kToLeft : kToRight);

  const bool yellow = TagOr(tags, "color", EmptyString()) == "yellow";
  static const std::set<std::string> kBarriers = {
      "curbstone", "road_border", "guard_rail",     "wall", "fence",
      "gate",      "lift_gate",   "jersey_barrier", "door", "rail"};
  static const std::set<std::string> kMarkings = {
      "zebra_marking", "pedestrian_marking",
      "bike_marking",  "keepout",
      "stop_line",     "zig-zag",
      "bump",          "symbol",
      "arrow",         "trajectory",
      "visualization"};

  BoundaryStyle style;
  if (line) {
    static const std::set<std::string> kSubtypes = {
        "", "solid", "dashed", "dashed_solid", "solid_dashed", "solid_solid"};
    if (kSubtypes.count(subtype) == 0) {
      report_->UnknownTag("way type=" + type + " subtype=" + subtype);
    }
    if (crossable) {
      style.type = yellow ? LaneBoundaryType::DOTTED_YELLOW
                          : LaneBoundaryType::DOTTED_WHITE;
    } else if (subtype == "solid_solid") {
      style.type = LaneBoundaryType::DOUBLE_YELLOW;
    } else {
      style.type = yellow ? LaneBoundaryType::SOLID_YELLOW
                          : LaneBoundaryType::SOLID_WHITE;
    }
  } else if (kBarriers.count(type) > 0) {
    style.type = LaneBoundaryType::CURB;
  } else if (type == "virtual") {
    style.is_virtual = true;
    style.type =
        crossable ? LaneBoundaryType::DOTTED_WHITE : LaneBoundaryType::UNKNOWN;
  } else if (kMarkings.count(type) > 0) {
    style.type = crossable ? LaneBoundaryType::DOTTED_WHITE
                           : LaneBoundaryType::SOLID_WHITE;
  } else {
    report_->UnknownTag("way type=" + type);
    style.type =
        crossable ? LaneBoundaryType::DOTTED_WHITE : LaneBoundaryType::UNKNOWN;
  }
  return style;
}

///////////////////////////////////////////////////////////////////////////////
// Lanes

std::string Lanelet2Converter::UniqueId(const OsmTags* tags,
                                        const std::string& fallback) {
  std::string id = fallback;
  bool from_tag = false;
  if (options_.use_apollo_tags && tags != nullptr) {
    const auto it = tags->find(kApolloIdTag);
    if (it != tags->end() && !it->second.empty() &&
        apollo_id_uses_[it->second] == 1) {
      id = it->second;
      from_tag = true;
    }
  }
  // Generated ids give way to apollo:id tags; everything stays unique.
  while (used_ids_.count(id) > 0 ||
         (!from_tag && apollo_id_uses_.count(id) > 0 &&
          apollo_id_uses_[id] == 1)) {
    id += "_";
  }
  used_ids_.insert(id);
  return id;
}

bool Lanelet2Converter::FinishLaneGeometry(LaneData* lane) const {
  RemoveDuplicatePoints(&lane->center);
  if (lane->center.size() < 2) {
    return false;
  }
  lane->center_s = AccumulatedLength(lane->center);
  lane->length = lane->center_s.back();
  if (!(lane->length > 1e-3) || !std::isfinite(lane->length)) {
    return false;
  }
  const std::vector<double> left_s = AccumulatedLength(lane->left.points);
  const std::vector<double> right_s = AccumulatedLength(lane->right.points);
  const size_t n = lane->center.size();
  lane->left_width.resize(n);
  lane->right_width.resize(n);
  lane->left_foot.resize(n);
  lane->right_foot.resize(n);
  for (size_t i = 0; i < n; ++i) {
    lane->left_width[i] =
        ProjectToPolyline(lane->left.points, left_s, lane->center[i], nullptr,
                          &lane->left_foot[i]);
    lane->right_width[i] =
        ProjectToPolyline(lane->right.points, right_s, lane->center[i], nullptr,
                          &lane->right_foot[i]);
  }
  lane->polygon = lane->left.points;
  lane->polygon.insert(lane->polygon.end(), lane->right.points.rbegin(),
                       lane->right.points.rend());
  lane->box = BoundingBox();
  lane->box.Add(lane->polygon);
  lane->box.Add(lane->center);
  return true;
}

int Lanelet2Converter::AddLane(int lanelet_index, bool inverted,
                               Lane::LaneType type, const std::string& id) {
  const Lanelet& lanelet = lanelets_[lanelet_index];
  LaneData lane;
  lane.id = id;
  lane.lanelet = lanelet_index;
  lane.inverted = inverted;
  lane.type = type;
  lane.left = lanelet.left;
  lane.right = lanelet.right;
  if (inverted) {
    std::swap(lane.left, lane.right);
    for (Bound* bound : {&lane.left, &lane.right}) {
      std::reverse(bound->node_ids.begin(), bound->node_ids.end());
      std::reverse(bound->points.begin(), bound->points.end());
      bound->inverted = !bound->inverted;
    }
  }

  if (!lanelet.centerline.empty()) {
    // An explicit centerline: make it run in the driving direction and
    // densify it; its vertices are kept.
    Polyline center = lanelet.centerline;
    const Point3 start =
        Middle(lanelet.left.points.front(), lanelet.right.points.front());
    if (Distance2d(center.front(), start) > Distance2d(center.back(), start)) {
      std::reverse(center.begin(), center.end());
    }
    lane.center = Densify(center, options_.sample_distance);
  } else {
    // Both bounds resampled at the same normalized arc length, averaged.
    const std::vector<double> left_s = AccumulatedLength(lanelet.left.points);
    const std::vector<double> right_s = AccumulatedLength(lanelet.right.points);
    const double longest = std::max(left_s.back(), right_s.back());
    const double spacing = std::max(options_.sample_distance, 0.05);
    // (the tolerance keeps a 60 m lane at 121 points although projected
    // lengths are only exact to a micrometer)
    const int n =
        std::max(2, static_cast<int>(std::ceil(longest / spacing - 1e-6)) + 1);
    lane.center.reserve(n);
    for (int i = 0; i < n; ++i) {
      const double t = static_cast<double>(i) / (n - 1);
      lane.center.push_back(
          Middle(PointAt(lanelet.left.points, left_s, t * left_s.back()),
                 PointAt(lanelet.right.points, right_s, t * right_s.back())));
    }
  }
  if (inverted) {
    std::reverse(lane.center.begin(), lane.center.end());
  }
  if (!FinishLaneGeometry(&lane)) {
    return -1;
  }
  lane.left_style = ClassifyBound(lane.left, true);
  lane.right_style = ClassifyBound(lane.right, false);

  const std::string& turn =
      TagOr(lanelet.relation->tags, "turn_direction", EmptyString());
  if (!turn.empty()) {
    lane.has_turn_tag = true;
    if (turn == "left") {
      lane.turn = inverted ? Lane::RIGHT_TURN : Lane::LEFT_TURN;
    } else if (turn == "right") {
      lane.turn = inverted ? Lane::LEFT_TURN : Lane::RIGHT_TURN;
    } else if (turn == "straight") {
      lane.turn = Lane::NO_TURN;
    } else {
      lane.has_turn_tag = false;
      report_->UnknownTag("lanelet turn_direction=" + turn);
    }
  }
  // Apollo's turn type where turn_direction cannot express it (u-turns).
  if (options_.use_apollo_tags) {
    Lane::LaneTurn apollo_turn = Lane::NO_TURN;
    if (Lane::LaneTurn_Parse(
            TagOr(lanelet.relation->tags, kApolloTurnTag, EmptyString()),
            &apollo_turn)) {
      lane.has_turn_tag = true;
      lane.turn_is_final = true;
      lane.turn = apollo_turn;
      if (inverted && apollo_turn == Lane::LEFT_TURN) {
        lane.turn = Lane::RIGHT_TURN;
      } else if (inverted && apollo_turn == Lane::RIGHT_TURN) {
        lane.turn = Lane::LEFT_TURN;
      }
    }
  }
  lanes_.push_back(std::move(lane));
  return static_cast<int>(lanes_.size()) - 1;
}

void Lanelet2Converter::BuildLanes() {
  for (size_t i = 0; i < lanelets_.size(); ++i) {
    Lanelet& lanelet = lanelets_[i];
    const OsmTags& tags = lanelet.relation->tags;
    const std::string numeric_id = std::to_string(lanelet.id);

    Lane::LaneType type = Lane::CITY_DRIVING;
    std::string participant = options_.participant;
    switch (lanelet.kind) {
      case LaneletKind::DRIVING:
        break;
      case LaneletKind::BIKING:
        type = Lane::BIKING;
        participant = "bicycle";
        break;
      case LaneletKind::SIDEWALK:
        type = Lane::SIDEWALK;
        participant = "pedestrian";
        break;
      case LaneletKind::SHOULDER:
        type = Lane::SHOULDER;
        break;
      case LaneletKind::CROSSWALK:
        continue;  // becomes a Crosswalk, see BuildCrosswalks()
      case LaneletKind::IGNORED:
        report_->Count("lanelets_ignored");
        report_->Count("lanelets_ignored_subtype_" +
                       TagOr(tags, "subtype", EmptyString()));
        continue;
    }
    if (lanelet.kind != LaneletKind::DRIVING && !options_.non_driving_lanes) {
      report_->Count("lanelets_ignored");
      continue;
    }
    lanelet.one_way = IsOneWay(tags, participant);

    const std::string id = UniqueId(&tags, numeric_id);
    lanelet.forward_lane = AddLane(static_cast<int>(i), false, type, id);
    if (lanelet.forward_lane < 0) {
      report_->Warn("lanelet_skipped",
                    "lanelet " + numeric_id + " has a degenerate geometry");
      continue;
    }
    lanes_[lanelet.forward_lane].speed_limit =
        SpeedLimit(lanelet, lanelet.kind);

    if (!lanelet.one_way) {
      if (lanelet.kind == LaneletKind::DRIVING &&
          options_.bidirectional_lanes) {
        lanelet.inverted_lane = AddLane(static_cast<int>(i), true, type,
                                        UniqueId(nullptr, id + "_inv"));
        if (lanelet.inverted_lane >= 0) {
          LaneData& inverted = lanes_[lanelet.inverted_lane];
          inverted.speed_limit = lanes_[lanelet.forward_lane].speed_limit;
          inverted.self_reverse = lanelet.forward_lane;
          lanes_[lanelet.forward_lane].self_reverse = lanelet.inverted_lane;
          report_->Count("lanes_inverted");
        }
      } else {
        lanes_[lanelet.forward_lane].direction = Lane::BIDIRECTION;
      }
    }
  }
  for (const LaneData& lane : lanes_) {
    report_->Count("lanes");
    report_->Count("lanes_" + ToLower(Lane::LaneType_Name(lane.type)));
  }
}

void Lanelet2Converter::BuildTopology() {
  using Ids = std::vector<OsmId>;
  std::map<std::pair<OsmId, OsmId>, std::vector<int>> by_start;
  std::map<Ids, std::vector<int>> by_left;
  std::map<Ids, std::vector<int>> by_right;
  std::map<std::pair<Ids, Ids>, int> by_bounds;
  for (size_t i = 0; i < lanes_.size(); ++i) {
    const LaneData& lane = lanes_[i];
    by_start[{lane.left.node_ids.front(), lane.right.node_ids.front()}]
        .push_back(static_cast<int>(i));
    by_left[lane.left.node_ids].push_back(static_cast<int>(i));
    by_right[lane.right.node_ids].push_back(static_cast<int>(i));
    by_bounds.emplace(std::make_pair(lane.left.node_ids, lane.right.node_ids),
                      static_cast<int>(i));
  }
  auto dotted = [](const BoundaryStyle& style) {
    return style.type == LaneBoundaryType::DOTTED_WHITE ||
           style.type == LaneBoundaryType::DOTTED_YELLOW;
  };
  // Lanes of different kinds (a road next to a bicycle lane or a walkway)
  // are neighbors only across a bound that may be crossed: planning borrows
  // neighbor lanes and must never do so over a curb or a solid line.
  auto collect = [this](const std::map<Ids, std::vector<int>>& index,
                        const Ids& key, int self, bool crossable,
                        std::vector<int>* out) {
    const auto it = index.find(key);
    if (it == index.end()) {
      return;
    }
    for (const int other : it->second) {
      if (other != self && lanes_[other].lanelet != lanes_[self].lanelet &&
          (lanes_[other].type == lanes_[self].type || crossable)) {
        out->push_back(other);
      }
    }
  };

  for (size_t i = 0; i < lanes_.size(); ++i) {
    LaneData& lane = lanes_[i];
    const int self = static_cast<int>(i);
    // lanelet::geometry::follows(): the end points of both bounds are the
    // start points of the next lanelet.
    const auto next =
        by_start.find({lane.left.node_ids.back(), lane.right.node_ids.back()});
    if (next != by_start.end()) {
      for (const int other : next->second) {
        if (other != self && lanes_[other].type == lane.type &&
            lanes_[other].lanelet != lane.lanelet) {
          lane.successors.push_back(other);
        }
      }
    }
    // A shared bound in the same direction: neighbors driving the same way.
    const bool left_crossable = dotted(lane.left_style);
    const bool right_crossable = dotted(lane.right_style);
    collect(by_right, lane.left.node_ids, self, left_crossable,
            &lane.left_forward);
    collect(by_left, lane.right.node_ids, self, right_crossable,
            &lane.right_forward);
    // A shared bound in opposite directions: oncoming neighbors.
    const Ids left_reversed(lane.left.node_ids.rbegin(),
                            lane.left.node_ids.rend());
    const Ids right_reversed(lane.right.node_ids.rbegin(),
                             lane.right.node_ids.rend());
    collect(by_left, left_reversed, self, left_crossable, &lane.left_reverse);
    collect(by_right, right_reversed, self, right_crossable,
            &lane.right_reverse);
    // Two one-way lanelets on the same two bounds, driven in opposite
    // directions, are one two-way lane as well.
    if (lane.self_reverse < 0) {
      const auto twin =
          by_bounds.find(std::make_pair(right_reversed, left_reversed));
      if (twin != by_bounds.end() && twin->second != self &&
          lanes_[twin->second].type == lane.type) {
        lane.self_reverse = twin->second;
      }
    }
  }
  for (size_t i = 0; i < lanes_.size(); ++i) {
    for (const int successor : lanes_[i].successors) {
      lanes_[successor].predecessors.push_back(static_cast<int>(i));
    }
  }
  for (LaneData& lane : lanes_) {
    SortUnique(&lane.successors);
    SortUnique(&lane.predecessors);
    SortUnique(&lane.left_forward);
    SortUnique(&lane.right_forward);
    SortUnique(&lane.left_reverse);
    SortUnique(&lane.right_reverse);
  }
}

void Lanelet2Converter::BuildRoads() {
  UnionFind sets(lanes_.size());
  for (size_t i = 0; i < lanes_.size(); ++i) {
    for (const int other : lanes_[i].left_forward) {
      sets.Union(static_cast<int>(i), other);
    }
    for (const int other : lanes_[i].right_forward) {
      sets.Union(static_cast<int>(i), other);
    }
  }
  std::map<int, std::vector<int>> groups;
  for (size_t i = 0; i < lanes_.size(); ++i) {
    groups[sets.Find(static_cast<int>(i))].push_back(static_cast<int>(i));
  }
  for (const auto& group : groups) {
    // Order the lanes from left to right.
    const std::set<int> members(group.second.begin(), group.second.end());
    std::vector<int> ordered;
    std::set<int> done;
    std::vector<int> frontier;
    for (const int lane : group.second) {
      if (lanes_[lane].left_forward.empty()) {
        frontier.push_back(lane);
      }
    }
    if (frontier.empty()) {
      frontier.push_back(group.second.front());  // a ring of neighbors
    }
    while (!frontier.empty()) {
      std::vector<int> next;
      for (const int lane : frontier) {
        if (!done.insert(lane).second) {
          continue;
        }
        ordered.push_back(lane);
        for (const int right : lanes_[lane].right_forward) {
          if (members.count(right) > 0 && done.count(right) == 0) {
            next.push_back(right);
          }
        }
      }
      frontier.swap(next);
    }
    for (const int lane : group.second) {
      if (done.insert(lane).second) {
        ordered.push_back(lane);
      }
    }

    RoadData road;
    road.id = UniqueId(nullptr, "road_" + lanes_[ordered.front()].id);
    road.lanes = ordered;
    for (const int lane : ordered) {
      lanes_[lane].road = static_cast<int>(roads_.size());
      const OsmTags& tags = lanelets_[lanes_[lane].lanelet].relation->tags;
      road.highway |= TagOr(tags, "subtype", EmptyString()) == "highway";
    }

    // Distance of every centerline point to the edges of the road.
    const Polyline& left_edge = lanes_[ordered.front()].left.points;
    const Polyline& right_edge = lanes_[ordered.back()].right.points;
    const std::vector<double> left_s = AccumulatedLength(left_edge);
    const std::vector<double> right_s = AccumulatedLength(right_edge);
    for (const int index : ordered) {
      LaneData& lane = lanes_[index];
      if (ordered.size() == 1) {
        lane.left_road_width = lane.left_width;
        lane.right_road_width = lane.right_width;
        continue;
      }
      lane.left_road_width.resize(lane.center.size());
      lane.right_road_width.resize(lane.center.size());
      for (size_t i = 0; i < lane.center.size(); ++i) {
        lane.left_road_width[i] =
            std::max(lane.left_width[i],
                     ProjectToPolyline(left_edge, left_s, lane.center[i],
                                       nullptr, nullptr));
        lane.right_road_width[i] =
            std::max(lane.right_width[i],
                     ProjectToPolyline(right_edge, right_s, lane.center[i],
                                       nullptr, nullptr));
      }
    }
    roads_.push_back(std::move(road));
  }
  report_->Count("roads", static_cast<int>(roads_.size()));
}

///////////////////////////////////////////////////////////////////////////////
// Overlaps

std::string Lanelet2Converter::NextOverlapId() {
  return "overlap_" + std::to_string(overlaps_.size() + 1);
}

void Lanelet2Converter::SetObjectInfo(ObjectKind kind,
                                      ObjectOverlapInfo* info) {
  switch (kind) {
    case kSignal:
      info->mutable_signal_overlap_info();
      break;
    case kStopSign:
      info->mutable_stop_sign_overlap_info();
      break;
    case kYieldSign:
      info->mutable_yield_sign_overlap_info();
      break;
    case kCrosswalk:
      info->mutable_crosswalk_overlap_info();
      break;
    case kJunction:
      info->mutable_junction_overlap_info();
      break;
  }
}

void Lanelet2Converter::AddObjectOverlapId(const std::string& object_id,
                                           ObjectKind kind,
                                           const std::string& overlap_id) {
  switch (kind) {
    case kSignal:
      signals_[object_id].add_overlap_id()->set_id(overlap_id);
      break;
    case kStopSign:
      stop_signs_[object_id].add_overlap_id()->set_id(overlap_id);
      break;
    case kYieldSign:
      yield_signs_[object_id].add_overlap_id()->set_id(overlap_id);
      break;
    case kCrosswalk:
      crosswalks_[object_id].add_overlap_id()->set_id(overlap_id);
      break;
    case kJunction:
      for (JunctionData& junction : junctions_) {
        if (junction.id == object_id) {
          junction.overlap_ids.push_back(overlap_id);
        }
      }
      break;
  }
}

void Lanelet2Converter::AddLaneObjectOverlap(int lane, double start_s,
                                             double end_s,
                                             const std::string& object_id,
                                             ObjectKind kind) {
  if (!lane_object_overlaps_.insert({lane, object_id}).second) {
    return;
  }
  Overlap overlap;
  const std::string id = NextOverlapId();
  overlap.mutable_id()->set_id(id);
  ObjectOverlapInfo* lane_info = overlap.add_object();
  lane_info->mutable_id()->set_id(lanes_[lane].id);
  lane_info->mutable_lane_overlap_info()->set_start_s(start_s);
  lane_info->mutable_lane_overlap_info()->set_end_s(end_s);
  lane_info->mutable_lane_overlap_info()->set_is_merge(false);
  ObjectOverlapInfo* object_info = overlap.add_object();
  object_info->mutable_id()->set_id(object_id);
  SetObjectInfo(kind, object_info);
  lanes_[lane].overlap_ids.push_back(id);
  AddObjectOverlapId(object_id, kind, id);
  overlaps_.push_back(std::move(overlap));
  report_->Count("overlaps");
}

void Lanelet2Converter::BuildCrosswalks() {
  for (const Lanelet& lanelet : lanelets_) {
    if (lanelet.kind != LaneletKind::CROSSWALK) {
      continue;
    }
    std::vector<Point3> polygon = lanelet.left.points;
    polygon.insert(polygon.end(), lanelet.right.points.rbegin(),
                   lanelet.right.points.rend());
    RemoveDuplicatePoints(&polygon);
    if (polygon.size() >= 2 &&
        Distance2d(polygon.front(), polygon.back()) < 1e-6) {
      polygon.pop_back();
    }
    if (polygon.size() < 3 || std::fabs(SignedArea(polygon)) < 1e-2) {
      report_->Warn(
          "crosswalk_skipped",
          "crosswalk lanelet " + std::to_string(lanelet.id) + " has no area");
      continue;
    }
    const std::string id = UniqueId(&lanelet.relation->tags,
                                    "crosswalk_" + std::to_string(lanelet.id));
    Crosswalk& crosswalk = crosswalks_[id];
    crosswalk.mutable_id()->set_id(id);
    for (const Point3& point : polygon) {
      SetPoint(point, crosswalk.mutable_polygon()->add_point());
    }
    crosswalk_polygons_[id] = polygon;
    report_->Count("crosswalks");

    BoundingBox box;
    box.Add(polygon);
    for (size_t i = 0; i < lanes_.size(); ++i) {
      const LaneData& lane = lanes_[i];
      if (lane.type == Lane::SIDEWALK || !lane.box.Overlaps(box)) {
        continue;
      }
      // The lane is on the crosswalk wherever its cross section touches the
      // polygon: a vehicle reaches a skewed crosswalk with one side first.
      double begin = 0.0;
      double end = 0.0;
      if (!CrossSectionRange(lane, polygon, &begin, &end)) {
        // a polygon thinner than the sample spacing: exact along the
        // centerline
        const auto ranges =
            RangesInsidePolygon(lane.center, lane.center_s, polygon);
        if (ranges.empty()) {
          continue;
        }
        begin = ranges.front().first;
        end = ranges.back().second;
      }
      if (end - begin < kMinOverlapLength) {
        continue;
      }
      AddLaneObjectOverlap(static_cast<int>(i), begin, end, id, kCrosswalk);
    }
  }
}

bool Lanelet2Converter::CrossSectionRange(const LaneData& lane,
                                          const std::vector<Point3>& polygon,
                                          double* begin, double* end) const {
  auto lerp = [](const Point3& a, const Point3& b, double t) {
    Point3 p;
    p.x = a.x + (b.x - a.x) * t;
    p.y = a.y + (b.y - a.y) * t;
    return p;
  };
  // Cross section between the samples i and i + 1.
  auto touches = [&](size_t i, double t) {
    if (i + 1 >= lane.center.size()) {
      return SegmentTouchesPolygon(lane.left_foot[i], lane.right_foot[i],
                                   polygon);
    }
    return SegmentTouchesPolygon(
        lerp(lane.left_foot[i], lane.left_foot[i + 1], t),
        lerp(lane.right_foot[i], lane.right_foot[i + 1], t), polygon);
  };
  // First position in (i, i + 1] (forward) or last in [i, i + 1) that
  // touches, by bisection.
  auto refine = [&](size_t i, bool forward) {
    double outside = forward ? 0.0 : 1.0;
    double inside = forward ? 1.0 : 0.0;
    for (int k = 0; k < 6; ++k) {
      const double middle = 0.5 * (outside + inside);
      (touches(i, middle) ? inside : outside) = middle;
    }
    return lane.center_s[i] +
           (lane.center_s[i + 1] - lane.center_s[i]) * inside;
  };

  const size_t n = lane.center.size();
  size_t first = n;
  size_t last = n;
  for (size_t i = 0; i < n; ++i) {
    if (touches(i, 0.0)) {
      if (first == n) {
        first = i;
      }
      last = i;
    }
  }
  if (first == n) {
    return false;
  }
  *begin = first == 0 ? lane.center_s[0] : refine(first - 1, true);
  *end = last + 1 >= n ? lane.center_s[n - 1] : refine(last, false);
  return true;
}

double Lanelet2Converter::StopPosition(const LaneData& lane,
                                       const std::vector<OsmId>& ref_lines,
                                       OsmId element_id, Polyline* stop_line) {
  // The first stop line the centerline crosses.
  double best_s = -1.0;
  const LineString* best_line = nullptr;
  double nearest = std::numeric_limits<double>::max();
  double nearest_s = 0.0;
  const LineString* nearest_line = nullptr;
  for (const OsmId way_id : ref_lines) {
    const LineString* line = FindLineString(way_id);
    if (line == nullptr) {
      continue;
    }
    const std::vector<double> line_s = AccumulatedLength(line->points);
    const auto hits =
        IntersectPolylines(lane.center, lane.center_s, line->points, line_s);
    if (!hits.empty() && (best_line == nullptr || hits.front().s_a < best_s)) {
      best_s = hits.front().s_a;
      best_line = line;
    }
    // Otherwise the stop line may end next to the centerline or just
    // behind the lane end.
    for (size_t i = 0; i < lane.center.size(); ++i) {
      const double distance = ProjectToPolyline(
          line->points, line_s, lane.center[i], nullptr, nullptr);
      if (distance < nearest) {
        nearest = distance;
        nearest_s = lane.center_s[i];
        nearest_line = line;
      }
    }
  }
  double s = lane.length;
  if (best_line != nullptr) {
    s = best_s;
    *stop_line = best_line->points;
  } else if (nearest_line != nullptr && nearest <= kStopLineReach) {
    s = nearest_s;
    *stop_line = nearest_line->points;
  } else {
    // No stop line: Lanelet2 stops at the end of the lanelet.
    if (!ref_lines.empty()) {
      report_->Warn("stop_line_not_on_lanelet",
                    "regulatory element " + std::to_string(element_id) +
                        ": no stop line reaches lanelet " + lane.id +
                        ", its end is used");
    }
    *stop_line = {lane.left.points.back(), lane.right.points.back()};
    if (Distance2d(stop_line->front(), stop_line->back()) < 0.1) {
      // The lanelet ends in a point: a line across its last direction.
      // (Apollo aborts on signals and signs without a stop line.)
      const Point3& end = lane.center.back();
      const Point3& before = lane.center[lane.center.size() - 2];
      const double norm = Distance2d(before, end);
      Point3 left = end;
      Point3 right = end;
      left.x -= (end.y - before.y) / norm * 1.5;
      left.y += (end.x - before.x) / norm * 1.5;
      right.x += (end.y - before.y) / norm * 1.5;
      right.y -= (end.x - before.x) / norm * 1.5;
      *stop_line = {left, right};
    }
  }
  return std::max(0.0, std::min(s, lane.length - 0.01));
}

void Lanelet2Converter::AppendStopLine(
    const Polyline& line, google::protobuf::RepeatedPtrField<Curve>* out) {
  // The same stop line is reported once per lane; keep one copy.
  for (const Curve& existing : *out) {
    const auto& points = existing.segment(0).line_segment().point();
    if (points.size() == static_cast<int>(line.size()) &&
        std::fabs(points.Get(0).x() - line.front().x) < 1e-6 &&
        std::fabs(points.Get(0).y() - line.front().y) < 1e-6 &&
        std::fabs(points.Get(points.size() - 1).x() - line.back().x) < 1e-6 &&
        std::fabs(points.Get(points.size() - 1).y() - line.back().y) < 1e-6) {
      return;
    }
  }
  FillCurve(line, out->Add());
}

void Lanelet2Converter::BuildTrafficControls() {
  // Lanes a regulatory element applies to. Only the stored direction of a
  // lanelet is controlled; the inverted lane of a two-way lanelet is not.
  auto lanes_of = [this](const std::vector<int>& lanelet_indices) {
    std::vector<int> lanes;
    for (const int index : lanelet_indices) {
      const int lane = lanelets_[index].forward_lane;
      if (lane >= 0 && lanes_[lane].type != Lane::SIDEWALK) {
        lanes.push_back(lane);
      }
    }
    SortUnique(&lanes);
    return lanes;
  };
  auto lanelets_of = [this](const std::vector<OsmId>& ids) {
    std::vector<int> indices;
    for (const OsmId id : ids) {
      const auto it = lanelet_index_.find(id);
      if (it != lanelet_index_.end()) {
        indices.push_back(it->second);
      }
    }
    return indices;
  };
  auto sign_type = [this](const RegulatoryElement& element) {
    for (const OsmId way_id : element.refers) {
      const auto way = line_strings_.find(way_id);
      if (way != line_strings_.end()) {
        const std::string& subtype =
            TagOr(*way->second.tags, "subtype", EmptyString());
        if (!subtype.empty()) {
          return subtype;
        }
      }
    }
    return TagOr(element.relation->tags, "sign_type", EmptyString());
  };
  // A stop or yield sign with its stop lines and lane overlaps.
  auto add_sign = [this](ObjectKind kind, const std::string& id,
                         const std::vector<int>& lanes,
                         const std::vector<OsmId>& ref_lines, OsmId element_id,
                         bool all_way) {
    for (const int lane : lanes) {
      Polyline stop_line;
      const double s =
          StopPosition(lanes_[lane], ref_lines, element_id, &stop_line);
      if (kind == kStopSign) {
        StopSign& sign = stop_signs_[id];
        sign.mutable_id()->set_id(id);
        sign.set_type(all_way ? StopSign::ALL_WAY : StopSign::UNKNOWN);
        AppendStopLine(stop_line, sign.mutable_stop_line());
      } else {
        YieldSign& sign = yield_signs_[id];
        sign.mutable_id()->set_id(id);
        AppendStopLine(stop_line, sign.mutable_stop_line());
      }
      AddLaneObjectOverlap(lane, s, s, id, kind);
      control_points_.push_back({lane, s, id, kind});
    }
  };

  std::map<OsmId, std::string> signal_ids;  // light way -> signal id
  static const std::set<std::string> kNotConverted = {
      "crosswalk",       "detection_area", "no_stopping_area",
      "no_parking_area", "speed_bump",     "virtual_traffic_light",
      "bus_stop_area",   "roundabout"};

  for (const auto& kv : regulatory_elements_) {
    const RegulatoryElement& element = kv.second;
    const std::string name = "regulatory element " + std::to_string(kv.first);
    report_->Count("regulatory_elements_" + element.subtype);
    if (element.fallback) {
      // Only valid while the traffic lights are out of order.
      report_->Count("regulatory_elements_fallback_ignored");
      continue;
    }

    if (element.subtype == "traffic_light") {
      const std::vector<int> lanes = lanes_of(element.lanelets);
      std::vector<const LineString*> lights;
      for (const OsmId way_id : element.refers) {
        if (const LineString* light = FindLineString(way_id)) {
          lights.push_back(light);
        }
      }
      if (lights.empty()) {
        report_->Warn("regulatory_element_skipped",
                      name + " (traffic_light) refers to no traffic light");
        continue;
      }
      if (lanes.empty()) {
        report_->Warn("regulatory_element_unused",
                      name + " (traffic_light) is not used by any lane");
        continue;
      }
      for (const LineString* light : lights) {
        auto id = signal_ids.find(light->id);
        if (id == signal_ids.end()) {
          const std::string signal_id =
              UniqueId(light->tags, "signal_" + std::to_string(light->id));
          id = signal_ids.emplace(light->id, signal_id).first;
          Signal& signal = signals_[signal_id];
          signal.mutable_id()->set_id(signal_id);

          const std::string& subtype =
              TagOr(*light->tags, "subtype", EmptyString());
          int bulbs = 0;
          if (subtype == "red_yellow_green") {
            bulbs = 3;
          } else if (subtype == "red_green" || subtype == "red_yellow") {
            bulbs = 2;
          } else if (subtype == "red" || subtype == "yellow" ||
                     subtype == "white") {
            bulbs = 1;
          } else if (!subtype.empty()) {
            report_->UnknownTag("way type=traffic_light subtype=" + subtype);
          }
          const Point3& left = light->points.front();
          const Point3& right = light->points.back();
          const bool outline =
              light->points.size() >= 4 || TagBool(*light->tags, "area", false);
          double height = 0.0;
          const bool has_height =
              ParseNumber(TagOr(*light->tags, "height", EmptyString()),
                          &height) &&
              height > 0.0;
          const double width = Distance2d(left, right);
          if (!has_height) {
            height =
                bulbs > 1 ? bulbs * std::max(width, 0.3) : kDefaultSignalHeight;
          }
          const bool horizontal = has_height && width > 1.5 * height;
          if (outline) {
            for (const Point3& point : light->points) {
              auto* corner = signal.mutable_boundary()->add_point();
              corner->set_x(point.x);
              corner->set_y(point.y);
              corner->set_z(point.z);
            }
          } else {
            // The way is the lower edge of the light, `height` its size.
            const Point3 corners[4] = {{left.x, left.y, left.z + height},
                                       {right.x, right.y, right.z + height},
                                       {right.x, right.y, right.z},
                                       {left.x, left.y, left.z}};
            for (const Point3& point : corners) {
              auto* corner = signal.mutable_boundary()->add_point();
              corner->set_x(point.x);
              corner->set_y(point.y);
              corner->set_z(point.z);
            }
          }
          switch (bulbs) {
            case 3:
              signal.set_type(horizontal ? Signal::MIX_3_HORIZONTAL
                                         : Signal::MIX_3_VERTICAL);
              break;
            case 2:
              signal.set_type(horizontal ? Signal::MIX_2_HORIZONTAL
                                         : Signal::MIX_2_VERTICAL);
              break;
            case 1:
              signal.set_type(Signal::SINGLE);
              break;
            default:
              signal.set_type(Signal::UNKNOWN);
          }
          for (int i = 0; i < bulbs && !outline; ++i) {
            Subsignal* subsignal = signal.add_subsignal();
            subsignal->mutable_id()->set_id(std::to_string(i));
            subsignal->set_type(Subsignal::CIRCLE);
            const double t = (i + 0.5) / bulbs;
            auto* location = subsignal->mutable_location();
            if (horizontal) {
              location->set_x(left.x + (right.x - left.x) * t);
              location->set_y(left.y + (right.y - left.y) * t);
              location->set_z(left.z + 0.5 * height);
            } else {
              location->set_x(0.5 * (left.x + right.x));
              location->set_y(0.5 * (left.y + right.y));
              location->set_z(left.z + height * (1.0 - t));
            }
          }
          report_->Count("signals");
        }
        for (const int lane : lanes) {
          Polyline stop_line;
          const double s = StopPosition(lanes_[lane], element.ref_lines,
                                        kv.first, &stop_line);
          AppendStopLine(stop_line, signals_[id->second].mutable_stop_line());
          AddLaneObjectOverlap(lane, s, s, id->second, kSignal);
          control_points_.push_back({lane, s, id->second, kSignal});
        }
      }
    } else if (element.subtype == "traffic_sign") {
      const std::string type = sign_type(element);
      const std::vector<int> lanes = lanes_of(element.lanelets);
      if (!IsStopSign(type) && !IsYieldSign(type)) {
        report_->Count("traffic_signs_ignored");
        report_->Count("traffic_signs_ignored_" + type);
        continue;
      }
      if (lanes.empty()) {
        report_->Warn(
            "regulatory_element_unused",
            name + " (traffic_sign " + type + ") is not used by any lane");
        continue;
      }
      if (IsStopSign(type)) {
        add_sign(kStopSign,
                 UniqueId(&element.relation->tags,
                          "stop_sign_" + std::to_string(kv.first)),
                 lanes, element.ref_lines, kv.first, false);
      } else {
        add_sign(kYieldSign,
                 UniqueId(&element.relation->tags,
                          "yield_" + std::to_string(kv.first)),
                 lanes, element.ref_lines, kv.first, false);
      }
    } else if (element.subtype == "right_of_way") {
      const std::vector<int> lanes = lanes_of(lanelets_of(element.yield));
      if (lanes.empty()) {
        report_->Warn("regulatory_element_unused",
                      name + " (right_of_way) has no yielding lane");
        continue;
      }
      if (IsStopSign(sign_type(element))) {
        add_sign(kStopSign,
                 UniqueId(&element.relation->tags,
                          "stop_sign_" + std::to_string(kv.first)),
                 lanes, element.ref_lines, kv.first, false);
      } else {
        add_sign(kYieldSign,
                 UniqueId(&element.relation->tags,
                          "yield_" + std::to_string(kv.first)),
                 lanes, element.ref_lines, kv.first, false);
      }
    } else if (element.subtype == "all_way_stop") {
      // One stop sign per approach; ref_line[i] belongs to yield[i].
      int approaches = 0;
      for (size_t i = 0; i < element.yield.size(); ++i) {
        const std::vector<int> lanes =
            lanes_of(lanelets_of({element.yield[i]}));
        if (lanes.empty()) {
          continue;
        }
        std::vector<OsmId> ref_lines;
        if (element.ref_lines.size() == element.yield.size()) {
          ref_lines.push_back(element.ref_lines[i]);
        } else {
          ref_lines = element.ref_lines;
        }
        add_sign(kStopSign,
                 UniqueId(nullptr, "stop_sign_" + std::to_string(kv.first) +
                                       "_" + std::to_string(element.yield[i])),
                 lanes, ref_lines, kv.first, true);
        ++approaches;
      }
      if (approaches == 0) {
        report_->Warn("regulatory_element_unused",
                      name + " (all_way_stop) has no yielding lane");
      }
    } else if (element.subtype == "road_marking") {
      // Autoware: a stop line painted on the road.
      std::vector<OsmId> stop_lines;
      for (const OsmId way_id : element.refers) {
        const auto way = line_strings_.find(way_id);
        if (way != line_strings_.end() &&
            TagOr(*way->second.tags, "type", EmptyString()) == "stop_line") {
          stop_lines.push_back(way_id);
        }
      }
      const std::vector<int> lanes = lanes_of(element.lanelets);
      if (stop_lines.empty() || lanes.empty()) {
        report_->Count("regulatory_elements_ignored");
        continue;
      }
      add_sign(kStopSign,
               UniqueId(&element.relation->tags,
                        "stop_sign_" + std::to_string(kv.first)),
               lanes, stop_lines, kv.first, false);
    } else if (element.subtype == "speed_limit") {
      // Applied to the lanes in SpeedLimit().
    } else if (kNotConverted.count(element.subtype) > 0) {
      report_->Count("regulatory_elements_ignored");
    } else {
      report_->UnknownTag("regulatory_element subtype=" + element.subtype);
    }
  }
  report_->Count("stop_signs", static_cast<int>(stop_signs_.size()));
  report_->Count("yield_signs", static_cast<int>(yield_signs_.size()));
}

///////////////////////////////////////////////////////////////////////////////
// Junctions

void Lanelet2Converter::BuildExplicitJunctions(const OsmFile& osm) {
  // Autoware: a closed way tagged type=intersection_area; the lanelets of
  // the intersection carry the tag intersection_area=<way id>.
  std::map<std::string, int> by_way_id;
  for (const auto& kv : osm.ways) {
    if (TagOr(kv.second.tags, "type", EmptyString()) != "intersection_area") {
      continue;
    }
    const auto line = line_strings_.find(kv.first);
    std::vector<Point3> polygon =
        line != line_strings_.end() ? line->second.points : Polyline();
    if (polygon.size() >= 2 &&
        Distance2d(polygon.front(), polygon.back()) < 1e-6) {
      polygon.pop_back();
    }
    if (polygon.size() < 3 || std::fabs(SignedArea(polygon)) < 1e-2) {
      report_->Warn(
          "junction_skipped",
          "intersection_area " + std::to_string(kv.first) + " has no area");
      continue;
    }
    JunctionData junction;
    junction.id =
        UniqueId(&kv.second.tags, "junction_" + std::to_string(kv.first));
    junction.polygon = std::move(polygon);
    by_way_id[std::to_string(kv.first)] = static_cast<int>(junctions_.size());
    junctions_.push_back(std::move(junction));
    report_->Count("junctions_explicit");
  }
  for (size_t i = 0; i < lanes_.size(); ++i) {
    const OsmTags& tags = lanelets_[lanes_[i].lanelet].relation->tags;
    const auto tag = tags.find("intersection_area");
    if (tag == tags.end()) {
      continue;
    }
    const auto junction = by_way_id.find(tag->second);
    if (junction == by_way_id.end()) {
      report_->Warn("missing_intersection_area",
                    "lanelet " + lanes_[i].id +
                        " refers to the unknown intersection_area " +
                        tag->second);
      continue;
    }
    junctions_[junction->second].member_lanes.insert(static_cast<int>(i));
  }
}

void Lanelet2Converter::BuildLaneConflicts() {
  struct Conflict {
    int a = 0;
    int b = 0;
    double a_begin = 0.0;
    double a_end = 0.0;
    double b_begin = 0.0;
    double b_end = 0.0;
    bool merge = false;
    bool diverge = false;
  };

  auto related = [this](int a, int b) {
    const LaneData& lane = lanes_[a];
    if (lane.lanelet == lanes_[b].lanelet || lane.self_reverse == b ||
        lanes_[b].self_reverse == a) {
      return true;
    }
    for (const auto* list :
         {&lane.successors, &lane.predecessors, &lane.left_forward,
          &lane.right_forward, &lane.left_reverse, &lane.right_reverse}) {
      if (std::find(list->begin(), list->end(), b) != list->end()) {
        return true;
      }
    }
    return false;
  };
  // Does the centerline of `a` run through the inside of `b`?
  auto enters = [this](int a, int b) {
    const LaneData& lane = lanes_[a];
    const LaneData& other = lanes_[b];
    for (const Point3& point : lane.center) {
      if (point.x < other.box.min_x || point.x > other.box.max_x ||
          point.y < other.box.min_y || point.y > other.box.max_y) {
        continue;
      }
      if (PointInPolygon(other.polygon, point) &&
          DistanceToPolygonBorder(other.polygon, point) > kConflictMargin) {
        return true;
      }
    }
    return false;
  };
  // Part of `a` whose cross sections reach into `b`.
  auto range = [this](int a, int b, double* begin, double* end) {
    const LaneData& lane = lanes_[a];
    const LaneData& other = lanes_[b];
    bool found = false;
    for (size_t i = 0; i < lane.center.size(); ++i) {
      if (!SegmentTouchesPolygon(lane.left_foot[i], lane.right_foot[i],
                                 other.polygon)) {
        continue;
      }
      if (!found) {
        *begin = lane.center_s[i];
        found = true;
      }
      *end = lane.center_s[i];
    }
    return found;
  };
  auto share = [](const std::vector<int>& a, const std::vector<int>& b) {
    for (const int value : a) {
      if (std::find(b.begin(), b.end(), value) != b.end()) {
        return true;
      }
    }
    return false;
  };

  // Candidate pairs from a uniform grid over the lane boxes.
  constexpr double kCell = 25.0;
  std::map<std::pair<int64_t, int64_t>, std::vector<int>> grid;
  for (size_t i = 0; i < lanes_.size(); ++i) {
    const LaneData& lane = lanes_[i];
    if (lane.type != Lane::CITY_DRIVING && lane.type != Lane::BIKING) {
      continue;
    }
    for (int64_t x = static_cast<int64_t>(std::floor(lane.box.min_x / kCell));
         x <= static_cast<int64_t>(std::floor(lane.box.max_x / kCell)); ++x) {
      for (int64_t y = static_cast<int64_t>(std::floor(lane.box.min_y / kCell));
           y <= static_cast<int64_t>(std::floor(lane.box.max_y / kCell)); ++y) {
        grid[{x, y}].push_back(static_cast<int>(i));
      }
    }
  }
  std::set<std::pair<int, int>> candidates;
  for (const auto& cell : grid) {
    for (size_t i = 0; i < cell.second.size(); ++i) {
      for (size_t j = i + 1; j < cell.second.size(); ++j) {
        const int a = cell.second[i];
        const int b = cell.second[j];
        if (lanes_[a].box.Overlaps(lanes_[b].box) && !related(a, b)) {
          candidates.insert({a, b});
        }
      }
    }
  }

  std::vector<Conflict> conflicts;
  for (const auto& pair : candidates) {
    Conflict conflict;
    conflict.a = pair.first;
    conflict.b = pair.second;
    if (!enters(conflict.a, conflict.b) && !enters(conflict.b, conflict.a)) {
      continue;
    }
    if (!range(conflict.a, conflict.b, &conflict.a_begin, &conflict.a_end) ||
        !range(conflict.b, conflict.a, &conflict.b_begin, &conflict.b_end)) {
      continue;
    }
    conflict.merge =
        share(lanes_[conflict.a].successors, lanes_[conflict.b].successors);
    conflict.diverge =
        share(lanes_[conflict.a].predecessors, lanes_[conflict.b].predecessors);
    conflicts.push_back(conflict);
  }

  // Lane/lane overlaps.
  for (const Conflict& conflict : conflicts) {
    Overlap overlap;
    const std::string id = NextOverlapId();
    overlap.mutable_id()->set_id(id);
    ObjectOverlapInfo* first = overlap.add_object();
    first->mutable_id()->set_id(lanes_[conflict.a].id);
    first->mutable_lane_overlap_info()->set_start_s(conflict.a_begin);
    first->mutable_lane_overlap_info()->set_end_s(conflict.a_end);
    first->mutable_lane_overlap_info()->set_is_merge(conflict.merge);
    ObjectOverlapInfo* second = overlap.add_object();
    second->mutable_id()->set_id(lanes_[conflict.b].id);
    second->mutable_lane_overlap_info()->set_start_s(conflict.b_begin);
    second->mutable_lane_overlap_info()->set_end_s(conflict.b_end);
    second->mutable_lane_overlap_info()->set_is_merge(conflict.merge);
    lanes_[conflict.a].overlap_ids.push_back(id);
    lanes_[conflict.b].overlap_ids.push_back(id);
    overlaps_.push_back(std::move(overlap));
    report_->Count("overlaps");
    report_->Count("lane_lane_overlaps");
  }

  if (!options_.infer_junctions) {
    return;
  }
  // Junctions: groups of conflicting driving lanes in which at least two
  // lanes really cross (merging and diverging alone is a fork, not a
  // junction). The polygon is the hull of the conflicting lane parts.
  std::set<int> explicit_members;
  for (const JunctionData& junction : junctions_) {
    explicit_members.insert(junction.member_lanes.begin(),
                            junction.member_lanes.end());
  }
  UnionFind sets(lanes_.size());
  for (const Conflict& conflict : conflicts) {
    if (lanes_[conflict.a].type == Lane::CITY_DRIVING &&
        lanes_[conflict.b].type == Lane::CITY_DRIVING) {
      sets.Union(conflict.a, conflict.b);
    }
  }
  struct Cluster {
    bool crossing = false;
    bool has_explicit = false;
    std::map<int, std::pair<double, double>> spans;  // lane -> conflict span
  };
  std::map<int, Cluster> clusters;
  for (const Conflict& conflict : conflicts) {
    if (lanes_[conflict.a].type != Lane::CITY_DRIVING ||
        lanes_[conflict.b].type != Lane::CITY_DRIVING) {
      continue;
    }
    Cluster& cluster = clusters[sets.Find(conflict.a)];
    cluster.crossing |= !conflict.merge && !conflict.diverge;
    const std::pair<int, std::pair<double, double>> spans[2] = {
        {conflict.a, {conflict.a_begin, conflict.a_end}},
        {conflict.b, {conflict.b_begin, conflict.b_end}}};
    for (const auto& span : spans) {
      cluster.has_explicit |= explicit_members.count(span.first) > 0;
      const auto it = cluster.spans.find(span.first);
      if (it == cluster.spans.end()) {
        cluster.spans[span.first] = span.second;
      } else {
        it->second.first = std::min(it->second.first, span.second.first);
        it->second.second = std::max(it->second.second, span.second.second);
      }
    }
  }
  for (const auto& kv : clusters) {
    const Cluster& cluster = kv.second;
    if (!cluster.crossing || cluster.has_explicit) {
      continue;
    }
    std::vector<Point3> points;
    OsmId smallest = std::numeric_limits<OsmId>::max();
    for (const auto& span : cluster.spans) {
      const LaneData& lane = lanes_[span.first];
      smallest = std::min(smallest, lanelets_[lane.lanelet].id);
      // A lane that is in conflict over most of its length is a connector
      // of the intersection and belongs to it as a whole; of a long lane
      // that merely runs through, only the conflict zone does.
      const bool connector =
          span.second.second - span.second.first >= 0.5 * lane.length;
      for (size_t i = 0; i < lane.center.size(); ++i) {
        if (connector || (lane.center_s[i] >= span.second.first - 1e-6 &&
                          lane.center_s[i] <= span.second.second + 1e-6)) {
          points.push_back(lane.left_foot[i]);
          points.push_back(lane.right_foot[i]);
        }
      }
    }
    JunctionData junction;
    junction.polygon = ConvexHull(points);
    RemoveCollinearPoints(&junction.polygon, 0.01);
    if (junction.polygon.size() < 3 ||
        std::fabs(SignedArea(junction.polygon)) < 1.0) {
      continue;
    }
    for (Point3& point : junction.polygon) {
      point.z = 0.0;
    }
    junction.inferred = true;
    junction.id = UniqueId(nullptr, "junction_" + std::to_string(smallest));
    for (const auto& span : cluster.spans) {
      junction.member_lanes.insert(span.first);
    }
    junctions_.push_back(std::move(junction));
    report_->Count("junctions_inferred");
  }
}

void Lanelet2Converter::BuildJunctionOverlaps() {
  for (size_t j = 0; j < junctions_.size(); ++j) {
    // Copy: AddLaneObjectOverlap() appends to junctions_[j].overlap_ids.
    const std::vector<Point3> polygon = junctions_[j].polygon;
    const std::string id = junctions_[j].id;
    const bool inferred = junctions_[j].inferred;
    const std::set<int> members = junctions_[j].member_lanes;
    BoundingBox box;
    box.Add(polygon);
    for (size_t i = 0; i < lanes_.size(); ++i) {
      LaneData& lane = lanes_[i];
      if (lane.type == Lane::SIDEWALK || !lane.box.Overlaps(box)) {
        continue;
      }
      const auto ranges =
          RangesInsidePolygon(lane.center, lane.center_s, polygon);
      double inside = 0.0;
      for (const auto& range : ranges) {
        inside += range.second - range.first;
      }
      const bool tagged = !inferred && members.count(static_cast<int>(i)) > 0;
      if (inside < kMinOverlapLength) {
        if (tagged) {
          report_->Warn("lanelet_outside_intersection_area",
                        "lanelet " + lane.id + " is tagged with " + id +
                            " but does not run through it");
        }
        continue;
      }
      AddLaneObjectOverlap(static_cast<int>(i), ranges.front().first,
                           ranges.back().second, id, kJunction);
      lane_junctions_[static_cast<int>(i)].push_back(
          {static_cast<int>(j), {ranges.front().first, ranges.back().second}});
      if (lane.junction_id.empty() &&
          (tagged || inside >= kJunctionLaneRatio * lane.length)) {
        lane.junction_id = id;
      }
    }
  }
  report_->Count("junctions", static_cast<int>(junctions_.size()));

  for (RoadData& road : roads_) {
    std::string junction_id = lanes_[road.lanes.front()].junction_id;
    for (const int lane : road.lanes) {
      if (lanes_[lane].junction_id != junction_id) {
        junction_id.clear();
        break;
      }
    }
    road.junction_id = junction_id;
  }
}

void Lanelet2Converter::LinkControlsToJunctions() {
  // A signal or sign belongs to the junction its lane leads into: Apollo
  // looks up the other stop signs of a junction through these overlaps.
  std::set<std::pair<std::string, std::string>> linked;
  for (const ControlPoint& control : control_points_) {
    int junction = -1;
    const auto own = lane_junctions_.find(control.lane);
    if (own != lane_junctions_.end()) {
      for (const auto& entry : own->second) {
        if (entry.second.second >= control.s - 1.0) {
          junction = entry.first;
          break;
        }
      }
    }
    for (size_t i = 0;
         junction < 0 && i < lanes_[control.lane].successors.size(); ++i) {
      const auto next =
          lane_junctions_.find(lanes_[control.lane].successors[i]);
      if (next != lane_junctions_.end() && !next->second.empty()) {
        junction = next->second.front().first;
      }
    }
    if (junction < 0) {
      continue;
    }
    const std::string junction_id = junctions_[junction].id;
    if (!linked.insert({junction_id, control.object_id}).second) {
      continue;
    }
    Overlap overlap;
    const std::string id = NextOverlapId();
    overlap.mutable_id()->set_id(id);
    ObjectOverlapInfo* object = overlap.add_object();
    object->mutable_id()->set_id(control.object_id);
    SetObjectInfo(static_cast<ObjectKind>(control.kind), object);
    ObjectOverlapInfo* junction_info = overlap.add_object();
    junction_info->mutable_id()->set_id(junction_id);
    junction_info->mutable_junction_overlap_info();
    AddObjectOverlapId(control.object_id, static_cast<ObjectKind>(control.kind),
                       id);
    junctions_[junction].overlap_ids.push_back(id);
    overlaps_.push_back(std::move(overlap));
    report_->Count("overlaps");
  }
}

void Lanelet2Converter::InferTurns() {
  for (LaneData& lane : lanes_) {
    if (lane.type != Lane::CITY_DRIVING) {
      continue;
    }
    // Heading over the first and the last meter of the centerline.
    const double reach = std::min(1.0, 0.5 * lane.length);
    const Point3 a0 = lane.center.front();
    const Point3 a1 = PointAt(lane.center, lane.center_s, reach);
    const Point3 b0 = PointAt(lane.center, lane.center_s, lane.length - reach);
    const Point3 b1 = lane.center.back();
    const double change = NormalizeAngle(std::atan2(b1.y - b0.y, b1.x - b0.x) -
                                         std::atan2(a1.y - a0.y, a1.x - a0.x));
    if (lane.has_turn_tag) {
      // turn_direction knows left and right only: a tagged turn that comes
      // back the way it went is a u-turn for Apollo.
      if (!lane.turn_is_final && lane.turn != Lane::NO_TURN &&
          std::fabs(change) >= kUTurnAngle) {
        lane.turn = Lane::U_TURN;
      }
      continue;
    }
    // Without tag only lanes inside a junction turn; a road may bend.
    if (lane.junction_id.empty()) {
      continue;
    }
    if (std::fabs(change) >= kUTurnAngle) {
      lane.turn = Lane::U_TURN;
    } else if (change >= kTurnAngle) {
      lane.turn = Lane::LEFT_TURN;
    } else if (change <= -kTurnAngle) {
      lane.turn = Lane::RIGHT_TURN;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Output

void Lanelet2Converter::FillCurve(const Polyline& points, Curve* curve) {
  CurveSegment* segment = curve->add_segment();
  LineSegment* line = segment->mutable_line_segment();
  for (const Point3& point : points) {
    SetPoint(point, line->add_point());
  }
  segment->set_s(0.0);
  if (!points.empty()) {
    SetPoint(points.front(), segment->mutable_start_position());
  }
  if (points.size() >= 2) {
    segment->set_heading(
        std::atan2(points[1].y - points[0].y, points[1].x - points[0].x));
  }
  segment->set_length(PolylineLength(points));
}

void Lanelet2Converter::WriteMap(const OsmFile& osm,
                                 const std::string& district,
                                 apollo::hdmap::Map* map) {
  Header* header = map->mutable_header();
  header->set_version(osm.map_version.empty() ? "1.0" : osm.map_version);
  if (!osm.format_version.empty()) {
    header->set_rev_major(osm.format_version);
  }
  header->set_district(district);
  header->set_vendor(osm.generator.empty() ? "lanelet2"
                                           : "lanelet2/" + osm.generator);
  if (!projection_.empty()) {
    header->mutable_projection()->set_proj(projection_);
    double west = 180.0;
    double east = -180.0;
    double south = 90.0;
    double north = -90.0;
    for (const auto& kv : osm.nodes) {
      west = std::min(west, kv.second.lon);
      east = std::max(east, kv.second.lon);
      south = std::min(south, kv.second.lat);
      north = std::max(north, kv.second.lat);
    }
    header->set_left(west);
    header->set_right(east);
    header->set_top(north);
    header->set_bottom(south);
  }

  auto write_boundary = [](const Bound& bound, const BoundaryStyle& style,
                           LaneBoundary* out) {
    FillCurve(bound.points, out->mutable_curve());
    out->set_length(PolylineLength(bound.points));
    out->set_virtual_(style.is_virtual);
    LaneBoundaryType* type = out->add_boundary_type();
    type->set_s(0.0);
    type->add_types(style.type);
  };
  auto write_ids = [this](const std::vector<int>& lanes,
                          google::protobuf::RepeatedPtrField<Id>* out) {
    for (const int lane : lanes) {
      out->Add()->set_id(lanes_[lane].id);
    }
  };
  auto write_samples =
      [](const std::vector<double>& s, const std::vector<double>& width,
         google::protobuf::RepeatedPtrField<LaneSampleAssociation>* out) {
        for (size_t i = 0; i < s.size(); ++i) {
          LaneSampleAssociation* sample = out->Add();
          sample->set_s(s[i]);
          sample->set_width(width[i]);
        }
      };

  for (const LaneData& lane : lanes_) {
    Lane* out = map->add_lane();
    out->mutable_id()->set_id(lane.id);
    FillCurve(lane.center, out->mutable_central_curve());
    write_boundary(lane.left, lane.left_style, out->mutable_left_boundary());
    write_boundary(lane.right, lane.right_style, out->mutable_right_boundary());
    out->set_length(lane.length);
    out->set_speed_limit(lane.speed_limit);
    for (const std::string& overlap_id : lane.overlap_ids) {
      out->add_overlap_id()->set_id(overlap_id);
    }
    write_ids(lane.predecessors, out->mutable_predecessor_id());
    write_ids(lane.successors, out->mutable_successor_id());
    write_ids(lane.left_forward, out->mutable_left_neighbor_forward_lane_id());
    write_ids(lane.right_forward,
              out->mutable_right_neighbor_forward_lane_id());
    write_ids(lane.left_reverse, out->mutable_left_neighbor_reverse_lane_id());
    write_ids(lane.right_reverse,
              out->mutable_right_neighbor_reverse_lane_id());
    out->set_type(lane.type);
    out->set_turn(lane.turn);
    if (!lane.junction_id.empty()) {
      out->mutable_junction_id()->set_id(lane.junction_id);
    }
    write_samples(lane.center_s, lane.left_width, out->mutable_left_sample());
    write_samples(lane.center_s, lane.right_width, out->mutable_right_sample());
    out->set_direction(lane.direction);
    write_samples(lane.center_s, lane.left_road_width,
                  out->mutable_left_road_sample());
    write_samples(lane.center_s, lane.right_road_width,
                  out->mutable_right_road_sample());
    if (lane.self_reverse >= 0) {
      out->add_self_reverse_lane_id()->set_id(lanes_[lane.self_reverse].id);
    }
  }

  for (const RoadData& road : roads_) {
    Road* out = map->add_road();
    out->mutable_id()->set_id(road.id);
    out->set_type(road.highway ? Road::HIGHWAY : Road::CITY_ROAD);
    if (!road.junction_id.empty()) {
      out->mutable_junction_id()->set_id(road.junction_id);
    }
    RoadSection* section = out->add_section();
    section->mutable_id()->set_id("1");
    for (const int lane : road.lanes) {
      section->add_lane_id()->set_id(lanes_[lane].id);
    }
    BoundaryPolygon* outer =
        section->mutable_boundary()->mutable_outer_polygon();
    BoundaryEdge* left = outer->add_edge();
    left->set_type(BoundaryEdge::LEFT_BOUNDARY);
    FillCurve(lanes_[road.lanes.front()].left.points, left->mutable_curve());
    BoundaryEdge* right = outer->add_edge();
    right->set_type(BoundaryEdge::RIGHT_BOUNDARY);
    FillCurve(lanes_[road.lanes.back()].right.points, right->mutable_curve());
  }

  for (const JunctionData& junction : junctions_) {
    Junction* out = map->add_junction();
    out->mutable_id()->set_id(junction.id);
    for (const Point3& point : junction.polygon) {
      auto* corner = out->mutable_polygon()->add_point();
      corner->set_x(point.x);
      corner->set_y(point.y);
      corner->set_z(point.z);
    }
    for (const std::string& overlap_id : junction.overlap_ids) {
      out->add_overlap_id()->set_id(overlap_id);
    }
  }
  for (const auto& kv : signals_) {
    *map->add_signal() = kv.second;
  }
  for (const auto& kv : stop_signs_) {
    *map->add_stop_sign() = kv.second;
  }
  for (const auto& kv : yield_signs_) {
    *map->add_yield() = kv.second;
  }
  for (const auto& kv : crosswalks_) {
    *map->add_crosswalk() = kv.second;
  }
  for (const Overlap& overlap : overlaps_) {
    *map->add_overlap() = overlap;
  }
}

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
