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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

/**
 * @brief In-memory representation of an OSM XML file as written by JOSM,
 * Lanelet2 or the Autoware Vector Map Builder. Only what Lanelet2 itself
 * reads is kept: nodes, ways and relations with their tags.
 */
using OsmId = int64_t;
using OsmTags = std::map<std::string, std::string>;

struct OsmNode {
  OsmId id = 0;
  double lat = 0.0;
  double lon = 0.0;
  OsmTags tags;
};

struct OsmWay {
  OsmId id = 0;
  std::vector<OsmId> node_ids;
  OsmTags tags;
};

struct OsmMember {
  enum class Type { NODE, WAY, RELATION };
  Type type = Type::WAY;
  OsmId ref = 0;
  std::string role;
};

struct OsmRelation {
  OsmId id = 0;
  std::vector<OsmMember> members;
  OsmTags tags;
};

struct OsmFile {
  // "generator" attribute of the <osm> element.
  std::string generator;
  // Autoware <MetaInfo format_version="" map_version=""/>, empty if absent.
  std::string format_version;
  std::string map_version;
  // Id of the first node in document order (used to pick the UTM zone).
  OsmId first_node_id = 0;
  bool has_nodes = false;

  // Ordered by id, which keeps every later step deterministic.
  std::map<OsmId, OsmNode> nodes;
  std::map<OsmId, OsmWay> ways;
  std::map<OsmId, OsmRelation> relations;
};

/// Returns the tag value or `fallback` when the key is absent.
const std::string& TagOr(const OsmTags& tags, const std::string& key,
                         const std::string& fallback);

/**
 * @brief Parses an OSM XML file.
 * @param filename path of the .osm file
 * @param osm output; elements marked action="delete" are dropped
 * @param errors human readable problems of single elements (the element is
 *        skipped, parsing goes on)
 * @return false only if the file cannot be read or is not an OSM document
 */
bool ParseOsmFile(const std::string& filename, OsmFile* osm,
                  std::vector<std::string>* errors);

/// Same as ParseOsmFile for an in-memory document.
bool ParseOsmString(const std::string& content, OsmFile* osm,
                    std::vector<std::string>* errors);

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
