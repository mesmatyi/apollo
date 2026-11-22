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
"""Compares two Apollo base maps, typically a map and what comes back after
Apollo -> Lanelet2 -> Apollo (apollo_to_lanelet2.py, then the Lanelet2
adapter).

  compare_maps.py --reference modules/map/data/borregas_ave \\
      --candidate /tmp/borregas_back [--export_report export.json]

Compared: the set of lanes, per lane the centerline, length, width, speed
limit, type, turn, boundary types and the successor / predecessor / neighbor
ids; signals, stop signs, yield signs, crosswalks and junctions with the
lanes they overlap and where.

--export_report names the report of apollo_to_lanelet2.py. Relations it
lists as impossible in Lanelet2 (see docs/ros2/lanelet2.md) and the widths of
lanes whose bounds it had to move are then reported as expected differences
instead of failures.

Exit code 0 when nothing but expected differences is found.
"""

import argparse
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import lanelet2_common as common  # noqa: E402  pylint: disable=wrong-import-position

from modules.common_msgs.map_msgs import map_lane_pb2  # noqa: E402

LANE = map_lane_pb2.Lane
BT = map_lane_pb2.LaneBoundaryType


def ids(repeated):
    return sorted(i.id for i in repeated)


def width_at(samples, s):
    """Linear interpolation of LaneSampleAssociation, like LaneInfo."""
    if not samples:
        return 0.0
    if s <= samples[0].s:
        return samples[0].width
    if s >= samples[-1].s:
        return samples[-1].width
    lo, hi = 0, len(samples) - 1
    while lo + 1 < hi:
        mid = (lo + hi) // 2
        if samples[mid].s <= s:
            lo = mid
        else:
            hi = mid
    a, b = samples[lo], samples[hi]
    r = (s - a.s) / (b.s - a.s) if b.s > a.s else 0.0
    return a.width + r * (b.width - a.width)


def polygon_area(points):
    return 0.5 * abs(sum(points[i][0] * points[(i + 1) % len(points)][1] -
                         points[(i + 1) % len(points)][0] * points[i][1]
                         for i in range(len(points))))


def boundary_types(boundary):
    return [BT.Type.Name(t) for bt in boundary.boundary_type for t in bt.types]


def object_overlaps(hdmap, object_ids):
    """object id -> {lane id: (start_s, end_s)} for a set of map objects."""
    result = collections.defaultdict(dict)
    for overlap in hdmap.overlap:
        lanes = [o for o in overlap.object if o.HasField('lane_overlap_info')]
        others = [o for o in overlap.object if o.id.id in object_ids]
        if len(lanes) == 1 and len(others) == 1:
            info = lanes[0].lane_overlap_info
            result[others[0].id.id][lanes[0].id.id] = (info.start_s,
                                                       info.end_s)
    return result


def point_in_polygon(point, ring):
    inside = False
    for i in range(len(ring)):
        p, q = ring[i], ring[i - 1]
        if (p[1] > point[1]) != (q[1] > point[1]) and \
                point[0] < p[0] + (point[1] - p[1]) / (q[1] - p[1]) * \
                (q[0] - p[0]):
            inside = not inside
    return inside


def lane_area_range(lane, ring, step=0.1):
    """[s0, s1] where the centerline or a boundary of a lane is inside a
    polygon, measured along the centerline; None if it never is."""
    center = common.curve_points(lane.central_curve)
    lines = [center, common.curve_points(lane.left_boundary.curve),
             common.curve_points(lane.right_boundary.curve)]
    length = common.polyline_length(center)
    hits = []
    count = max(2, int(length / step) + 1)
    for line in lines:
        line_length = common.polyline_length(line)
        for i in range(count):
            ratio = i / (count - 1.0)
            if point_in_polygon(common.point_at(line, ratio * line_length),
                                ring):
                hits.append(ratio * length)
    return (min(hits), max(hits)) if hits else None


