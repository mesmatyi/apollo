#!/usr/bin/env python3
###############################################################################
# Copyright 2026 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################
"""Generates the synthetic Lanelet2 maps of the adapter unit tests
(modules/map/hdmap/adapter/lanelet2/testdata/*.osm).

  make_test_maps.py [--output_dir modules/map/hdmap/adapter/lanelet2/testdata]

The maps are drawn in meters around ORIGIN; with the adapter option
origin = ORIGIN the Apollo coordinates are those meters again, which is what
lanelet2_adapter_test.cc asserts. Ids are fixed so that the tests can name
lanes. Re-run this script after changing a map and commit the result.
"""

import argparse
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import lanelet2_common as common  # noqa: E402  pylint: disable=wrong-import-position

ORIGIN = (49.0, 8.4)  # lat, lon; UTM zone 32


class Osm(object):
    """A small OSM XML writer with meter coordinates."""

    def __init__(self, local_xy=False):
        self.projection = common.Projection(common.utm_proj_string(32))
        self.x0, self.y0 = self.projection.forward(*ORIGIN)
        self.local_xy = local_xy
        self.nodes = []
        self.ways = []
        self.relations = []
        self.raw = []
        self._node_ids = {}
        self._next_node = 1

    def node(self, x, y, z=None, tags=None, share=True):
        """Node at (x, y); nodes at the same place are shared."""
        key = (round(x, 4), round(y, 4), z)
        if share and not tags and key in self._node_ids:
            return self._node_ids[key]
        node_id = self._next_node
        self._next_node += 1
        tags = dict(tags or {})
        if z is not None:
            tags['ele'] = '%g' % z
        if self.local_xy:
            tags['local_x'] = '%.4f' % x
            tags['local_y'] = '%.4f' % y
            lat, lon = 0.0, 0.0  # Autoware local maps carry no geo reference
        else:
            lat, lon = self.projection.inverse(self.x0 + x, self.y0 + y)
        self.nodes.append((node_id, lat, lon, tags))
        if share and not tags.get('share') and key not in self._node_ids:
            self._node_ids[key] = node_id
        return node_id

    def way(self, way_id, points, **tags):
        """Way through (x, y[, z]) points or node ids."""
        ids = [p if isinstance(p, int) else self.node(*p) for p in points]
        self.ways.append((way_id, ids, _tags(tags)))
        return way_id

    def relation(self, relation_id, members, **tags):
        self.relations.append((relation_id, members, _tags(tags)))
        return relation_id

    def lanelet(self, relation_id, left, right, extra=(), **tags):
        members = [('way', left, 'left'), ('way', right, 'right')]
        members += list(extra)
        all_tags = {'type': 'lanelet', 'subtype': 'road',
                    'location': 'urban'}
        all_tags.update(tags)
        return self.relation(relation_id, members, **all_tags)

    def write(self, path, meta=''):
        with open(path, 'w') as f:
            f.write("<?xml version='1.0' encoding='UTF-8'?>\n")
            f.write("<osm version='0.6' generator='make_test_maps.py'>\n")
            if meta:
                f.write('  %s\n' % meta)
            for node_id, lat, lon, tags in self.nodes:
                head = "  <node id='%d' visible='true' version='1' " \
                    "lat='%.11f' lon='%.11f'" % (node_id, lat, lon)
                if tags:
                    f.write(head + '>\n')
                    for k, v in sorted(tags.items()):
                        f.write("    <tag k='%s' v='%s' />\n" % (k, v))
                    f.write('  </node>\n')
                else:
                    f.write(head + ' />\n')
            for way_id, ids, tags in self.ways:
                f.write("  <way id='%d' visible='true' version='1'>\n" %
                        way_id)
                for node_id in ids:
                    f.write("    <nd ref='%d' />\n" % node_id)
                for k, v in tags:
                    f.write("    <tag k='%s' v='%s' />\n" % (k, v))
                f.write('  </way>\n')
            for relation_id, members, tags in self.relations:
                f.write("  <relation id='%d' visible='true' version='1'>\n" %
                        relation_id)
                for kind, ref, role in members:
                    f.write("    <member type='%s' ref='%d' role='%s' />\n" %
                            (kind, ref, role))
                for k, v in tags:
                    f.write("    <tag k='%s' v='%s' />\n" % (k, v))
                f.write('  </relation>\n')
            for line in self.raw:
                f.write('  %s\n' % line)
            f.write('</osm>\n')


