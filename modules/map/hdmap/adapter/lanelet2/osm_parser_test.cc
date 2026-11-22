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

#include "gtest/gtest.h"

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

TEST(OsmParserTest, ParsesNodesWaysAndRelations) {
  const char kOsm[] = R"(<?xml version='1.0' encoding='UTF-8'?>
<osm version='0.6' generator='JOSM'>
  <MetaInfo format_version='1' map_version='3' />
  <node id='7' lat='49.5' lon='8.25' />
  <node id='-3' lat='49.6' lon='8.26'>
    <tag k='ele' v='112.5' />
    <tag k='local_x' v='10.5' />
  </node>
  <node id='9' action='delete' lat='0' lon='0' />
  <node id='10' lat='north' lon='8.0' />
  <way id='20'>
    <nd ref='7' /><nd ref='-3' />
    <tag k='type' v='line_thin' /><tag k='subtype' v='dashed' />
  </way>
  <way id='21' action='delete'><nd ref='7' /></way>
  <way id='22'><nd ref='x' /></way>
  <relation id='30'>
    <member type='way' ref='20' role='left' />
    <member type='relation' ref='31' role='regulatory_element' />
    <member type='node' ref='7' role='' />
    <tag k='type' v='lanelet' />
  </relation>
  <relation id='31'><member type='area' ref='1' role='x' /></relation>
</osm>)";
  OsmFile osm;
  std::vector<std::string> errors;
  ASSERT_TRUE(ParseOsmString(kOsm, &osm, &errors));
  EXPECT_EQ("JOSM", osm.generator);
  EXPECT_EQ("1", osm.format_version);
  EXPECT_EQ("3", osm.map_version);

  // deleted and broken elements are dropped, the latter with a message
  ASSERT_EQ(2U, osm.nodes.size());
  EXPECT_TRUE(osm.has_nodes);
  EXPECT_EQ(7, osm.first_node_id);
  EXPECT_DOUBLE_EQ(49.5, osm.nodes.at(7).lat);
  EXPECT_DOUBLE_EQ(8.25, osm.nodes.at(7).lon);
  EXPECT_EQ("112.5", osm.nodes.at(-3).tags.at("ele"));
  EXPECT_EQ("10.5", TagOr(osm.nodes.at(-3).tags, "local_x", ""));
  EXPECT_EQ("none", TagOr(osm.nodes.at(-3).tags, "local_y", "none"));

  ASSERT_EQ(1U, osm.ways.size());
  EXPECT_EQ((std::vector<OsmId>{7, -3}), osm.ways.at(20).node_ids);
  EXPECT_EQ("dashed", osm.ways.at(20).tags.at("subtype"));

  ASSERT_EQ(1U, osm.relations.size());
  const OsmRelation& relation = osm.relations.at(30);
  ASSERT_EQ(3U, relation.members.size());
  EXPECT_EQ(OsmMember::Type::WAY, relation.members[0].type);
  EXPECT_EQ(20, relation.members[0].ref);
  EXPECT_EQ("left", relation.members[0].role);
  EXPECT_EQ(OsmMember::Type::RELATION, relation.members[1].type);
  EXPECT_EQ(OsmMember::Type::NODE, relation.members[2].type);
  EXPECT_EQ("lanelet", relation.tags.at("type"));

  ASSERT_EQ(3U, errors.size());
  EXPECT_NE(std::string::npos, errors[0].find("node 10"));
  EXPECT_NE(std::string::npos, errors[1].find("way 22"));
  EXPECT_NE(std::string::npos, errors[2].find("relation 31"));
}

TEST(OsmParserTest, RejectsWhatIsNotOsm) {
  OsmFile osm;
  std::vector<std::string> errors;
  EXPECT_FALSE(ParseOsmString("<osm><node", &osm, &errors));
  EXPECT_FALSE(ParseOsmString("<gpx version='1.1'/>", &osm, &errors));
  EXPECT_FALSE(ParseOsmFile("/nonexistent/map.osm", &osm, &errors));
  EXPECT_EQ(3U, errors.size());
  // an empty map is a valid file; the adapter rejects it later
  errors.clear();
  EXPECT_TRUE(ParseOsmString("<osm version='0.6'/>", &osm, &errors));
  EXPECT_TRUE(errors.empty());
  EXPECT_FALSE(osm.has_nodes);
}

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
