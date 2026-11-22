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
"""Exports an Apollo base map as a Lanelet2 map (OSM XML).

  apollo_to_lanelet2.py --map modules/map/data/borregas_ave --output out.osm
  apollo_to_lanelet2.py --map modules/map/data/demo --utm_zone 10 \\
      --output demo.osm

Lanelet2 derives the lane topology from shared primitives, Apollo stores it
explicitly. The exporter therefore builds the primitives from the topology
of the Apollo map:

* lanes that are left/right neighbors use ONE way as their common bound,
* a lane and its successors share the end nodes of both bounds,

so that lanelet2's routing graph has an edge for every successor and every
allowed lane change of the Apollo map. Where the boundaries of neighbor
lanes do not coincide in the Apollo map (staggered lane ends), the shared
bound is the average of both and the report says how far bounds moved.

Every lanelet carries the Apollo lane id in the tag `apollo:id`; the Lanelet2
adapter of Apollo uses it as lane id again, so ids survive a round trip.
See docs/ros2/lanelet2.md.
"""

import argparse
import collections
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import lanelet2_common as common  # noqa: E402  pylint: disable=wrong-import-position

from modules.common_msgs.map_msgs import map_lane_pb2  # noqa: E402
from modules.common_msgs.map_msgs import map_road_pb2  # noqa: E402

BT = map_lane_pb2.LaneBoundaryType
LANE = map_lane_pb2.Lane
DOTTED = (BT.DOTTED_WHITE, BT.DOTTED_YELLOW)
YELLOW = (BT.DOTTED_YELLOW, BT.SOLID_YELLOW, BT.DOUBLE_YELLOW)
LEFT, RIGHT = 0, 1


def xml_escape(text):
    return (str(text).replace('&', '&amp;').replace('<', '&lt;')
            .replace('>', '&gt;').replace('"', '&quot;'))


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def average_polylines(lines):
    """Point-wise mean of polylines resampled at equal normalized arc
    length."""
    longest = max(common.polyline_length(line) for line in lines)
    count = max(max(len(line) for line in lines),
                int(math.ceil(longest)) + 1, 2)
    sampled = [common.resample(line, count) for line in lines]
    return [tuple(sum(s[i][k] for s in sampled) / len(sampled)
                  for k in range(3)) for i in range(count)]


def snap_end(points, target, at_end):
    """Moves the last (at_end) or first point of a polyline to `target`,
    dropping points the move would leave behind it."""
    line = list(points) if at_end else list(reversed(points))
    if dist(line[-1], target) > 1e-9:
        # arc length of the point of the line closest to the target
        best = (float('inf'), 0.0)
        travelled = 0.0
        for a, b in zip(line, line[1:]):
            seg = dist(a, b)
            t = 0.0
            if seg > 0.0:
                t = ((target[0] - a[0]) * (b[0] - a[0]) +
                     (target[1] - a[1]) * (b[1] - a[1])) / (seg * seg)
                t = max(0.0, min(1.0, t))
            d = math.hypot(a[0] + t * (b[0] - a[0]) - target[0],
                           a[1] + t * (b[1] - a[1]) - target[1])
            if d < best[0]:
                best = (d, travelled + t * seg)
            travelled += seg
        keep = [line[0]]
        travelled = 0.0
        for a, b in zip(line, line[1:]):
            travelled += dist(a, b)
            if travelled < best[1] - 0.05:
                keep.append(b)
        line = keep
        if dist(line[-1], target) < 1e-6 and len(line) > 1:
            line.pop()
        line.append(tuple(target))
    return line if at_end else list(reversed(line))


class ParityUnionFind(object):
    """Union-find whose elements have an orientation relative to their set."""

    def __init__(self, n):
        self.parent = list(range(n))
        self.flip = [0] * n

    def find(self, x):
        path = []
        while self.parent[x] != x:
            path.append(x)
            x = self.parent[x]
        root = x
        # path compression, keeping the parity to the root
        flips = 0
        for node in reversed(path):
            flips ^= self.flip[node]
            self.parent[node] = root
            self.flip[node] = flips
        return root

    def parity(self, x):
        self.find(x)
        return self.flip[x] if self.parent[x] != x else 0

    def union(self, a, b, opposite):
        ra, rb = self.find(a), self.find(b)
        if ra == rb:
            return (self.parity(a) ^ self.parity(b)) == opposite
        pa, pb = self.parity(a), self.parity(b)
        if rb < ra:
            ra, rb, pa, pb = rb, ra, pb, pa
        self.parent[rb] = ra
        self.flip[rb] = pa ^ pb ^ opposite
        return True