def _tags(tags):
    """Keyword arguments -> tag list; `a__b` stands for the key `a:b`."""
    return sorted((k.replace('__', ':'), str(v)) for k, v in tags.items())


def arc(cx, cy, radius, start_deg, end_deg, step_deg=5.0):
    count = max(1, int(round(abs(end_deg - start_deg) / step_deg)))
    return [(cx + radius * math.cos(math.radians(
        start_deg + (end_deg - start_deg) * i / count)),
        cy + radius * math.sin(math.radians(
            start_deg + (end_deg - start_deg) * i / count)))
        for i in range(count + 1)]


def two_lane_road():
    """A straight road of two lanes in the same direction (+x), 100 m long,
    cut at x = 60 into two sections.

        y = 7.0  ---- solid yellow line ----------------------
                  lanelet 101 (60 m)      | lanelet 103 (40 m)
        y = 3.5  - - dashed line, stored against the driving direction
                  lanelet 100 (60 m)      | lanelet 102 (40 m)
        y = 0.0  ==== curbstone ===============================
    """
    osm = Osm()
    # section 1; the divider runs from x = 60 back to x = 0
    osm.way(1000, [(0, 0), (30, 0), (60, 0)], type='curbstone', subtype='high')
    osm.way(1001, [(60, 3.5), (30, 3.5), (0, 3.5)], type='line_thin',
            subtype='dashed')
    osm.way(1002, [(0, 7), (60, 7)], type='line_thin', subtype='solid',
            color='yellow')
    # section 2
    osm.way(1003, [(60, 0), (100, 0)], type='curbstone', subtype='high')
    osm.way(1004, [(60, 3.5), (100, 3.5)], type='line_thin', subtype='dashed')
    osm.way(1005, [(60, 7), (100, 7)], type='line_thin', subtype='solid',
            color='yellow')
    # an explicit centerline for lanelet 103, 0.25 m right of the middle
    osm.way(1006, [(60, 5.0), (100, 5.0)])
    # a 60 km/h sign for lanelet 102
    osm.way(1007, [(60, -1, 2.0), (60.6, -1, 2.0)], type='traffic_sign',
            subtype='de274-60')
    osm.relation(200, [('way', 1007, 'refers')], type='regulatory_element',
                 subtype='speed_limit')

    osm.lanelet(100, 1001, 1000, speed_limit='30')
    osm.lanelet(101, 1002, 1001, speed_limit='40 km/h')
    # the regulatory element wins over the tag
    osm.lanelet(102, 1004, 1003, [('relation', 200, 'regulatory_element')],
                speed_limit='10 mps')
    osm.lanelet(103, 1005, 1004, [('way', 1006, 'centerline')],
                location='nonurban')
    return osm


def curve():
    """20 m straight, a 90 degree left bend (radius 20 m, 3.5 m wide), 20 m
    straight: lanelets 110 -> 111 -> 112. A road that bends is no turn."""
    osm = Osm()
    inner, outer = 18.25, 21.75
    osm.way(1010, [(-20, 1.75), (0, 1.75)], type='line_thin', subtype='solid')
    osm.way(1011, [(-20, -1.75), (0, -1.75)], type='road_border')
    # bend around (0, 20): from heading +x at (0, 0) to heading +y at (20, 20)
    osm.way(1012, arc(0, 20, inner, -90, 0), type='line_thin', subtype='solid')
    osm.way(1013, arc(0, 20, outer, -90, 0), type='road_border')
    osm.way(1014, [(inner, 20), (inner, 40)], type='line_thin',
            subtype='solid')
    osm.way(1015, [(outer, 20), (outer, 40)], type='road_border')
    osm.lanelet(110, 1010, 1011)
    osm.lanelet(111, 1012, 1013)
    osm.lanelet(112, 1014, 1015)
    return osm


