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
"""Cross-checks Apollo's Lanelet2 support against the real Lanelet2 library.

Needs the `lanelet2` Python package (pip install lanelet2). It is a
validation tool only; nothing in Apollo depends on that package.

  check_with_lanelet2.py import --osm map.osm --apollo_map <dir|file>
      The .osm was converted to an Apollo map by the Lanelet2 adapter. Loads
      the .osm with Lanelet2 and compares, lanelet by lanelet: which lanelets
      are drivable, one-way handling, bound geometry, centerline, speed
      limit, successors, lane changes, conflicts and stop positions.

  check_with_lanelet2.py export --osm map.osm --apollo_map <dir|file>
      The .osm was exported from an Apollo map by apollo_to_lanelet2.py.
      Loads it with Lanelet2 and checks that every lane is a lanelet and that
      every successor / lane change of the Apollo map is an edge of the
      Lanelet2 routing graph.

Exit code 0 when no mismatch is found.
"""

import argparse
import collections
import json
import math
import os
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import lanelet2_common as common  # noqa: E402  pylint: disable=wrong-import-position

from modules.common_msgs.map_msgs import map_lane_pb2  # noqa: E402


def import_lanelet2():
    try:
        import lanelet2  # pylint: disable=import-outside-toplevel
        import lanelet2.core  # noqa  pylint: disable=import-outside-toplevel
        import lanelet2.geometry  # noqa  pylint: disable=import-outside-toplevel
        import lanelet2.io  # noqa  pylint: disable=import-outside-toplevel
        import lanelet2.projection  # noqa  pylint: disable=import-outside-toplevel
        import lanelet2.routing  # noqa  pylint: disable=import-outside-toplevel
        import lanelet2.traffic_rules  # noqa  pylint: disable=import-outside-toplevel
    except ImportError as e:
        print('the lanelet2 Python package is required (pip install lanelet2): '
              '%s' % e, file=sys.stderr)
        sys.exit(3)
    return lanelet2


def first_node(osm_path):
    for _, element in ET.iterparse(osm_path, events=('start',)):
        if element.tag == 'node' and element.get('action') != 'delete':
            return float(element.get('lat')), float(element.get('lon'))
    raise ValueError('no node in %s' % osm_path)


class Projector(object):
    """A lanelet2 projector that reproduces the frame of an Apollo map."""

    def __init__(self, ll2, osm_path, apollo_map):
        self.ll2 = ll2
        self.dx = 0.0
        self.dy = 0.0
        lat, lon = first_node(osm_path)
        proj = apollo_map.header.projection.proj
        params = dict(t[1:].partition('=')[::2] for t in proj.split()
                      if t.startswith('+'))
        if params.get('proj') == 'utm':
            zone = int(params['zone'])
            lon = zone * 6 - 183.0
        elif params.get('proj') == 'tmerc' and \
                abs(float(params.get('k', 1)) - 0.9996) < 1e-12 and \
                float(params.get('lat_0', 0)) == 0.0:
            # UTM with a shifted origin, see Lanelet2Projector::Init().
            lon = float(params['lon_0'])
            south = lat < 0
            self.dx = float(params['x_0']) - 500000.0
            self.dy = float(params['y_0']) - (10000000.0 if south else 0.0)
        else:
            raise ValueError('cannot build a lanelet2 projector for "%s"' %
                             proj)
        origin = ll2.io.Origin(lat, lon)
        self.projector = ll2.projection.UtmProjector(origin, False, False)

    def xy(self, point):
        return point.x + self.dx, point.y + self.dy


def lane_table(apollo_map):
    return {lane.id.id: lane for lane in apollo_map.lane}


def boundary_is_dotted(boundary):
    types = [t for bt in boundary.boundary_type for t in bt.types]
    return bool(types) and all(
        t in (map_lane_pb2.LaneBoundaryType.DOTTED_WHITE,
              map_lane_pb2.LaneBoundaryType.DOTTED_YELLOW) for t in types)


