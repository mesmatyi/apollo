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
"""Closed-loop test along a route of several lanes.

tools/ros2/closed_loop/run_closed_loop.py measures the lateral deviation
against the centerline of the start lane, which is right for its demo
scenario (one lane). Lanelet2 maps are made of many short lanelets, so this
wrapper runs the very same test (same processes, recording and pass criteria:
it calls run_closed_loop.run_once) with the centerlines of all lanes of the
route as reference and the goal on the last lane.

  run_route_closed_loop.py --map_dir <apollo map dir> --lanes 45084 45326 ...
  run_route_closed_loop.py --map_dir <apollo map dir> --find_route 250

--find_route prints routes of at least that many meters that a test can use:
successive lanes without traffic lights, stop or yield signs (the test has
no perception: a traffic light would stay red), wide enough for the vehicle.
"""

import argparse
import datetime
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', 'closed_loop')))

import lanelet2_common as common  # noqa: E402  pylint: disable=wrong-import-position
import run_closed_loop  # noqa: E402  pylint: disable=wrong-import-position
import vehicle_sim  # noqa: E402  pylint: disable=wrong-import-position

from modules.common_msgs.map_msgs import map_lane_pb2  # noqa: E402


def controlled_lanes(hdmap):
    """Lanes with a signal, stop sign or yield sign overlap."""
    objects = {o.id.id for field in ('signal', 'stop_sign', 'yield')
               for o in getattr(hdmap, field)}
    lanes = set()
    for overlap in hdmap.overlap:
        ids = [o.id.id for o in overlap.object]
        if any(i in objects for i in ids):
            lanes |= {o.id.id for o in overlap.object
                      if o.HasField('lane_overlap_info')}
    return lanes


def max_heading_rate(points):
    """Largest heading change per meter of a polyline (1/m)."""
    worst = 0.0
    for a, b, c in zip(points, points[1:], points[2:]):
        h1 = math.atan2(b[1] - a[1], b[0] - a[0])
        h2 = math.atan2(c[1] - b[1], c[0] - b[0])
        d = abs(math.atan2(math.sin(h2 - h1), math.cos(h2 - h1)))
        length = 0.5 * (math.hypot(b[0] - a[0], b[1] - a[1]) +
                        math.hypot(c[0] - b[0], c[1] - b[1]))
        if length > 1e-6:
            worst = max(worst, d / length)
    return worst