class Exporter(object):
    def __init__(self, hdmap, projection, args):
        self.map = hdmap
        self.projection = projection
        self.args = args
        self.report = collections.OrderedDict()
        self.warnings = []
        self.next_id = 0
        self.nodes = []      # (id, x, y, z, tags)
        self.ways = []       # (id, [node ids], tags)
        self.relations = []  # (id, [(type, ref, role)], tags)

    def warn(self, message):
        self.warnings.append(message)

    def new_id(self):
        self.next_id += 1
        return self.next_id

    def add_node(self, point, tags=None):
        node_id = self.new_id()
        self.nodes.append((node_id, point[0], point[1],
                           point[2] if len(point) > 2 else 0.0, tags or {}))
        return node_id

    def add_way(self, points_or_ids, tags, closed=False):
        ids = [p if isinstance(p, int) else self.add_node(p)
               for p in points_or_ids]
        if closed:
            ids.append(ids[0])
        way_id = self.new_id()
        self.ways.append((way_id, ids, tags))
        return way_id

    def add_relation(self, members, tags):
        relation_id = self.new_id()
        self.relations.append((relation_id, members, tags))
        return relation_id

    ###########################################################################

    def junction_lanes(self):
        """lane id -> junction id for lanes that lie inside a junction."""
        result = {}
        junction_ids = {j.id.id for j in self.map.junction}
        for road in self.map.road:
            if road.HasField('junction_id') and \
                    road.junction_id.id in junction_ids:
                for section in road.section:
                    for lane_id in section.lane_id:
                        result[lane_id.id] = road.junction_id.id
        lanes = {lane.id.id: lane for lane in self.map.lane}
        for lane in self.map.lane:
            if lane.HasField('junction_id') and \
                    lane.junction_id.id in junction_ids:
                result[lane.id.id] = lane.junction_id.id
        for overlap in self.map.overlap:
            lane_info = [o for o in overlap.object
                         if o.HasField('lane_overlap_info')]
            junction_info = [o for o in overlap.object
                             if o.HasField('junction_overlap_info')]
            if len(lane_info) == 1 and len(junction_info) == 1 and \
                    lane_info[0].id.id in lanes and \
                    junction_info[0].id.id in junction_ids:
                lane = lanes[lane_info[0].id.id]
                info = lane_info[0].lane_overlap_info
                if lane.id.id not in result and lane.length > 0 and \
                        (info.end_s - info.start_s) >= 0.8 * lane.length:
                    result[lane.id.id] = junction_info[0].id.id
        return result

    @staticmethod
    def boundary_type(boundary):
        """The boundary type that covers most of a boundary."""
        entries = [(bt.s, list(bt.types)) for bt in boundary.boundary_type
                   if bt.types]
        if not entries:
            return BT.UNKNOWN, False
        entries.sort()
        best, best_length = entries[0][1][0], -1.0
        for i, (s, types) in enumerate(entries):
            end = entries[i + 1][0] if i + 1 < len(entries) else \
                max(boundary.length, s)
            if end - s > best_length:
                best, best_length = types[0], end - s
        return best, len({tuple(t) for _, t in entries}) > 1

    def build_lanes(self):
        lanes = list(self.map.lane)
        index = {lane.id.id: i for i, lane in enumerate(lanes)}
        in_junction = self.junction_lanes()
        bounds = {}
        for i, lane in enumerate(lanes):
            left = common.curve_points(lane.left_boundary.curve)
            right = common.curve_points(lane.right_boundary.curve)
            if len(left) < 2 or len(right) < 2:
                self.warn('lane %s has no boundary geometry and is skipped' %
                          lane.id.id)
                continue
            bounds[(i, LEFT)] = left
            bounds[(i, RIGHT)] = right
        usable = {i for i in range(len(lanes)) if (i, LEFT) in bounds}

        # 1. Lateral sharing: one way for the common bound of neighbors.
        slots = ParityUnionFind(2 * len(lanes))
        shared = 0
        rejected = 0
        self.unshared_neighbors = []

        def share(a, side_a, b, side_b, opposite, what):
            nonlocal shared, rejected
            line_a = bounds[(a, side_a)]
            line_b = bounds[(b, side_b)]
            if opposite:
                line_b = list(reversed(line_b))
            # neighbors whose boundaries are far apart cannot share one
            gaps = sorted(common.distance_to_polyline(p, line_b)
                          for p in common.resample(line_a, 11))
            limit = self.args.max_reverse_neighbor_gap if opposite else \
                self.args.max_neighbor_gap
            if gaps[len(gaps) // 2] > limit:
                rejected += 1
                self.unshared_neighbors.append(
                    [lanes[a].id.id, lanes[b].id.id, what])
                self.warn('%s %s/%s: boundaries are %.2f m apart, no shared '
                          'bound' % (what, lanes[a].id.id, lanes[b].id.id,
                                     gaps[len(gaps) // 2]))
                return
            if not slots.union(2 * a + side_a, 2 * b + side_b,
                               1 if opposite else 0):
                rejected += 1
                self.warn('%s %s/%s contradicts other neighbor relations' %
                          (what, lanes[a].id.id, lanes[b].id.id))
                return
            shared += 1

        for i in sorted(usable):
            lane = lanes[i]
            for other in lane.left_neighbor_forward_lane_id:
                j = index.get(other.id)
                if j in usable:
                    share(i, LEFT, j, RIGHT, False, 'left neighbor')
            for other in lane.right_neighbor_forward_lane_id:
                j = index.get(other.id)
                if j in usable:
                    share(i, RIGHT, j, LEFT, False, 'right neighbor')
            for other in lane.left_neighbor_reverse_lane_id:
                j = index.get(other.id)
                if j in usable:
                    share(i, LEFT, j, LEFT, True, 'left reverse neighbor')
            for other in lane.right_neighbor_reverse_lane_id:
                j = index.get(other.id)
                if j in usable:
                    share(i, RIGHT, j, RIGHT, True, 'right reverse neighbor')
            for other in lane.self_reverse_lane_id:
                j = index.get(other.id)
                if j in usable:
                    share(i, LEFT, j, RIGHT, True, 'self reverse lane')
                    share(i, RIGHT, j, LEFT, True, 'self reverse lane')

        groups = collections.OrderedDict()
        for i in sorted(usable):
            for side in (LEFT, RIGHT):
                slot = 2 * i + side
                groups.setdefault(slots.find(slot), []).append(
                    (i, side, slots.parity(slot)))

        ways = []          # dict(points, members)
        slot_way = {}      # (lane, side) -> (way index, flipped)
        averaged = 0
        for members in groups.values():
            lines = []
            for lane, side, flipped in members:
                line = bounds[(lane, side)]
                lines.append(list(reversed(line)) if flipped else line)
            points = max(lines, key=len)
            if len(lines) > 1 and max(common.max_deviation(points, line)
                                      for line in lines) > \
                    self.args.merge_tolerance:
                points = average_polylines(lines)
                averaged += 1
            for lane, side, flipped in members:
                slot_way[(lane, side)] = (len(ways), flipped)
            ways.append({'points': list(points), 'members': members})

        # 2. Longitudinal sharing: a lane and its successors share the end
        # nodes of both bounds.
        def endpoint(lane, side, at_end):
            way, flipped = slot_way[(lane, side)]
            return (way, 1 if at_end != bool(flipped) else 0)

        parent = {}
        members_of = {}
        enemies_of = {}

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for w in range(len(ways)):
            for end in (0, 1):
                parent[(w, end)] = (w, end)
                members_of[(w, end)] = {(w, end)}
                enemies_of[(w, end)] = {(w, 1 - end)}
        for i in sorted(usable):
            for at_end in (False, True):
                a = endpoint(i, LEFT, at_end)
                b = endpoint(i, RIGHT, at_end)
                enemies_of[a].add(b)
                enemies_of[b].add(a)

        def join(a, b):
            ra, rb = find(a), find(b)
            if ra == rb:
                return True
            if members_of[ra] & enemies_of[rb] or \
                    members_of[rb] & enemies_of[ra]:
                return False
            if len(members_of[ra]) < len(members_of[rb]):
                ra, rb = rb, ra
            parent[rb] = ra
            members_of[ra] |= members_of.pop(rb)
            enemies_of[ra] |= enemies_of.pop(rb)
            return True

        # Where one lane continues into two neighbor lanes, Lanelet2 can
        # express only one of them as follower (the other one would have no
        # width at its start): the links that fit best are made first.
        links = []
        for i in sorted(usable):
            for successor in lanes[i].successor_id:
                j = index.get(successor.id)
                if j not in usable:
                    continue
                gap = max(dist(bounds[(i, LEFT)][-1], bounds[(j, LEFT)][0]),
                          dist(bounds[(i, RIGHT)][-1], bounds[(j, RIGHT)][0]))
                links.append((gap, len(links), i, j))
        def can_join_both(left, right):
            """Would joining the left and then the right end points keep
            every lanelet and way from collapsing?"""
            la, lb = find(left[0]), find(left[1])
            if la != lb and (members_of[la] & enemies_of[lb] or
                             members_of[lb] & enemies_of[la]):
                return False

            def after_left(x):  # class of x once the left ends are joined
                root = find(x)
                if root in (la, lb):
                    return (members_of[la] | members_of[lb],
                            enemies_of[la] | enemies_of[lb])
                return members_of[root], enemies_of[root]
            ra, rb = find(right[0]), find(right[1])
            if ra == rb or {ra, rb} <= {la, lb}:
                return True
            members_a, enemies_a = after_left(right[0])
            members_b, enemies_b = after_left(right[1])
            return not (members_a & enemies_b or members_b & enemies_a)

        connected = 0
        self.unexpressed_successors = []
        for _, _, i, j in sorted(links):
            left = (endpoint(i, LEFT, True), endpoint(j, LEFT, False))
            right = (endpoint(i, RIGHT, True), endpoint(j, RIGHT, False))
            # both bounds or none
            if can_join_both(left, right):
                join(*left)
                join(*right)
                connected += 1
                continue
            self.unexpressed_successors.append(
                [lanes[i].id.id, lanes[j].id.id])
            self.warn('successor %s -> %s cannot be a lanelet2 follower: '
                      'both lanelets would collapse to a line (lane split '
                      'into neighbor lanes)' %
                      (lanes[i].id.id, lanes[j].id.id))

        # Position of the shared end nodes: lanes outside junctions keep
        # their geometry, junction lanes (virtual in Apollo) follow.
        def way_in_junction(way):
            return all(lanes[lane].id.id in in_junction
                       for lane, _, _ in ways[way]['members'])

        roots = collections.OrderedDict()
        for w in range(len(ways)):
            for end in (0, 1):
                roots.setdefault(find((w, end)), []).append((w, end))
        end_position = {}
        for root, group in roots.items():
            preferred = [e for e in group if not way_in_junction(e[0])]
            used = preferred or group
            position = tuple(
                sum(ways[w]['points'][-1 if end else 0][k] for w, end in used)
                / len(used) for k in range(3))
            for e in group:
                end_position[e] = (root, position)

        # 3. Final geometry and how far it is from the Apollo boundaries.
        for w, way in enumerate(ways):
            points = snap_end(way['points'], end_position[(w, 0)][1], False)
            points = snap_end(points, end_position[(w, 1)][1], True)
            if len(points) < 2 or dist(points[0], points[-1]) < 1e-6 and \
                    len(points) == 2:
                points = [end_position[(w, 0)][1], end_position[(w, 1)][1]]
            way['points'] = points
        moved = {}
        for (lane, side), (w, flipped) in slot_way.items():
            line = ways[w]['points']
            if flipped:
                line = list(reversed(line))
            moved[(lane, side)] = common.max_deviation(bounds[(lane, side)],
                                                       line)

        self.lanes = lanes
        self.lane_index = index
        self.usable = usable
        self.in_junction = in_junction
        self.bound_ways = ways
        self.slot_way = slot_way
        self.end_node_root = {e: v[0] for e, v in end_position.items()}
        self.bound_moved = moved
        self.report['lanes'] = len(lanes)
        self.report['lanes_exported'] = len(usable)
        self.report['bound_ways'] = len(ways)
        self.report['neighbor_bounds_shared'] = shared
        self.report['neighbor_bounds_not_shared'] = rejected
        self.report['shared_bounds_averaged'] = averaged
        self.report['successor_links'] = connected
        self.report['successor_links_not_expressed'] = \
            len(self.unexpressed_successors)
        shifts = sorted(moved.values())
        self.report['bound_shift_max_m'] = round(shifts[-1], 4) if shifts else 0
        self.report['bounds_shifted_over_0.1m'] = sum(1 for s in shifts
                                                       if s > 0.1)
        self.report['bounds_shifted_over_1m'] = sum(1 for s in shifts if s > 1)

    def way_tags(self, way):
        """Lanelet2 line type from the Apollo boundary types on both sides."""
        sides = {LEFT: [], RIGHT: []}  # side of the way the lane lies on
        virtual = False
        mixed = False
        for lane, side, flipped in way['members']:
            boundary = self.lanes[lane].left_boundary if side == LEFT else \
                self.lanes[lane].right_boundary
            kind, changes = self.boundary_type(boundary)
            mixed |= changes
            virtual |= boundary.virtual
            # a lane is right of its left bound and left of its right bound
            lies = RIGHT if side == LEFT else LEFT
            if flipped:
                lies = 1 - lies
            sides[lies].append(kind)
        if mixed:
            self.report['bounds_with_changing_type'] = \
                self.report.get('bounds_with_changing_type', 0) + 1
        kinds = sides[LEFT] + sides[RIGHT]
        tags = collections.OrderedDict()
        left_dotted = bool(sides[LEFT]) and all(k in DOTTED
                                                for k in sides[LEFT])
        right_dotted = bool(sides[RIGHT]) and all(k in DOTTED
                                                  for k in sides[RIGHT])
        one_sided = not sides[LEFT] or not sides[RIGHT]
        if BT.CURB in kinds:
            tags['type'] = 'curbstone'
        elif (left_dotted and right_dotted) or \
                (one_sided and (left_dotted or right_dotted)):
            tags['type'] = 'virtual' if virtual else 'line_thin'
            if virtual:
                tags['lane_change'] = 'yes'
            else:
                tags['subtype'] = 'dashed'
        elif left_dotted or right_dotted:
            # dashed_solid: dashed for the lane on the left of the way
            tags['type'] = 'line_thin'
            tags['subtype'] = 'dashed_solid' if left_dotted else 'solid_dashed'
        elif all(k == BT.UNKNOWN for k in kinds) or virtual:
            tags['type'] = 'virtual'
        else:
            tags['type'] = 'line_thin'
            tags['subtype'] = 'solid_solid' if BT.DOUBLE_YELLOW in kinds \
                else 'solid'
        if tags['type'] == 'line_thin' and any(k in YELLOW for k in kinds):
            tags['color'] = 'yellow'
        return tags

    def export_lanes(self):
        # nodes: one per shared end node, own nodes inside every way
        end_nodes = {}
        way_ids = []
        for w, way in enumerate(self.bound_ways):
            points = way['points']
            ids = []
            for k, point in enumerate(points):
                if k == 0 or k == len(points) - 1:
                    root = self.end_node_root[(w, 0 if k == 0 else 1)]
                    if root not in end_nodes:
                        end_nodes[root] = self.add_node(point)
                    ids.append(end_nodes[root])
                else:
                    ids.append(self.add_node(point))
            way_ids.append(self.add_way(ids, self.way_tags(way)))

        highway_lanes = set()
        for road in self.map.road:
            if road.type == map_road_pb2.Road.HIGHWAY:
                for section in road.section:
                    highway_lanes |= {i.id for i in section.lane_id}

        self.lanelet_ids = {}
        self.lanelet_members = {}
        self.lanelet_tags = {}
        for i in sorted(self.usable):
            lane = self.lanes[i]
            members = [('way', way_ids[self.slot_way[(i, LEFT)][0]], 'left'),
                       ('way', way_ids[self.slot_way[(i, RIGHT)][0]],
                        'right')]
            if self.args.centerline == 'explicit':
                center = common.curve_points(lane.central_curve)
                if len(center) >= 2:
                    members.append(('way', self.add_way(center, {}),
                                    'centerline'))
            tags = collections.OrderedDict()
            tags['type'] = 'lanelet'
            if lane.type == LANE.BIKING:
                tags['subtype'] = 'bicycle_lane'
            elif lane.type == LANE.SIDEWALK:
                tags['subtype'] = 'walkway'
            elif lane.type == LANE.SHOULDER:
                tags['subtype'] = 'road_shoulder'
            else:
                tags['subtype'] = 'highway' if lane.id.id in highway_lanes \
                    else 'road'
                if lane.type not in (LANE.CITY_DRIVING, LANE.SHARED):
                    # NONE / PARKING: nothing lanelet2 could drive on
                    tags['participant:vehicle'] = 'no'
                    self.warn('lane %s of type %s is exported as a lanelet '
                              'no participant may use' %
                              (lane.id.id, LANE.LaneType.Name(lane.type)))
            tags['location'] = self.args.location
            tags['one_way'] = 'no' if lane.direction == LANE.BIDIRECTION \
                else 'yes'
            if lane.speed_limit > 0.0:
                tags['speed_limit'] = '%.6g' % (lane.speed_limit * 3.6)
            if lane.turn == LANE.LEFT_TURN:
                tags['turn_direction'] = 'left'
            elif lane.turn == LANE.RIGHT_TURN:
                tags['turn_direction'] = 'right'
            elif lane.turn == LANE.U_TURN:
                # turn_direction has no u-turn; Apollo reads apollo:turn back
                tags['turn_direction'] = 'left'
                tags['apollo:turn'] = 'U_TURN'
            elif lane.id.id in self.in_junction:
                tags['turn_direction'] = 'straight'
            tags['apollo:id'] = lane.id.id
            self.lanelet_members[lane.id.id] = members
            self.lanelet_tags[lane.id.id] = tags

    ###########################################################################

    def lanes_overlapping(self, object_id):
        """[(lane id, start_s)] of the lane overlaps of a map object."""
        result = []
        for overlap in self.map.overlap:
            ids = [o.id.id for o in overlap.object]
            if object_id not in ids:
                continue
            for o in overlap.object:
                if o.HasField('lane_overlap_info') and \
                        o.id.id in self.lanelet_members:
                    result.append((o.id.id, o.lane_overlap_info.start_s))
        return result

    def stop_line_groups(self, object_id, stop_lines):
        """Splits the lanes of a signal/sign by the stop line they stop at:
        [(stop line points, [lane ids])]."""
        lines = [common.curve_points(curve) for curve in stop_lines]
        lines = [line for line in lines if len(line) >= 2]
        groups = [(line, []) for line in lines]
        for lane_id, start_s in self.lanes_overlapping(object_id):
            if not groups:
                continue
            lane = self.lanes[self.lane_index[lane_id]]
            point = common.point_at(common.curve_points(lane.central_curve),
                                    start_s)
            nearest = min(range(len(lines)), key=lambda k:
                          common.distance_to_polyline(point, lines[k]))
            if lane_id not in groups[nearest][1]:
                groups[nearest][1].append(lane_id)
        return [g for g in groups if g[1]]

    def export_signals(self):
        count = 0
        for signal in self.map.signal:
            corners = [(p.x, p.y, p.z) for p in signal.boundary.point]
            distinct = []
            for c in corners:
                if all(dist(c, d) > 1e-3 for d in distinct):
                    distinct.append(c)
            groups = self.stop_line_groups(signal.id.id, signal.stop_line)
            if not groups:
                self.warn('signal %s controls no exported lane and is '
                          'skipped' % signal.id.id)
                continue
            if len(distinct) >= 2:
                bottom = min(c[2] for c in corners)
                height = max(c[2] for c in corners) - bottom
                # the two posts of the light, left to right as in the map
                a, b = distinct[0], max(distinct,
                                        key=lambda d: dist(d, distinct[0]))
                light = [(a[0], a[1], bottom), (b[0], b[1], bottom)]
            else:
                middle = groups[0][0][len(groups[0][0]) // 2]
                bottom, height = middle[2] + 5.0, 1.0
                light = [(middle[0] - 0.2, middle[1], bottom),
                         (middle[0] + 0.2, middle[1], bottom)]
                self.warn('signal %s has no boundary; a placeholder light is '
                          'written above its stop line' % signal.id.id)
            bulbs = len(signal.subsignal)
            tags = collections.OrderedDict(type='traffic_light')
            if bulbs >= 3:
                tags['subtype'] = 'red_yellow_green'
            elif bulbs == 2:
                tags['subtype'] = 'red_green'
            elif bulbs == 1:
                tags['subtype'] = 'red'
            if height > 0.0:
                tags['height'] = '%.4g' % height
            tags['apollo:id'] = signal.id.id
            light_way = self.add_way(
                [self.add_node(p, {'ele': '%.4f' % p[2]}) for p in light],
                tags)
            for line, lane_ids in groups:
                stop_way = self.add_way(line, {'type': 'stop_line'})
                relation = self.add_relation(
                    [('way', light_way, 'refers'),
                     ('way', stop_way, 'ref_line')],
                    collections.OrderedDict(type='regulatory_element',
                                            subtype='traffic_light'))
                for lane_id in lane_ids:
                    self.lanelet_members[lane_id].append(
                        ('relation', relation, 'regulatory_element'))
            count += 1
        self.report['signals_exported'] = count

    def export_signs(self, signs, sign_subtype, name):
        count = 0
        for sign in signs:
            groups = self.stop_line_groups(sign.id.id, sign.stop_line)
            if not groups:
                self.warn('%s %s controls no exported lane and is skipped' %
                          (name, sign.id.id))
                continue
            for k, (line, lane_ids) in enumerate(groups):
                # Apollo does not know where the sign stands: put a small
                # one 2 m above the end of its stop line.
                a, b = line[-2], line[-1]
                length = dist(a, b) or 1.0
                ux, uy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
                plate = [(b[0] + ux * 0.5, b[1] + uy * 0.5, b[2] + 2.0),
                         (b[0] + ux * 1.1, b[1] + uy * 1.1, b[2] + 2.0)]
                sign_way = self.add_way(
                    [self.add_node(p, {'ele': '%.4f' % p[2]}) for p in plate],
                    collections.OrderedDict(
                        [('type', 'traffic_sign'), ('subtype', sign_subtype),
                         ('apollo:synthetic', 'yes')]))
                stop_way = self.add_way(line, {'type': 'stop_line'})
                tags = collections.OrderedDict(type='regulatory_element',
                                               subtype='traffic_sign')
                if k == 0:
                    tags['apollo:id'] = sign.id.id
                relation = self.add_relation(
                    [('way', sign_way, 'refers'),
                     ('way', stop_way, 'ref_line')], tags)
                for lane_id in lane_ids:
                    self.lanelet_members[lane_id].append(
                        ('relation', relation, 'regulatory_element'))
            if len(groups) > 1:
                self.warn('%s %s has %d stop lines: only the first keeps the '
                          'id on a round trip' %
                          (name, sign.id.id, len(groups)))
            count += 1
        return count

    def export_crosswalks(self):
        count = 0
        for crosswalk in self.map.crosswalk:
            ring = [(p.x, p.y, p.z) for p in crosswalk.polygon.point]
            while len(ring) > 1 and dist(ring[0], ring[-1]) < 1e-6:
                ring.pop()
            if len(ring) < 3:
                self.warn('crosswalk %s has no area and is skipped' %
                          crosswalk.id.id)
                continue
            n = len(ring)
            nodes = [self.add_node(p) for p in ring]
            if n == 4:
                # the two longer opposite edges are the bounds
                e = [dist(ring[k], ring[(k + 1) % 4]) for k in range(4)]
                k = 0 if e[0] + e[2] >= e[1] + e[3] else 1
                left = [nodes[k], nodes[(k + 1) % 4]]
                right = [nodes[(k + 3) % 4], nodes[(k + 2) % 4]]
            else:
                # split the outline at its two most distant corners
                i, j = max(((a, b) for a in range(n) for b in range(a + 1, n)),
                           key=lambda ab: dist(ring[ab[0]], ring[ab[1]]))
                left = nodes[i:j + 1]
                right = (nodes[j:] + nodes[:i + 1])[::-1]
            marking = {'type': 'pedestrian_marking'}
            left_way = self.add_way(left, dict(marking))
            right_way = self.add_way(right, dict(marking))
            self.add_relation(
                [('way', left_way, 'left'), ('way', right_way, 'right')],
                collections.OrderedDict(
                    [('type', 'lanelet'), ('subtype', 'crosswalk'),
                     ('location', self.args.location), ('one_way', 'no'),
                     ('apollo:id', crosswalk.id.id)]))
            count += 1
        self.report['crosswalks_exported'] = count

    def export_junctions(self):
        count = 0
        for junction in self.map.junction:
            ring = [(p.x, p.y, p.z) for p in junction.polygon.point]
            while len(ring) > 1 and dist(ring[0], ring[-1]) < 1e-6:
                ring.pop()
            if len(ring) < 3:
                self.warn('junction %s has no area and is skipped' %
                          junction.id.id)
                continue
            way = self.add_way(ring, collections.OrderedDict(
                [('type', 'intersection_area'), ('area', 'yes'),
                 ('apollo:id', junction.id.id)]), closed=True)
            for lane_id, junction_id in self.in_junction.items():
                if junction_id == junction.id.id and \
                        lane_id in self.lanelet_tags:
                    self.lanelet_tags[lane_id]['intersection_area'] = str(way)
            count += 1
        self.report['junctions_exported'] = count

    def run(self):
        self.build_lanes()
        self.export_lanes()
        self.export_signals()
        self.report['stop_signs_exported'] = self.export_signs(
            self.map.stop_sign, 'stop_sign', 'stop sign')
        self.report['yield_signs_exported'] = self.export_signs(
            getattr(self.map, 'yield'), 'de205', 'yield sign')
        self.export_crosswalks()
        self.export_junctions()
        for lane_id, members in self.lanelet_members.items():
            self.lanelet_ids[lane_id] = self.add_relation(
                members, self.lanelet_tags[lane_id])
        for name in ('clear_area', 'speed_bump', 'parking_space',
                     'pnc_junction', 'rsu', 'ad_area', 'barrier_gate'):
            n = len(getattr(self.map, name))
            if n:
                self.report['%s_not_exported' % name] = n
                self.warn('%d %s object(s) have no Lanelet2 counterpart and '
                          'are not exported' % (n, name))
        self.report['nodes'] = len(self.nodes)
        self.report['ways'] = len(self.ways)
        self.report['relations'] = len(self.relations)
        self.report['warnings'] = len(self.warnings)

    def write(self, path):
        latlon = self.projection.inverse_many(
            [(x, y) for _, x, y, _, _ in self.nodes])
        with open(path, 'w') as f:
            f.write("<?xml version='1.0' encoding='UTF-8'?>\n")
            f.write("<osm version='0.6' generator='apollo_to_lanelet2'>\n")
            for (node_id, x, y, z, tags), (lat, lon) in zip(self.nodes,
                                                            latlon):
                tags = collections.OrderedDict(tags)
                if z != 0.0 and 'ele' not in tags:
                    tags['ele'] = '%.4f' % z
                if self.args.local_xy:
                    tags['local_x'] = '%.4f' % x
                    tags['local_y'] = '%.4f' % y
                head = "  <node id='%d' visible='true' version='1' " \
                    "lat='%.11f' lon='%.11f'" % (node_id, lat, lon)
                if tags:
                    f.write(head + '>\n')
                    for k, v in tags.items():
                        f.write('    <tag k="%s" v="%s" />\n' %
                                (xml_escape(k), xml_escape(v)))
                    f.write('  </node>\n')
                else:
                    f.write(head + ' />\n')
            for way_id, ids, tags in self.ways:
                f.write("  <way id='%d' visible='true' version='1'>\n" %
                        way_id)
                for node_id in ids:
                    f.write("    <nd ref='%d' />\n" % node_id)
                for k, v in tags.items():
                    f.write('    <tag k="%s" v="%s" />\n' %
                            (xml_escape(k), xml_escape(v)))
                f.write('  </way>\n')
            for relation_id, members, tags in self.relations:
                f.write("  <relation id='%d' visible='true' version='1'>\n" %
                        relation_id)
                for kind, ref, role in members:
                    f.write("    <member type='%s' ref='%d' role='%s' />\n" %
                            (kind, ref, role))
                for k, v in tags.items():
                    f.write('    <tag k="%s" v="%s" />\n' %
                            (xml_escape(k), xml_escape(v)))
                f.write('  </relation>\n')
            f.write('</osm>\n')


def pick_projection(hdmap, args):
    if args.proj:
        return args.proj
    if args.utm_zone:
        return common.utm_proj_string(args.utm_zone, args.south)
    proj = hdmap.header.projection.proj
    if common.usable_proj_string(proj):
        return proj
    raise SystemExit(
        'The map header has no usable projection ("%s"). Lanelet2 needs '
        'latitude/longitude: pass --utm_zone N (or --proj "+proj=...") for '
        'the frame the map coordinates are in.' % proj)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--map', required=True,
                        help='Apollo map directory or base map file')
    parser.add_argument('--output', required=True, help='.osm file to write')
    parser.add_argument('--proj', default='',
                        help='PROJ string of the map frame (default: the '
                        'map header)')
    parser.add_argument('--utm_zone', type=int, default=0,
                        help='shortcut for --proj "+proj=utm +zone=N ..."')
    parser.add_argument('--south', action='store_true',
                        help='with --utm_zone: southern hemisphere')
    parser.add_argument('--centerline', choices=['explicit', 'none'],
                        default='explicit',
                        help='explicit: write the Apollo central curve as '
                        'centerline of each lanelet (default); none: let '
                        'Lanelet2 compute centerlines from the bounds')
    parser.add_argument('--location', default='urban',
                        help='location tag of the lanelets')
    parser.add_argument('--local_xy', action='store_true',
                        help='also write the map coordinates as local_x / '
                        'local_y node tags')
    parser.add_argument('--merge_tolerance', type=float, default=0.05,
                        help='boundaries of neighbor lanes closer than this '
                        'are the same line (m)')
    parser.add_argument('--max_neighbor_gap', type=float, default=3.0,
                        help='same-direction neighbor lanes whose boundaries '
                        'are further apart do not share a bound (m)')
    parser.add_argument('--max_reverse_neighbor_gap', type=float, default=1.0,
                        help='oncoming neighbor lanes whose boundaries are '
                        'further apart do not share a bound (m)')
    parser.add_argument('--report', default='',
                        help='write the export report here (JSON)')
    args = parser.parse_args()

    hdmap = common.load_apollo_map(args.map)
    proj = pick_projection(hdmap, args)
    exporter = Exporter(hdmap, common.Projection(proj), args)
    exporter.run()
    exporter.write(args.output)
    exporter.report['projection'] = proj
    print('wrote %s' % args.output)
    for key, value in exporter.report.items():
        print('  %-34s %s' % (key, value))
    for message in exporter.warnings[:20]:
        print('  warning: ' + message)
    if len(exporter.warnings) > 20:
        print('  ... %d more warnings' % (len(exporter.warnings) - 20))
    if args.report:
        with open(args.report, 'w') as f:
            shifts = collections.OrderedDict()
            for (lane, _), shift in sorted(exporter.bound_moved.items()):
                name = exporter.lanes[lane].id.id
                if shift > 0.01:
                    shifts[name] = round(max(shift, shifts.get(name, 0.0)), 4)
            json.dump({'report': exporter.report,
                       # what Lanelet2 cannot express, see the module docs
                       'unexpressed_successors':
                           exporter.unexpressed_successors,
                       'unshared_neighbors': exporter.unshared_neighbors,
                       'bound_shift_m': shifts,
                       'warnings': exporter.warnings}, f, indent=2)
    return 0


if __name__ == '__main__':
    sys.exit(main())