class Result(object):
    def __init__(self):
        self.stats = collections.OrderedDict()
        self.mismatches = collections.OrderedDict()
        # differences that are expected and do not fail the check
        self.notes = collections.OrderedDict()

    def note(self, topic, message):
        self.notes.setdefault(topic, []).append(message)

    def stat(self, name, value):
        self.stats[name] = value

    def mismatch(self, check, message):
        self.mismatches.setdefault(check, []).append(message)

    def ok(self, check):
        self.mismatches.setdefault(check, [])

    def total(self):
        return sum(len(v) for v in self.mismatches.values())

    def report(self, max_lines=8):
        print('statistics:')
        for name, value in self.stats.items():
            if isinstance(value, float):
                value = '%.6g' % value
            print('  %-46s %s' % (name, value))
        print('checks:')
        for check, messages in self.mismatches.items():
            print('  %-46s %s' % (check, 'OK' if not messages else
                                  '%d MISMATCH(ES)' % len(messages)))
            for message in messages[:max_lines]:
                print('      ' + message)
            if len(messages) > max_lines:
                print('      ... %d more' % (len(messages) - max_lines))
        for topic, messages in self.notes.items():
            print('note: %s (%d)' % (topic, len(messages)))
            for message in messages[:max_lines]:
                print('      ' + message)
            if len(messages) > max_lines:
                print('      ... %d more' % (len(messages) - max_lines))
        print('result: %s' % ('PASS' if self.total() == 0 else
                               'FAIL (%d mismatches)' % self.total()))

    def to_json(self):
        return {'stats': self.stats,
                'mismatches': {k: v for k, v in self.mismatches.items()},
                'notes': {k: v for k, v in self.notes.items()},
                'passed': self.total() == 0}


def apollo_id(lanelet, inverted):
    """Apollo lane id the adapter gives a lanelet (see UniqueId())."""
    name = str(lanelet.id)
    if 'apollo:id' in lanelet.attributes:
        name = lanelet.attributes['apollo:id']
    return name + ('_inv' if inverted else '')


