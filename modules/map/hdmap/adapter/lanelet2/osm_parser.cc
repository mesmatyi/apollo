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
#include "modules/map/hdmap/adapter/lanelet2/osm_parser.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <tinyxml2.h>

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

namespace {

using tinyxml2::XMLElement;

bool ParseId(const char* text, OsmId* id) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char* end = nullptr;
  errno = 0;
  const long long value = std::strtoll(text, &end, 10);  // NOLINT
  if (errno != 0 || end == text || *end != '\0') {
    return false;
  }
  *id = static_cast<OsmId>(value);
  return true;
}

bool ParseDouble(const char* text, double* value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char* end = nullptr;
  errno = 0;
  const double v = std::strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' || !std::isfinite(v)) {
    return false;
  }
  *value = v;
  return true;
}

bool IsDeleted(const XMLElement& element) {
  const char* action = element.Attribute("action");
  return action != nullptr && std::strcmp(action, "delete") == 0;
}

void ReadTags(const XMLElement& element, OsmTags* tags) {
  for (const XMLElement* tag = element.FirstChildElement("tag"); tag != nullptr;
       tag = tag->NextSiblingElement("tag")) {
    const char* key = tag->Attribute("k");
    const char* value = tag->Attribute("v");
    if (key != nullptr && value != nullptr) {
      (*tags)[key] = value;
    }
  }
}

std::string Describe(const char* kind, const XMLElement& element) {
  const char* id = element.Attribute("id");
  return std::string(kind) + " " + (id != nullptr ? id : "<no id>") +
         " (line " + std::to_string(element.GetLineNum()) + ")";
}

bool ReadDocument(tinyxml2::XMLDocument* document, OsmFile* osm,
                  std::vector<std::string>* errors) {
  const XMLElement* root = document->RootElement();
  if (root == nullptr || std::strcmp(root->Name(), "osm") != 0) {
    errors->emplace_back("the document has no <osm> root element");
    return false;
  }
  if (const char* generator = root->Attribute("generator")) {
    osm->generator = generator;
  }
  if (const XMLElement* meta = root->FirstChildElement("MetaInfo")) {
    if (const char* v = meta->Attribute("format_version")) {
      osm->format_version = v;
    }
    if (const char* v = meta->Attribute("map_version")) {
      osm->map_version = v;
    }
  }

  for (const XMLElement* e = root->FirstChildElement("node"); e != nullptr;
       e = e->NextSiblingElement("node")) {
    if (IsDeleted(*e)) {
      continue;
    }
    OsmNode node;
    if (!ParseId(e->Attribute("id"), &node.id) ||
        !ParseDouble(e->Attribute("lat"), &node.lat) ||
        !ParseDouble(e->Attribute("lon"), &node.lon)) {
      errors->push_back(Describe("node", *e) +
                        " has no valid id/lat/lon attributes");
      continue;
    }
    ReadTags(*e, &node.tags);
    if (!osm->has_nodes) {
      osm->has_nodes = true;
      osm->first_node_id = node.id;
    }
    osm->nodes[node.id] = std::move(node);
  }

  for (const XMLElement* e = root->FirstChildElement("way"); e != nullptr;
       e = e->NextSiblingElement("way")) {
    if (IsDeleted(*e)) {
      continue;
    }
    OsmWay way;
    if (!ParseId(e->Attribute("id"), &way.id)) {
      errors->push_back(Describe("way", *e) + " has no valid id");
      continue;
    }
    bool valid = true;
    for (const XMLElement* nd = e->FirstChildElement("nd"); nd != nullptr;
         nd = nd->NextSiblingElement("nd")) {
      OsmId ref = 0;
      if (!ParseId(nd->Attribute("ref"), &ref)) {
        valid = false;
        break;
      }
      way.node_ids.push_back(ref);
    }
    if (!valid) {
      errors->push_back(Describe("way", *e) + " has an invalid <nd> element");
      continue;
    }
    ReadTags(*e, &way.tags);
    osm->ways[way.id] = std::move(way);
  }

  for (const XMLElement* e = root->FirstChildElement("relation"); e != nullptr;
       e = e->NextSiblingElement("relation")) {
    if (IsDeleted(*e)) {
      continue;
    }
    OsmRelation relation;
    if (!ParseId(e->Attribute("id"), &relation.id)) {
      errors->push_back(Describe("relation", *e) + " has no valid id");
      continue;
    }
    bool valid = true;
    for (const XMLElement* m = e->FirstChildElement("member"); m != nullptr;
         m = m->NextSiblingElement("member")) {
      OsmMember member;
      const char* type = m->Attribute("type");
      const char* role = m->Attribute("role");
      if (type == nullptr || !ParseId(m->Attribute("ref"), &member.ref)) {
        valid = false;
        break;
      }
      if (std::strcmp(type, "node") == 0) {
        member.type = OsmMember::Type::NODE;
      } else if (std::strcmp(type, "way") == 0) {
        member.type = OsmMember::Type::WAY;
      } else if (std::strcmp(type, "relation") == 0) {
        member.type = OsmMember::Type::RELATION;
      } else {
        valid = false;
        break;
      }
      member.role = role != nullptr ? role : "";
      relation.members.push_back(std::move(member));
    }
    if (!valid) {
      errors->push_back(Describe("relation", *e) +
                        " has an invalid <member> element");
      continue;
    }
    ReadTags(*e, &relation.tags);
    osm->relations[relation.id] = std::move(relation);
  }
  return true;
}

}  // namespace

const std::string& TagOr(const OsmTags& tags, const std::string& key,
                         const std::string& fallback) {
  const auto it = tags.find(key);
  return it != tags.end() ? it->second : fallback;
}

bool ParseOsmFile(const std::string& filename, OsmFile* osm,
                  std::vector<std::string>* errors) {
  tinyxml2::XMLDocument document;
  if (document.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS) {
    errors->push_back("cannot read " + filename + ": " + document.ErrorStr());
    return false;
  }
  return ReadDocument(&document, osm, errors);
}

bool ParseOsmString(const std::string& content, OsmFile* osm,
                    std::vector<std::string>* errors) {
  tinyxml2::XMLDocument document;
  if (document.Parse(content.c_str(), content.size()) !=
      tinyxml2::XML_SUCCESS) {
    errors->push_back(std::string("cannot parse the OSM document: ") +
                      document.ErrorStr());
    return false;
  }
  return ReadDocument(&document, osm, errors);
}

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