def intersection():
    """A crossing of two two-way roads (one lane per direction, 3.5 m wide)
    with the box [-7, 7] x [-7, 7], right-hand traffic.

    Arms (43 m each):          into the box   out of the box
        west  (x < -7)         120            121
        east  (x > 7)          122            123
        south (y < -7)         124            125
        north (y > 7)          126            127
    Connectors: 130 west->east, 131 west->north (left), 132 west->south
    (right), 133 south->north, 134 south->west (left), 135 south->east
    (right), 136 east->west, 137 north->south.
    Traffic light with stop line on 120, traffic light without stop line on
    124, stop sign on 122, yield (right_of_way) on 126, a crosswalk (140)
    across the east arm at x = 20..23.
    """
    osm = Osm()
    w = 3.5
    # arms: divider (shared by both directions) and the two outer borders
    osm.way(1020, [(-50, 0), (-7, 0)], type='line_thin', subtype='solid')
    osm.way(1021, [(-50, -w), (-7, -w)], type='curbstone')
    osm.way(1022, [(-50, w), (-7, w)], type='curbstone')
    osm.way(1023, [(7, 0), (50, 0)], type='line_thin', subtype='solid')
    osm.way(1024, [(7, -w), (50, -w)], type='curbstone')
    osm.way(1025, [(7, w), (50, w)], type='curbstone')
    osm.way(1026, [(0, -50), (0, -7)], type='line_thin', subtype='solid')
    osm.way(1027, [(w, -50), (w, -7)], type='curbstone')
    osm.way(1028, [(-w, -50), (-w, -7)], type='curbstone')
    osm.way(1029, [(0, 7), (0, 50)], type='line_thin', subtype='solid')
    osm.way(1030, [(w, 7), (w, 50)], type='curbstone')
    osm.way(1031, [(-w, 7), (-w, 50)], type='curbstone')
    osm.lanelet(120, 1020, 1021)  # west arm, eastbound
    osm.lanelet(121, 1020, 1022)  # west arm, westbound
    osm.lanelet(122, 1023, 1025)  # east arm, westbound
    osm.lanelet(123, 1023, 1024)  # east arm, eastbound
    osm.lanelet(124, 1026, 1027)  # south arm, northbound
    osm.lanelet(125, 1026, 1028)  # south arm, southbound
    osm.lanelet(126, 1029, 1031)  # north arm, southbound
    osm.lanelet(127, 1029, 1030)  # north arm, northbound

    # connectors: virtual bounds between the end nodes of the arms
    def virtual(way_id, points):
        return osm.way(way_id, points, type='virtual')
    virtual(1040, [(-7, 0), (7, 0)])
    virtual(1041, [(-7, -w), (7, -w)])
    osm.lanelet(130, 1040, 1041)
    virtual(1042, arc(-7, 7, 7, -90, 0, 15))
    virtual(1043, arc(-7, 7, 10.5, -90, 0, 15))
    osm.lanelet(131, 1042, 1043)
    virtual(1044, arc(-7, -7, 7, 90, 0, 15))
    virtual(1045, arc(-7, -7, 3.5, 90, 0, 15))
    osm.lanelet(132, 1044, 1045)
    virtual(1046, [(0, -7), (0, 7)])
    virtual(1047, [(w, -7), (w, 7)])
    osm.lanelet(133, 1046, 1047)
    virtual(1048, arc(-7, -7, 7, 0, 90, 15))
    virtual(1049, arc(-7, -7, 10.5, 0, 90, 15))
    osm.lanelet(134, 1048, 1049)
    virtual(1050, arc(7, -7, 7, 180, 90, 15))
    virtual(1051, arc(7, -7, 3.5, 180, 90, 15))
    osm.lanelet(135, 1050, 1051)
    virtual(1052, [(7, w), (-7, w)])
    osm.lanelet(136, 1040, 1052)
    virtual(1053, [(-w, 7), (-w, -7)])
    osm.lanelet(137, 1046, 1053)

    # traffic light for lanelet 120: stop line 2 m before the box
    osm.way(1060, [(-9, -w), (-9, 0)], type='stop_line')
    osm.way(1061, [(8, -5, 5.0), (8, -4.2, 5.0)], type='traffic_light',
            subtype='red_yellow_green', height='1.2')
    osm.relation(210, [('way', 1061, 'refers'), ('way', 1060, 'ref_line')],
                 type='regulatory_element', subtype='traffic_light')
    osm.relations = [r for r in osm.relations if r[0] != 120]
    osm.lanelet(120, 1020, 1021, [('relation', 210, 'regulatory_element')])
    # traffic light for lanelet 124 without stop line: stop at its end
    osm.way(1062, [(5, 8, 5.0), (4.2, 8, 5.0)], type='traffic_light',
            subtype='red_yellow_green', height='1.2')
    osm.relation(211, [('way', 1062, 'refers')], type='regulatory_element',
                 subtype='traffic_light')
    osm.relations = [r for r in osm.relations if r[0] != 124]
    osm.lanelet(124, 1026, 1027, [('relation', 211, 'regulatory_element')])
    # stop sign for lanelet 122 (Autoware style), stop line at x = 9
    osm.way(1063, [(9, w), (9, 0)], type='stop_line')
    osm.way(1064, [(9, 4.5, 2.0), (9.6, 4.5, 2.0)], type='traffic_sign',
            subtype='stop_sign')
    osm.relation(212, [('way', 1064, 'refers'), ('way', 1063, 'ref_line')],
                 type='regulatory_element', subtype='traffic_sign')
    osm.relations = [r for r in osm.relations if r[0] != 122]
    osm.lanelet(122, 1023, 1025, [('relation', 212, 'regulatory_element')])
    # lanelet 126 yields to the west->east connector, line at y = 9
    osm.way(1065, [(-w, 9), (0, 9)], type='stop_line')
    osm.way(1066, [(-4.5, 9, 2.0), (-5.1, 9, 2.0)], type='traffic_sign',
            subtype='de205')
    osm.relation(213, [('way', 1066, 'refers'), ('way', 1065, 'ref_line'),
                       ('relation', 126, 'yield'),
                       ('relation', 130, 'right_of_way')],
                 type='regulatory_element', subtype='right_of_way')
    osm.relations = [r for r in osm.relations if r[0] != 126]
    osm.lanelet(126, 1029, 1031, [('relation', 213, 'regulatory_element')])

    # crosswalk across the east arm, walked towards +y
    osm.way(1070, [(20, -w), (20, w)], type='pedestrian_marking')
    osm.way(1071, [(23, -w), (23, w)], type='pedestrian_marking')
    osm.lanelet(140, 1070, 1071, subtype='crosswalk', one_way='no')
    osm.relations.sort(key=lambda r: r[0])
    return osm