def find_routes(hdmap, min_length, min_half_width, max_curvature, limit=10):
    lanes = {lane.id.id: lane for lane in hdmap.lane}
    blocked = controlled_lanes(hdmap)

    def usable(lane):
        if lane.type != map_lane_pb2.Lane.CITY_DRIVING or \
                lane.id.id in blocked:
            return False
        widths = [s.width for s in lane.left_sample] + \
                 [s.width for s in lane.right_sample]
        return min(widths) >= min_half_width and max_heading_rate(
            common.curve_points(lane.central_curve)) <= max_curvature

    good = {name for name, lane in lanes.items() if usable(lane)}
    routes = []
    for start in sorted(good):
        if any(p.id in good for p in lanes[start].predecessor_id):
            continue  # start routes where a chain begins
        route, length = [start], lanes[start].length
        while length < min_length:
            options = [s.id for s in lanes[route[-1]].successor_id
                       if s.id in good and s.id not in route]
            if not options:
                break
            # prefer going straight on
            options.sort(key=lambda name: (lanes[name].turn !=
                                           map_lane_pb2.Lane.NO_TURN, name))
            route.append(options[0])
            length += lanes[options[0]].length
        if length >= min_length and len(route) > 1:
            routes.append((length, route))
    routes.sort(key=lambda r: (-len(r[1]), -r[0]))
    return routes[:limit]


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--map_dir', required=True)
    parser.add_argument('--lanes', nargs='+', default=[],
                        help='lane ids of the route, in driving order')
    parser.add_argument('--find_route', type=float, default=0.0,
                        metavar='METERS', help='list usable routes and exit')
    parser.add_argument('--start_s', type=float, default=2.0)
    parser.add_argument('--end_s', type=float, default=None,
                        help='goal position on the last lane (default: 2 m '
                        'before its end)')
    parser.add_argument('--runs', type=int, default=1)
    parser.add_argument('--storage', default='mcap',
                        choices=['mcap', 'sqlite3'])
    parser.add_argument('--output_dir', default=os.path.join(
        os.getcwd(), 'closed_loop_' +
        datetime.datetime.now().strftime('%Y%m%d_%H%M%S')))
    parser.add_argument('--apollo_root',
                        default=os.environ.get('APOLLO_ROOT', '/apollo'))
    parser.add_argument('--target_speed', type=float, default=5.0)
    parser.add_argument('--goal_tolerance', type=float, default=3.0)
    parser.add_argument('--max_lateral_deviation', type=float, default=1.0)
    parser.add_argument('--timeout', type=float, default=300.0)
    parser.add_argument('--startup_timeout', type=float, default=120.0)
    parser.add_argument('--domain_id_base', type=int, default=60)
    args = parser.parse_args()

    args.apollo_root = os.path.abspath(args.apollo_root)
    args.map_dir = os.path.abspath(args.map_dir)
    hdmap = common.load_apollo_map(args.map_dir)
    lanes = {lane.id.id: lane for lane in hdmap.lane}

    if args.find_route > 0.0:
        for length, route in find_routes(hdmap, args.find_route, 1.4, 0.12):
            print('%.1f m, %d lanes: --lanes %s' % (length, len(route),
                                                    ' '.join(route)))
        return 0
    if not args.lanes:
        parser.error('--lanes or --find_route is required')

    # reference line of the route; consecutive lanes must be connected
    reference = []
    for i, name in enumerate(args.lanes):
        if name not in lanes:
            parser.error('lane %s is not in the map' % name)
        if i > 0 and name not in [s.id for s in
                                  lanes[args.lanes[i - 1]].successor_id]:
            parser.error('lane %s is no successor of lane %s' %
                         (name, args.lanes[i - 1]))
        points = [(p[0], p[1]) for p in
                  common.curve_points(lanes[name].central_curve)]
        if reference and math.hypot(reference[-1][0] - points[0][0],
                                    reference[-1][1] - points[0][1]) < 1e-6:
            points = points[1:]
        reference += points
    last = lanes[args.lanes[-1]]
    end_s = args.end_s if args.end_s is not None else \
        max(0.0, last.length - 2.0)
    gx, gy, gh = vehicle_sim.pose_on_polyline(
        [(p[0], p[1]) for p in common.curve_points(last.central_curve)], end_s)
    goal = (gx, gy, gh)

    args.lane = args.lanes[0]
    args.front_edge_to_center = run_closed_loop.front_edge_to_center(
        args.apollo_root)
    os.makedirs(args.output_dir, exist_ok=True)
    print('route closed loop: %d lanes, %.1f m, goal (%.2f, %.2f, %.3f) on '
          'lane %s -> %s' % (len(args.lanes), sum(
              lanes[n].length for n in args.lanes), gx, gy, gh,
              args.lanes[-1], args.output_dir), flush=True)
    results = []
    for i in range(args.runs):
        print('--- run %d/%d' % (i + 1, args.runs), flush=True)
        result = run_closed_loop.run_once(args, i, goal, reference)
        bag = result.get('bag_analysis', {})
        print('    %s: %s, drive %ss, front bumper to goal %s m, max lateral '
              '%s m' % ('PASS' if result['passed'] else 'FAIL',
                        result.get('outcome'), result.get('drive_s'),
                        bag.get('final_front_distance_to_goal_m'),
                        bag.get('lateral_deviation_m', {}).get('max')),
              flush=True)
        results.append(result)
    summary = run_closed_loop.write_summary(args, goal, results)
    return 0 if summary['passed'] == summary['runs'] else 1


if __name__ == '__main__':
    sys.exit(main())