def check_import(args):
    ll2 = import_lanelet2()
    apollo_map = common.load_apollo_map(args.apollo_map)
    lanes = lane_table(apollo_map)
    projector = Projector(ll2, args.osm, apollo_map)
    lmap, errors = ll2.io.loadRobust(args.osm, projector.projector)
    rules = ll2.traffic_rules.create(ll2.traffic_rules.Locations.Germany,
                                     args.participant)
    graph = ll2.routing.RoutingGraph(lmap, rules)
    result = Result()
    result.stat('lanelet2 load errors', len(errors))
    result.stat('lanelets', len(lmap.laneletLayer))

    driving = map_lane_pb2.Lane.CITY_DRIVING
    apollo_driving = {i for i, l in lanes.items() if l.type == driving}

    # What Lanelet2 considers a lane of the participant, in both directions.
    directed = []  # (lanelet as driven, apollo id)
    expected_ids = set()
    two_way = 0
    for lanelet in lmap.laneletLayer:
        if not rules.canPass(lanelet):
            continue
        directed.append((lanelet, apollo_id(lanelet, False)))
        if not rules.isOneWay(lanelet):
            two_way += 1
            if args.bidirectional_lanes:
                directed.append((lanelet.invert(), apollo_id(lanelet, True)))
    expected_ids = {name for _, name in directed}
    result.stat('drivable lanelets (lanelet2)', len(directed) - (
        two_way if args.bidirectional_lanes else 0))
    result.stat('two-way lanelets (lanelet2)', two_way)
    result.stat('CITY_DRIVING lanes (apollo)', len(apollo_driving))

    for name in sorted(expected_ids - apollo_driving):
        result.mismatch('drivable lanelets are CITY_DRIVING lanes',
                        'lanelet2 lane %s has no Apollo lane' % name)
    for name in sorted(apollo_driving - expected_ids):
        result.mismatch('drivable lanelets are CITY_DRIVING lanes',
                        'Apollo lane %s is not drivable in lanelet2' % name)
    result.ok('drivable lanelets are CITY_DRIVING lanes')

    def name_of(other):
        return apollo_id(other, other.inverted())

    max_bound_error = 0.0
    max_center_deviation = 0.0
    center_deviations = []
    max_length_error = 0.0
    max_length_error_rel = 0.0
    max_speed_error = 0.0
    edges = collections.Counter()
    conflicts = collections.Counter()
    worst_center = None
    for lanelet, name in directed:
        lane = lanes.get(name)
        if lane is None or lane.type != driving:
            continue

        # 1. bounds: same points, same orientation (lanelet2's align())
        for side, bound, boundary in (
                ('left', lanelet.leftBound, lane.left_boundary),
                ('right', lanelet.rightBound, lane.right_boundary)):
            expected = [projector.xy(p) for p in bound]
            actual = common.curve_points(boundary.curve)
            error = max(
                math.hypot(expected[0][0] - actual[0][0],
                           expected[0][1] - actual[0][1]),
                math.hypot(expected[-1][0] - actual[-1][0],
                           expected[-1][1] - actual[-1][1]),
                common.max_deviation(expected, actual))
            max_bound_error = max(max_bound_error, error)
            if error > args.bound_tolerance:
                result.mismatch(
                    'bound geometry and orientation',
                    'lane %s %s bound differs by %.3f m' % (name, side, error))

        # 2. centerline (the algorithms differ, the lines must not)
        expected = [projector.xy(p) for p in lanelet.centerline]
        actual = common.curve_points(lane.central_curve)
        deviation = common.max_deviation(expected, actual)
        if deviation > max_center_deviation:
            max_center_deviation = deviation
            worst_center = name
        length_error = abs(common.polyline_length(expected) - lane.length)
        max_length_error = max(max_length_error, length_error)
        max_length_error_rel = max(max_length_error_rel,
                                   length_error / max(lane.length, 1e-6))
        end_error = max(
            math.hypot(expected[0][0] - actual[0][0],
                       expected[0][1] - actual[0][1]),
            math.hypot(expected[-1][0] - actual[-1][0],
                       expected[-1][1] - actual[-1][1]))
        center_deviations.append(deviation)
        if end_error > args.bound_tolerance:
            result.mismatch(
                'centerline starts and ends like the lanelet2 centerline',
                'lane %s: end points differ by %.3f m' % (name, end_error))
        if deviation > args.center_tolerance:
            # Lanelet2 and the adapter compute centerlines differently; they
            # only part where the two bounds differ a lot in length.
            left_length = common.polyline_length(
                [projector.xy(p) for p in lanelet.leftBound])
            right_length = common.polyline_length(
                [projector.xy(p) for p in lanelet.rightBound])
            result.note(
                'centerline further than %.2f m from the lanelet2 centerline'
                % args.center_tolerance,
                'lane %s: %.2f m (bounds %.1f m and %.1f m long)' %
                (name, deviation, left_length, right_length))

        # 3. speed limit
        limit = rules.speedLimit(lanelet).speedLimit  # km/h
        speed_error = abs(limit - lane.speed_limit * 3.6)
        if limit > 0.0:
            max_speed_error = max(max_speed_error, speed_error)
            if speed_error > 0.01:
                result.mismatch(
                    'speed limit',
                    'lane %s: lanelet2 %.2f km/h, apollo %.2f km/h' %
                    (name, limit, lane.speed_limit * 3.6))

        # 4. successors == routing graph "following"
        expected = sorted(name_of(o) for o in graph.following(lanelet))
        actual = sorted(i.id for i in lane.successor_id)
        edges['following'] += len(expected)
        if expected != actual:
            result.mismatch('successors == routingGraph.following()',
                            'lane %s: lanelet2 %s, apollo %s' %
                            (name, expected, actual))
        expected = sorted(name_of(o) for o in graph.previous(lanelet))
        actual = sorted(i.id for i in lane.predecessor_id)
        if expected != actual:
            result.mismatch('predecessors == routingGraph.previous()',
                            'lane %s: lanelet2 %s, apollo %s' %
                            (name, expected, actual))

        # 5. lane changes and adjacency
        for side, change, adjacent, neighbors, boundary in (
                ('left', graph.left(lanelet), graph.adjacentLeft(lanelet),
                 lane.left_neighbor_forward_lane_id, lane.left_boundary),
                ('right', graph.right(lanelet), graph.adjacentRight(lanelet),
                 lane.right_neighbor_forward_lane_id, lane.right_boundary)):
            neighbor_ids = sorted(i.id for i in neighbors)
            dotted = boundary_is_dotted(boundary)
            if change is not None:
                edges['lane change'] += 1
                expected, expect_dotted = [name_of(change)], True
            elif adjacent is not None:
                edges['adjacent'] += 1
                expected, expect_dotted = [name_of(adjacent)], False
            else:
                expected, expect_dotted = [], None
            if neighbor_ids != expected:
                result.mismatch(
                    'forward neighbors == routingGraph left/right/adjacent',
                    'lane %s %s: lanelet2 %s, apollo %s' %
                    (name, side, expected, neighbor_ids))
            elif expect_dotted is not None and dotted != expect_dotted:
                result.mismatch(
                    'lane change allowed <=> dotted boundary',
                    'lane %s %s neighbor %s: lanelet2 lane change %s, '
                    'apollo boundary dotted %s' %
                    (name, side, expected[0], expect_dotted, dotted))

        # 6. conflicts (informative: lanelet2 uses polygon overlap, Apollo's
        # adapter asks for a centerline entering the other lane)
        expected = set()
        for other in graph.conflicting(lanelet):
            if hasattr(other, 'leftBound') and other.id != lanelet.id:
                expected.add(apollo_id(other, False))
                expected.add(apollo_id(other, True))
        actual = set()
        overlap_ids = {o.id for o in lane.overlap_id}
        for overlap in apollo_map.overlap:
            if overlap.id.id in overlap_ids and \
                    all(o.HasField('lane_overlap_info')
                        for o in overlap.object):
                actual |= {o.id.id for o in overlap.object} - {name}
        actual_driving = {a for a in actual if a in apollo_driving}
        conflicts['both'] += len(actual_driving & expected)
        conflicts['apollo only'] += len(actual_driving - expected)
        conflicts['lanelet2 only'] += len(
            {e for e in expected if e in apollo_driving} - actual_driving)

    for check in ('bound geometry and orientation',
                  'centerline starts and ends like the lanelet2 centerline',
                  'speed limit',
                  'successors == routingGraph.following()',
                  'predecessors == routingGraph.previous()',
                  'forward neighbors == routingGraph left/right/adjacent',
                  'lane change allowed <=> dotted boundary'):
        result.ok(check)
    result.stat('max bound error [m]', max_bound_error)
    center_deviations.sort()
    if center_deviations:
        n = len(center_deviations)
        result.stat('centerline vs lanelet2: median deviation [m]',
                    center_deviations[n // 2])
        result.stat('centerline vs lanelet2: 95% deviation [m]',
                    center_deviations[min(n - 1, int(0.95 * n))])
    result.stat('centerline vs lanelet2: max deviation [m]',
                max_center_deviation)
    result.stat('  (worst lane)', worst_center)
    result.stat('max length difference [m]', max_length_error)
    result.stat('max length difference [relative]', max_length_error_rel)
    result.stat('max speed limit difference [km/h]', max_speed_error)
    result.stat('following edges (lanelet2)', edges['following'])
    result.stat('lane change edges (lanelet2)', edges['lane change'])
    result.stat('adjacent-only edges (lanelet2)', edges['adjacent'])
    result.stat('lane/lane conflicts in both', conflicts['both'] // 2)
    result.stat('lane/lane conflicts apollo only',
                conflicts['apollo only'] // 2)
    result.stat('lane/lane conflicts lanelet2 only',
                conflicts['lanelet2 only'] // 2)

    # 7. traffic lights: stop position measured on the lanelet2 centerline
    overlap_by_id = {o.id.id: o for o in apollo_map.overlap}
    signal_ids = {s.id.id for s in apollo_map.signal}
    lights = 0
    max_stop_error = 0.0
    for lanelet, name in directed:
        lane = lanes.get(name)
        if lane is None or lanelet.inverted():
            continue
        expected = []
        for light in lanelet.trafficLights():
            if 'fallback' in light.attributes and \
                    light.attributes['fallback'] in ('yes', 'true', '1'):
                continue
            center = ll2.geometry.to2D(lanelet.centerline)
            stop_line = light.stopLine
            if stop_line is not None:
                s = stop_position(center, ll2.geometry.to2D(stop_line))
            else:
                s = ll2.geometry.length(center)
            expected.extend([s] * len(light.trafficLights))
        actual = []
        for overlap_id in lane.overlap_id:
            overlap = overlap_by_id[overlap_id.id]
            others = [o.id.id for o in overlap.object if o.id.id != name]
            if others and others[0] in signal_ids:
                for o in overlap.object:
                    if o.id.id == name:
                        actual.append(o.lane_overlap_info.start_s)
        lights += len(expected)
        if len(expected) != len(actual):
            result.mismatch('traffic lights per lane',
                            'lane %s: lanelet2 %d lights, apollo %d signal '
                            'overlaps' % (name, len(expected), len(actual)))
            continue
        for e, a in zip(sorted(expected), sorted(actual)):
            max_stop_error = max(max_stop_error, abs(e - a))
            if abs(e - a) > args.stop_tolerance:
                result.mismatch('signal stop position',
                                'lane %s: lanelet2 s=%.2f, apollo s=%.2f' %
                                (name, e, a))
    result.ok('traffic lights per lane')
    result.ok('signal stop position')
    result.stat('traffic light/lane pairs', lights)
    result.stat('max stop position difference [m]', max_stop_error)

    # 8. Apollo's routing graph (topo_creator) against lanelet2's
    if args.routing_map:
        from modules.routing.proto import topo_graph_pb2  # noqa
        topo = topo_graph_pb2.Graph()
        with open(args.routing_map, 'rb') as f:
            topo.ParseFromString(f.read())
        forward = set()
        change = set()
        for edge in topo.edge:
            pair = (edge.from_lane_id, edge.to_lane_id)
            if edge.direction_type == topo_graph_pb2.Edge.FORWARD:
                forward.add(pair)
            else:
                change.add(pair)
        expected_forward = set()
        expected_change = set()
        for lanelet, name in directed:
            for other in graph.following(lanelet):
                expected_forward.add((name, name_of(other)))
            for other in (graph.left(lanelet), graph.right(lanelet)):
                if other is not None:
                    expected_change.add((name, name_of(other)))
        result.stat('routing graph nodes (apollo)', len(topo.node))
        result.stat('routing graph forward edges (apollo / lanelet2)',
                    '%d / %d' % (len(forward), len(expected_forward)))
        result.stat('routing graph lane change edges (apollo / lanelet2)',
                    '%d / %d' % (len(change), len(expected_change)))
        if {n.lane_id for n in topo.node} != expected_ids:
            result.mismatch('routing graph nodes == drivable lanelets',
                            '%d nodes, %d drivable lanes' %
                            (len(topo.node), len(expected_ids)))
        for pair in sorted(forward ^ expected_forward)[:20]:
            result.mismatch('routing graph forward edges == following()',
                            '%s -> %s' % pair)
        for pair in sorted(change ^ expected_change)[:20]:
            result.mismatch('routing graph lane changes == left()/right()',
                            '%s -> %s' % pair)
        result.ok('routing graph nodes == drivable lanelets')
        result.ok('routing graph forward edges == following()')
        result.ok('routing graph lane changes == left()/right()')
    return result


def stop_position(center2d, line2d):
    """Arc length at which a lanelet2 centerline meets a stop line (or comes
    closest to it)."""
    center = [(p.x, p.y) for p in center2d]
    line = [(p.x, p.y) for p in line2d]
    travelled = 0.0
    best = None
    for a, b in zip(center, center[1:]):
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        for c, d in zip(line, line[1:]):
            den = (b[0] - a[0]) * (d[1] - c[1]) - (b[1] - a[1]) * (d[0] - c[0])
            if abs(den) < 1e-12:
                continue
            t = ((c[0] - a[0]) * (d[1] - c[1]) -
                 (c[1] - a[1]) * (d[0] - c[0])) / den
            u = ((c[0] - a[0]) * (b[1] - a[1]) -
                 (c[1] - a[1]) * (b[0] - a[0])) / den
            if -1e-9 <= t <= 1 + 1e-9 and -1e-9 <= u <= 1 + 1e-9:
                s = travelled + seg * min(1.0, max(0.0, t))
                best = s if best is None else min(best, s)
        travelled += seg
    if best is not None:
        return best
    # no crossing: closest centerline vertex
    best_distance = float('inf')
    travelled = 0.0
    best = 0.0
    previous = center[0]
    for point in center:
        travelled += math.hypot(point[0] - previous[0], point[1] - previous[1])
        previous = point
        distance = common.distance_to_polyline(point, line)
        if distance < best_distance:
            best_distance, best = distance, travelled
    return best


def check_export(args):
    ll2 = import_lanelet2()
    apollo_map = common.load_apollo_map(args.apollo_map)
    lanes = lane_table(apollo_map)
    proj = apollo_map.header.projection.proj
    if args.proj:
        proj = args.proj
    if not common.usable_proj_string(proj):
        raise ValueError('the Apollo map has no usable projection; pass '
                         '--proj (the string given to apollo_to_lanelet2.py)')
    projection = common.Projection(proj)
    lat, lon = first_node(args.osm)

    class Frame(object):  # lat/lon of lanelet2 points -> Apollo map frame
        @staticmethod
        def xy(gps):
            return projection.forward(gps.lat, gps.lon)

    # Any projector does: geometry is compared through lat/lon.
    projector = ll2.projection.UtmProjector(ll2.io.Origin(lat, lon))
    lmap, errors = ll2.io.loadRobust(args.osm, projector)
    result = Result()
    result.stat('lanelet2 load errors', len(errors))
    for error in list(errors)[:5]:
        result.mismatch('lanelet2 loads the file without errors', str(error))
    result.ok('lanelet2 loads the file without errors')

    rules = ll2.traffic_rules.create(ll2.traffic_rules.Locations.Germany,
                                     args.participant)
    graph = ll2.routing.RoutingGraph(lmap, rules)
    by_apollo_id = {}
    crosswalks = 0
    for lanelet in lmap.laneletLayer:
        subtype = lanelet.attributes['subtype'] \
            if 'subtype' in lanelet.attributes else ''
        if subtype == 'crosswalk':
            crosswalks += 1
            continue
        if 'apollo:id' in lanelet.attributes:
            by_apollo_id[lanelet.attributes['apollo:id']] = lanelet
    result.stat('apollo lanes', len(lanes))
    result.stat('lanelets (without crosswalks)',
                len(lmap.laneletLayer) - crosswalks)
    result.stat('crosswalk lanelets', crosswalks)
    result.stat('apollo crosswalks', len(apollo_map.crosswalk))
    result.stat('regulatory elements', len(lmap.regulatoryElementLayer))
    for name in sorted(set(lanes) - set(by_apollo_id)):
        result.mismatch('every lane is a lanelet',
                        'lane %s has no lanelet' % name)
    result.ok('every lane is a lanelet')
    if crosswalks != len(apollo_map.crosswalk):
        result.mismatch('every crosswalk is a crosswalk lanelet',
                        '%d crosswalks, %d crosswalk lanelets' %
                        (len(apollo_map.crosswalk), crosswalks))
    result.ok('every crosswalk is a crosswalk lanelet')

    unexpressed = set()
    if args.export_report:
        with open(args.export_report) as f:
            unexpressed = {tuple(pair) for pair in
                           json.load(f).get('unexpressed_successors', [])}
    driving = map_lane_pb2.Lane.CITY_DRIVING
    counts = collections.Counter()
    max_center_deviation = 0.0
    max_bound_deviation = 0.0
    shifted_bounds = 0
    for name, lane in sorted(lanes.items()):
        lanelet = by_apollo_id.get(name)
        if lanelet is None:
            continue
        passable = rules.canPass(lanelet)
        if passable != (lane.type == driving):
            result.mismatch('CITY_DRIVING lanes are drivable lanelets',
                            'lane %s: type %s, lanelet2 canPass %s' %
                            (name, map_lane_pb2.Lane.LaneType.Name(lane.type),
                             passable))
        if lane.type != driving:
            continue
        counts['driving lanes'] += 1

        expected = sorted(i.id for i in lane.successor_id
                          if lanes[i.id].type == driving)
        actual = sorted(o.attributes['apollo:id']
                        for o in graph.following(lanelet))
        counts['successor edges'] += len(expected)
        # A lane that splits into two neighbor lanes has only one lanelet2
        # follower; the exporter lists the others in its report.
        known = sorted(e for e in expected if (name, e) in unexpressed)
        counts['successors that are not followers'] += len(known)
        if sorted(set(expected) - set(known)) != actual:
            result.mismatch('successors are routingGraph.following()',
                            'lane %s: apollo %s, lanelet2 %s' %
                            (name, expected, actual))

        for side, neighbors, boundary, change, adjacent in (
                ('left', lane.left_neighbor_forward_lane_id,
                 lane.left_boundary, graph.left(lanelet),
                 graph.adjacentLeft(lanelet)),
                ('right', lane.right_neighbor_forward_lane_id,
                 lane.right_boundary, graph.right(lanelet),
                 graph.adjacentRight(lanelet))):
            for neighbor in neighbors:
                if lanes[neighbor.id].type != driving:
                    continue
                if boundary_is_dotted(boundary):
                    counts['lane change edges'] += 1
                    if change is None or \
                            change.attributes['apollo:id'] != neighbor.id:
                        result.mismatch(
                            'lane changes are routingGraph.left()/right()',
                            'lane %s -> %s (%s)' % (name, neighbor.id, side))
                else:
                    counts['adjacent edges'] += 1
                    if adjacent is None or \
                            adjacent.attributes['apollo:id'] != neighbor.id:
                        result.mismatch(
                            'solid neighbors are routingGraph.adjacent*()',
                            'lane %s -> %s (%s)' % (name, neighbor.id, side))

        speed = rules.speedLimit(lanelet).speedLimit
        if abs(speed - lane.speed_limit * 3.6) > 0.05:
            result.mismatch('speed limit',
                            'lane %s: apollo %.2f km/h, lanelet2 %.2f km/h' %
                            (name, lane.speed_limit * 3.6, speed))

        # geometry, through lat/lon
        def gps_points(line):
            return [Frame.xy(projector.reverse(ll2.core.BasicPoint3d(
                p.x, p.y, p.z))) for p in line]
        center = gps_points(lanelet.centerline)
        deviation = common.max_deviation(
            center, common.curve_points(lane.central_curve))
        max_center_deviation = max(max_center_deviation, deviation)
        if deviation > args.center_tolerance:
            result.mismatch('centerline of the lanelet is the lane centerline',
                            'lane %s: deviation %.3f m' % (name, deviation))
        for bound, boundary in ((lanelet.leftBound, lane.left_boundary),
                                (lanelet.rightBound, lane.right_boundary)):
            deviation = common.max_deviation(
                gps_points(bound), common.curve_points(boundary.curve))
            max_bound_deviation = max(max_bound_deviation, deviation)
            if deviation > args.bound_tolerance:
                shifted_bounds += 1

    # every routing edge of Apollo is reachable: route along each successor
    routed = 0
    for name, lane in sorted(lanes.items()):
        lanelet = by_apollo_id.get(name)
        if lanelet is None or lane.type != driving:
            continue
        for successor in lane.successor_id:
            target = by_apollo_id.get(successor.id)
            if target is None or lanes[successor.id].type != driving:
                continue
            route = graph.getRoute(lanelet, target)
            routed += 1
            if route is None:
                result.mismatch('lanelet2 finds a route along each successor',
                                '%s -> %s' % (name, successor.id))
    for check in ('CITY_DRIVING lanes are drivable lanelets',
                  'successors are routingGraph.following()',
                  'lane changes are routingGraph.left()/right()',
                  'solid neighbors are routingGraph.adjacent*()',
                  'speed limit',
                  'centerline of the lanelet is the lane centerline',
                  'lanelet2 finds a route along each successor'):
        result.ok(check)
    for key in ('driving lanes', 'successor edges',
                'successors that are not followers', 'lane change edges',
                'adjacent edges'):
        result.stat(key, counts[key])
    result.stat('successors lanelet2 can route to', routed - sum(
        1 for m in result.mismatches.get(
            'lanelet2 finds a route along each successor', [])))
    result.stat('max centerline deviation [m]', max_center_deviation)
    result.stat('max bound deviation [m]', max_bound_deviation)
    result.stat('bounds moved more than %.2f m' % args.bound_tolerance,
                shifted_bounds)
    result.stat('traffic light regulatory elements', sum(
        1 for r in lmap.regulatoryElementLayer
        if r.attributes['subtype'] == 'traffic_light'))
    result.stat('apollo signals', len(apollo_map.signal))
    return result


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('mode', choices=['import', 'export'])
    parser.add_argument('--osm', required=True, help='Lanelet2 map')
    parser.add_argument('--apollo_map', required=True,
                        help='Apollo map directory or base map file')
    parser.add_argument('--participant', default='vehicle')
    parser.add_argument('--no_bidirectional_lanes', dest='bidirectional_lanes',
                        action='store_false',
                        help='the map was converted with '
                        '--nolanelet2_bidirectional_lanes')
    parser.add_argument('--proj', default='',
                        help='export: PROJ string of the Apollo map frame if '
                        'its header has none')
    parser.add_argument('--routing_map', default='',
                        help='import: routing_map.bin made by topo_creator; '
                        'its edges are compared with the lanelet2 routing '
                        'graph')
    parser.add_argument('--export_report', default='',
                        help='export: report written by apollo_to_lanelet2.py '
                        '(names the relations Lanelet2 cannot express)')
    parser.add_argument('--bound_tolerance', type=float, default=0.01)
    parser.add_argument('--center_tolerance', type=float, default=0.5)
    parser.add_argument('--stop_tolerance', type=float, default=0.6)
    parser.add_argument('--json', default='', help='write the result here')
    args = parser.parse_args()

    result = check_import(args) if args.mode == 'import' else \
        check_export(args)
    result.report()
    if args.json:
        with open(args.json, 'w') as f:
            json.dump(result.to_json(), f, indent=2)
    return 0 if result.total() == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