def autoware_local():
    """An Autoware style map: no geo reference, coordinates in local_x /
    local_y, turn_direction, speed_limit and intersection_area tags.

    Lanelets 150 -> 151 -> 152 along +x (20 m each); 151 lies in the
    intersection area (way 1090) and is tagged as left turn. 153 is a two-way
    lanelet next to nothing, 154 a road shoulder right of 150. A stop line
    (road_marking) crosses 150 at x = 18.
    """
    osm = Osm(local_xy=True)
    for k, x in enumerate((0, 20, 40)):
        osm.way(1080 + 2 * k, [(x, 3.0, 12.5), (x + 20, 3.0, 12.5)],
                type='line_thin', subtype='solid')
        osm.way(1081 + 2 * k, [(x, 0.0, 12.5), (x + 20, 0.0, 12.5)],
                type='line_thin', subtype='solid')
    osm.way(1090, [(20, -1), (40, -1), (40, 4), (20, 4), (20, -1)],
            type='intersection_area', area='yes')
    osm.way(1091, [(18, 0), (18, 3)], type='stop_line')
    osm.relation(220, [('way', 1091, 'refers')], type='regulatory_element',
                 subtype='road_marking')
    osm.lanelet(150, 1080, 1081, [('relation', 220, 'regulatory_element')],
                speed_limit='30', one_way='yes', participant__vehicle='yes')
    osm.lanelet(151, 1082, 1083, speed_limit='20', one_way='yes',
                turn_direction='left', intersection_area='1090')
    osm.lanelet(152, 1084, 1085, speed_limit='30', one_way='yes')
    # a narrow two-way street
    osm.way(1092, [(0, 13), (30, 13)], type='road_border')
    osm.way(1093, [(0, 10), (30, 10)], type='road_border')
    osm.lanelet(153, 1092, 1093, speed_limit='15', one_way='no')
    # road shoulder right of lanelet 150
    osm.way(1094, [(0, -2.0, 12.5), (20, -2.0, 12.5)], type='road_border')
    osm.lanelet(154, 1081, 1094, subtype='road_shoulder')
    return osm