class Comparison(object):
    def __init__(self, args):
        self.args = args
        self.stats = collections.OrderedDict()
        self.failures = collections.OrderedDict()
        self.expected = collections.OrderedDict()

    def fail(self, check, message):
        self.failures.setdefault(check, []).append(message)

    def expect(self, check, message):
        self.expected.setdefault(check, []).append(message)

    def check(self, name):
        self.failures.setdefault(name, [])

    def total(self):
        return sum(len(v) for v in self.failures.values())

    def report(self, max_lines=8):
        print('statistics:')
        for name, value in self.stats.items():
            if isinstance(value, float):
                value = '%.6g' % value
            print('  %-50s %s' % (name, value))
        print('checks:')
        for check, messages in self.failures.items():
            print('  %-50s %s' % (check, 'OK' if not messages else
                                  '%d DIFFERENCE(S)' % len(messages)))
            for message in messages[:max_lines]:
                print('      ' + message)
            if len(messages) > max_lines:
                print('      ... %d more' % (len(messages) - max_lines))
        for check, messages in self.expected.items():
            print('expected: %s (%d)' % (check, len(messages)))
            for message in messages[:max_lines]:
                print('      ' + message)
            if len(messages) > max_lines:
                print('      ... %d more' % (len(messages) - max_lines))
        print('result: %s' % ('PASS' if self.total() == 0 else
                               'FAIL (%d differences)' % self.total()))