def malformed():
    """One good lanelet (160) and things the adapter has to survive."""
    osm = Osm()
    osm.way(1100, [(0, 3), (20, 3)], type='line_thin', subtype='solid')
    osm.way(1101, [(0, 0), (20, 0)], type='line_thin', subtype='solid')
    osm.lanelet(160, 1100, 1101)
    # no right bound
    osm.relation(161, [('way', 1100, 'left')], type='lanelet', subtype='road')
    # a bound with one point only
    osm.way(1102, [(0, 6)], type='line_thin')
    osm.lanelet(162, 1102, 1100)
    # a bound that does not exist
    osm.lanelet(163, 1100, 4711)
    # unknown line type and unknown lanelet subtype
    osm.way(1103, [(0, -3), (20, -3)], type='laser_fence')
    osm.lanelet(164, 1101, 1103)
    osm.way(1104, [(0, -6), (20, -6)], type='line_thin', subtype='solid')
    osm.lanelet(165, 1103, 1104, subtype='hovercraft_lane')
    # a relation that is no Lanelet2 primitive, a traffic light without light
    osm.relation(166, [('way', 1100, 'outer')], type='route')
    osm.relation(230, [('way', 1101, 'ref_line')], type='regulatory_element',
                 subtype='traffic_light')
    osm.relation(231, [], type='regulatory_element', subtype='teleporter')
    # a way with a node that is not in the file, a deleted lanelet
    osm.raw.append("<way id='1105' visible='true' version='1'>"
                   "<nd ref='1' /><nd ref='999999' />"
                   "<tag k='type' v='line_thin' /></way>")
    osm.raw.append("<relation id='167' action='delete' visible='true' "
                   "version='1'><member type='way' ref='1100' role='left' />"
                   "<member type='way' ref='1101' role='right' />"
                   "<tag k='type' v='lanelet' /></relation>")
    return osm


MAPS = (
    ('two_lane_road.osm', two_lane_road, ''),
    ('curve.osm', curve, ''),
    ('intersection.osm', intersection, ''),
    ('autoware_local.osm', autoware_local,
     "<MetaInfo format_version='1' map_version='7' />"),
    ('malformed.osm', malformed, ''),
)


def main():
    default_dir = os.path.normpath(os.path.join(
        HERE, '..', '..', '..', 'modules', 'map', 'hdmap', 'adapter',
        'lanelet2', 'testdata'))
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--output_dir', default=default_dir)
    args = parser.parse_args()
    os.makedirs(args.output_dir, exist_ok=True)
    for name, build, meta in MAPS:
        path = os.path.join(args.output_dir, name)
        build().write(path, meta)
        print('wrote %s' % path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