def compare(args):
    reference = common.load_apollo_map(args.reference)
    candidate = common.load_apollo_map(args.candidate)
    out = Comparison(args)

    export = {}
    if args.export_report:
        with open(args.export_report) as f:
            export = json.load(f)
    unexpressed = {tuple(p) for p in export.get('unexpressed_successors', [])}
    unshared = {(p[0], p[1]) for p in export.get('unshared_neighbors', [])}
    bound_shift = export.get('bound_shift_m', {})

    ref_lanes = {lane.id.id: lane for lane in reference.lane}
    new_lanes = {lane.id.id: lane for lane in candidate.lane}
    out.stats['lanes (reference / candidate)'] = '%d / %d' % (
        len(ref_lanes), len(new_lanes))
    for name in sorted(set(ref_lanes) - set(new_lanes)):
        out.fail('same lanes', 'lane %s is missing' % name)
    for name in sorted(set(new_lanes) - set(ref_lanes)):
        out.fail('same lanes', 'lane %s is new' % name)
    out.check('same lanes')

    length_errors = []
    center_errors = []
    width_errors = []
    counts = collections.Counter()
    for name in sorted(set(ref_lanes) & set(new_lanes)):
        a, b = ref_lanes[name], new_lanes[name]
        shift = bound_shift.get(name, 0.0)

        # geometry
        center_a = common.curve_points(a.central_curve)
        center_b = common.curve_points(b.central_curve)
        length_a = common.polyline_length(center_a)
        error = abs(length_a - b.length)
        length_errors.append(error)
        if error > args.length_tolerance:
            out.fail('lane length', 'lane %s: %.3f m -> %.3f m' %
                     (name, length_a, b.length))
        deviation = common.max_deviation(center_a, center_b)
        center_errors.append(deviation)
        if deviation > args.center_tolerance:
            out.fail('lane centerline', 'lane %s deviates by %.3f m' %
                     (name, deviation))
        worst_width = 0.0
        for k in range(11):
            ratio = k / 10.0
            for samples_a, samples_b in ((a.left_sample, b.left_sample),
                                         (a.right_sample, b.right_sample)):
                worst_width = max(worst_width, abs(
                    width_at(samples_a, ratio * length_a) -
                    width_at(samples_b, ratio * b.length)))
        width_errors.append(worst_width)
        if worst_width > args.width_tolerance:
            message = 'lane %s: half width differs by up to %.3f m' % (
                name, worst_width)
            # explained by what the exporter did to the bounds of this lane?
            if worst_width <= shift + args.width_tolerance:
                out.expect('width of lanes whose bounds the exporter had to '
                           'move', message + ' (bound moved %.2f m)' % shift)
            else:
                out.fail('lane width', message +
                         ' (bound moved %.2f m)' % shift)

        # attributes
        if a.speed_limit <= 0.0:
            # no limit in the reference: the adapter fills in its default
            counts['lanes without speed limit in the reference'] += 1
        elif abs(a.speed_limit - b.speed_limit) > 1e-3:
            out.fail('speed limit', 'lane %s: %.3f -> %.3f m/s' %
                     (name, a.speed_limit, b.speed_limit))
        if a.type != b.type:
            out.fail('lane type', 'lane %s: %s -> %s' % (
                name, LANE.LaneType.Name(a.type), LANE.LaneType.Name(b.type)))
        if a.turn != b.turn:
            out.fail('lane turn', 'lane %s: %s -> %s' % (
                name, LANE.LaneTurn.Name(a.turn), LANE.LaneTurn.Name(b.turn)))
        for side, ba, bb in (('left', a.left_boundary, b.left_boundary),
                             ('right', a.right_boundary, b.right_boundary)):
            types_a, types_b = boundary_types(ba), boundary_types(bb)
            if types_a[:1] != types_b[:1]:
                # Colors and line kinds Lanelet2 cannot tell apart on a
                # bound two lanes share are listed, not failed.
                out.fail('boundary type', 'lane %s %s: %s -> %s' %
                         (name, side, types_a, types_b))

        # topology
        for label, field in (
                ('successors', 'successor_id'),
                ('predecessors', 'predecessor_id'),
                ('left forward neighbors', 'left_neighbor_forward_lane_id'),
                ('right forward neighbors', 'right_neighbor_forward_lane_id'),
                ('left reverse neighbors', 'left_neighbor_reverse_lane_id'),
                ('right reverse neighbors',
                 'right_neighbor_reverse_lane_id')):
            ids_a, ids_b = ids(getattr(a, field)), ids(getattr(b, field))
            counts[label] += len(ids_a)
            if ids_a == ids_b:
                continue
            missing = set(ids_a) - set(ids_b)
            extra = set(ids_b) - set(ids_a)
            known = set()
            for other in missing:
                if label == 'successors' and (name, other) in unexpressed:
                    known.add(other)
                if label == 'predecessors' and (other, name) in unexpressed:
                    known.add(other)
                if 'neighbors' in label and ((name, other) in unshared or
                                             (other, name) in unshared):
                    known.add(other)
            for other in sorted(known):
                counts[label + ' lost (expected)'] += 1
                out.expect('relations Lanelet2 cannot express',
                           '%s of lane %s: %s' % (label, name, other))
            # The adapter makes lanes of different types neighbors only
            # across a dashed line (planning borrows neighbor lanes).
            by_design = set()
            for other in missing - known:
                boundary = a.left_boundary if label.startswith('left') else \
                    a.right_boundary
                if 'neighbors' in label and other in ref_lanes and \
                        ref_lanes[other].type != a.type and \
                        not any(t.startswith('DOTTED')
                                for t in boundary_types(boundary)):
                    by_design.add(other)
                    counts[label + ' dropped (by design)'] += 1
                    out.expect('neighbors of another lane type behind a '
                               'solid line (dropped by design)',
                               '%s of lane %s: %s' % (label, name, other))
            known |= by_design
            if missing - known or extra:
                out.fail(label, 'lane %s: %s -> %s' % (name, ids_a, ids_b))
    for label in ('lane length', 'lane centerline', 'lane width',
                  'speed limit', 'lane type', 'lane turn', 'boundary type',
                  'successors', 'predecessors', 'left forward neighbors',
                  'right forward neighbors', 'left reverse neighbors',
                  'right reverse neighbors'):
        out.check(label)
    if length_errors:
        out.stats['max lane length difference [m]'] = max(length_errors)
        out.stats['max centerline deviation [m]'] = max(center_errors)
        width_errors.sort()
        out.stats['half width difference: median / max [m]'] = '%.4g / %.4g' % (
            width_errors[len(width_errors) // 2], width_errors[-1])
        out.stats['lanes with half width difference > %.2f m' %
                  args.width_tolerance] = sum(
                      1 for w in width_errors if w > args.width_tolerance)
    if counts['lanes without speed limit in the reference']:
        out.stats['lanes without speed limit in the reference'] = \
            counts['lanes without speed limit in the reference']
    for label in ('successors', 'predecessors', 'left forward neighbors',
                  'right forward neighbors', 'left reverse neighbors',
                  'right reverse neighbors'):
        out.stats['%s (reference)' % label] = counts[label]
        if counts[label + ' lost (expected)']:
            out.stats['  %s not expressible in Lanelet2' % label] = \
                counts[label + ' lost (expected)']
        if counts[label + ' dropped (by design)']:
            out.stats['  %s dropped by design' % label] = \
                counts[label + ' dropped (by design)']

    # every candidate lane belongs to a road
    in_road = {i.id for road in candidate.road for section in road.section
               for i in section.lane_id}
    for name in sorted(set(new_lanes) - in_road):
        out.fail('every lane belongs to a road', 'lane %s' % name)
    out.check('every lane belongs to a road')
    out.stats['roads (reference / candidate)'] = '%d / %d' % (
        len(reference.road), len(candidate.road))

    # objects and their lane overlaps
    for label, field in (('signals', 'signal'), ('stop signs', 'stop_sign'),
                         ('yield signs', 'yield'),
                         ('crosswalks', 'crosswalk'),
                         ('junctions', 'junction')):
        objects_a = {o.id.id: o for o in getattr(reference, field)}
        objects_b = {o.id.id: o for o in getattr(candidate, field)}
        out.stats['%s (reference / candidate)' % label] = '%d / %d' % (
            len(objects_a), len(objects_b))
        if sorted(objects_a) != sorted(objects_b):
            out.fail('same ' + label, '%s -> %s' % (sorted(objects_a),
                                                    sorted(objects_b)))
        out.check('same ' + label)
        overlaps_a = object_overlaps(reference, set(objects_a))
        overlaps_b = object_overlaps(candidate, set(objects_b))
        area = label in ('crosswalks', 'junctions')
        worst_s = 0.0
        pairs = 0
        for name in sorted(set(objects_a) & set(objects_b)):
            lanes_a = overlaps_a.get(name, {})
            lanes_b = overlaps_b.get(name, {})
            if not area:
                if sorted(lanes_a) != sorted(lanes_b):
                    out.fail('lanes of ' + label, '%s: %s -> %s' % (
                        name, sorted(lanes_a), sorted(lanes_b)))
                for lane in set(lanes_a) & set(lanes_b):
                    pairs += 1
                    # the stop position must lie in the overlap interval of
                    # the reference (Apollo maps store an interval)
                    s0, s1 = lanes_a[lane]
                    s = lanes_b[lane][0]
                    error = max(0.0, s0 - s, s - s1)
                    worst_s = max(worst_s, error)
                    if error > args.stop_tolerance:
                        out.fail('stop position of ' + label,
                                 '%s on lane %s: [%.2f, %.2f] -> %.2f' %
                                 (name, lane, s0, s1, s))
                continue
            # Areas. The s-ranges of the reference are not trusted (maps
            # exported by simulators carry wrong ones): the range is measured
            # here from the geometry of the reference map.
            ring_a = [(p.x, p.y) for p in objects_a[name].polygon.point]
            ring_b = [(p.x, p.y) for p in objects_b[name].polygon.point]
            area_a, area_b = polygon_area(ring_a), polygon_area(ring_b)
            if abs(area_a - area_b) > 1e-3 * max(area_a, 1.0):
                out.fail('polygon of ' + label, '%s: area %.3f -> %.3f' %
                         (name, area_a, area_b))
            for lane in sorted(lanes_a):
                if lane not in ref_lanes:
                    continue
                measured = lane_area_range(ref_lanes[lane], ring_a)
                if measured is None or \
                        measured[1] - measured[0] < args.area_overlap_min:
                    continue  # the lane only touches the area
                if lane not in lanes_b:
                    out.fail('lanes of ' + label,
                             '%s: lane %s is inside for %.1f m but has no '
                             'overlap' % (name, lane,
                                          measured[1] - measured[0]))
                    continue
                pairs += 1
                if label == 'crosswalks':
                    error = max(abs(measured[0] - lanes_b[lane][0]),
                                abs(measured[1] - lanes_b[lane][1]))
                    worst_s = max(worst_s, error)
                    if error > args.stop_tolerance:
                        out.fail('range of lanes on crosswalks',
                                 '%s lane %s: measured [%.2f, %.2f], '
                                 'candidate [%.2f, %.2f]' %
                                 ((name, lane) + measured + lanes_b[lane]))
        out.check('lanes of ' + label)
        if area:
            out.check('polygon of ' + label)
            if label == 'crosswalks':
                out.check('range of lanes on crosswalks')
        else:
            out.check('stop position of ' + label)
        if pairs:
            out.stats['  %s: lane overlaps compared' % label] = pairs
            if label != 'junctions':
                out.stats['  %s: max position difference [m]' % label] = \
                    worst_s
    return out


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--reference', required=True,
                        help='Apollo map directory or base map file')
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--export_report', default='',
                        help='report of apollo_to_lanelet2.py')
    parser.add_argument('--length_tolerance', type=float, default=0.05)
    parser.add_argument('--center_tolerance', type=float, default=0.05)
    parser.add_argument('--width_tolerance', type=float, default=0.05)
    parser.add_argument('--stop_tolerance', type=float, default=1.0)
    parser.add_argument('--area_overlap_min', type=float, default=1.0)
    parser.add_argument('--json', default='', help='write the result here')
    args = parser.parse_args()
    result = compare(args)
    result.report()
    if args.json:
        with open(args.json, 'w') as f:
            json.dump({'stats': result.stats, 'failures': result.failures,
                       'expected': result.expected,
                       'passed': result.total() == 0}, f, indent=2)
    return 0 if result.total() == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
