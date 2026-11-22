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
"""Turns a Lanelet2 map (.osm) into a complete Apollo map directory.

  lanelet2_to_apollo.py --osm lanelet2_map.osm --output_dir /path/to/my_map

runs, in this order,

  lanelet2_map_converter   .osm -> base_map.bin, base_map.txt
  topo_creator             base map -> routing_map.bin, routing_map.txt
  sim_map_generator        base map -> sim_map.bin, sim_map.txt

and writes default_end_way_point.txt and lanelet2_conversion.json (the
report of the conversion). Start Apollo with --map_dir=/path/to/my_map
afterwards; every module then works as with any other Apollo map.

Flags of the converter (projection, participant, ...) are passed through,
e.g. `--lanelet2_projection=local` or `--lanelet2_utm_zone=54`, see
docs/ros2/lanelet2.md.

--in_place keeps the .osm as the base map: only routing_map.*, sim_map.* and
default_end_way_point.txt are written next to it, and Apollo converts the
.osm itself whenever it loads the map (pass the same --lanelet2_* flags to
Apollo then, e.g. in modules/common/data/global_flagfile.txt).
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import lanelet2_common as common  # noqa: E402  pylint: disable=wrong-import-position

from modules.common_msgs.map_msgs import map_lane_pb2  # noqa: E402

TOOLS = {
    'lanelet2_map_converter': 'modules/map/tools',
    'topo_creator': 'modules/routing',
    'sim_map_generator': 'modules/map/tools',
}


def find_tool(name, bin_dirs):
    """Looks for an Apollo binary: --bin_dir (a directory with the binary, or
    a build tree with bin/<bazel package>/), the installed package
    (<prefix>/lib/apollo) and PATH."""
    candidates = []
    for directory in bin_dirs:
        candidates.append(os.path.join(directory, name))
        candidates.append(os.path.join(directory, 'bin', TOOLS[name], name))
        candidates.append(os.path.join(directory, TOOLS[name], name))
        candidates.append(os.path.join(directory, 'lib', 'apollo', name))
    for prefix in os.environ.get('AMENT_PREFIX_PATH', '').split(os.pathsep):
        if prefix:
            candidates.append(os.path.join(prefix, 'lib', 'apollo', name))
    for candidate in candidates:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    found = shutil.which(name)
    if found:
        return found
    raise FileNotFoundError(
        'cannot find the Apollo tool "%s"; build the package `apollo` and '
        'source its setup.bash, or pass --bin_dir' % name)


def run(command, log_path, env):
    with open(log_path, 'w') as log:
        log.write('$ %s\n' % ' '.join(command))
        log.flush()
        return subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                              env=env, check=False).returncode


def default_end_way_point(hdmap, lane_id, s):
    """A POI landmark: the given lane position, else one meter before the end
    of the longest driving lane."""
    lanes = [lane for lane in hdmap.lane
             if lane.type == map_lane_pb2.Lane.CITY_DRIVING]
    if lane_id:
        lanes = [lane for lane in hdmap.lane if lane.id.id == lane_id]
        if not lanes:
            raise ValueError('lane %s is not in the converted map' % lane_id)
    if not lanes:
        return None
    lane = max(lanes, key=lambda item: item.length)
    if s is None:
        s = max(0.0, lane.length - 1.0)
    s = min(max(s, 0.0), lane.length)
    x, y, _ = common.point_at(common.curve_points(lane.central_curve), s)
    return ('landmark {\n'
            '    name: "%s",\n'
            '    waypoint {\n'
            '        id: "%s"\n'
            '        s: %.3f\n'
            '        pose {\n'
            '            x: %.6f\n'
            '            y: %.6f\n'
            '        }\n'
            '    }\n'
            '}\n' % (lane.id.id, lane.id.id, s, x, y))


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--osm', required=True, help='Lanelet2 map')
    parser.add_argument('--output_dir', default='',
                        help='Apollo map directory to create')
    parser.add_argument('--in_place', action='store_true',
                        help='write routing/sim map next to the .osm and '
                        'keep the .osm as base map')
    parser.add_argument('--apollo_root',
                        default=os.environ.get('APOLLO_ROOT', '/apollo'),
                        help='Apollo checkout (routing and vehicle config)')
    parser.add_argument('--bin_dir', action='append', default=[],
                        help='where to look for the Apollo tools first '
                        '(may be given several times)')
    parser.add_argument('--end_lane', default='',
                        help='lane of the default end way point')
    parser.add_argument('--end_s', type=float, default=None)
    parser.add_argument('--skip_routing_map', action='store_true')
    parser.add_argument('--skip_sim_map', action='store_true')
    args, converter_flags = parser.parse_known_args()
    for flag in converter_flags:
        if not flag.startswith('--lanelet2_') and \
                not flag.startswith('--nolanelet2_'):
            parser.error('unknown argument %s (only --lanelet2_* flags are '
                         'passed to the converter)' % flag)

    osm = os.path.abspath(args.osm)
    if not os.path.isfile(osm):
        parser.error('%s does not exist' % osm)
    if args.in_place:
        map_dir = os.path.dirname(osm)
        if os.path.basename(osm) not in ('base_map.osm', 'lanelet2_map.osm'):
            parser.error('--in_place needs the map to be called base_map.osm '
                         'or lanelet2_map.osm (the names Apollo looks for)')
    elif args.output_dir:
        map_dir = os.path.abspath(args.output_dir)
    else:
        parser.error('--output_dir or --in_place is required')
    os.makedirs(map_dir, exist_ok=True)

    env = dict(os.environ)
    env['APOLLO_ROOT'] = os.path.abspath(args.apollo_root)
    env.setdefault('GLOG_minloglevel', '1')
    log_dir = os.path.join(map_dir, 'lanelet2_conversion_logs')
    os.makedirs(log_dir, exist_ok=True)
    report_path = os.path.join(map_dir, 'lanelet2_conversion.json')

    # 1. base map. With --in_place it is only needed to derive the other
    # maps; Apollo converts the .osm again on its own with the same result.
    work_dir = tempfile.mkdtemp(prefix='lanelet2_to_apollo_') \
        if args.in_place else map_dir
    try:
        converter = find_tool('lanelet2_map_converter', args.bin_dir)
        code = run([converter, '--osm_file=' + osm, '--output_dir=' + work_dir,
                    '--report_file=' + report_path] + converter_flags,
                   os.path.join(log_dir, 'lanelet2_map_converter.log'), env)
        if code != 0:
            print('lanelet2_map_converter failed (exit %d), see %s' %
                  (code, os.path.join(log_dir, 'lanelet2_map_converter.log')),
                  file=sys.stderr)
            return 1
        hdmap = common.load_apollo_map(work_dir)
        written = [] if args.in_place else ['base_map.bin', 'base_map.txt']

        # 2. routing map
        if not args.skip_routing_map:
            code = run([find_tool('topo_creator', args.bin_dir),
                        '--map_dir=' + work_dir],
                       os.path.join(log_dir, 'topo_creator.log'), env)
            if code != 0 or not os.path.exists(
                    os.path.join(work_dir, 'routing_map.bin')):
                print('topo_creator failed (exit %d), see %s' %
                      (code, os.path.join(log_dir, 'topo_creator.log')),
                      file=sys.stderr)
                return 1
            written += ['routing_map.bin', 'routing_map.txt']

        # 3. sim map
        if not args.skip_sim_map:
            code = run([find_tool('sim_map_generator', args.bin_dir),
                        '--map_dir=' + work_dir, '--output_dir=' + work_dir],
                       os.path.join(log_dir, 'sim_map_generator.log'), env)
            if code != 0 or not os.path.exists(
                    os.path.join(work_dir, 'sim_map.bin')):
                print('sim_map_generator failed (exit %d), see %s' %
                      (code, os.path.join(log_dir, 'sim_map_generator.log')),
                      file=sys.stderr)
                return 1
            written += ['sim_map.bin', 'sim_map.txt']

        if args.in_place:
            for name in written:
                shutil.copyfile(os.path.join(work_dir, name),
                                os.path.join(map_dir, name))
    finally:
        if args.in_place:
            shutil.rmtree(work_dir, ignore_errors=True)

    # 4. default end way point
    poi = default_end_way_point(hdmap, args.end_lane, args.end_s)
    if poi:
        with open(os.path.join(map_dir, 'default_end_way_point.txt'),
                  'w') as f:
            f.write(poi)
        written.append('default_end_way_point.txt')

    with open(report_path) as f:
        report = json.load(f)
    report['map_dir'] = map_dir
    report['files'] = written
    report['converter_flags'] = converter_flags
    with open(report_path, 'w') as f:
        json.dump(report, f, indent=2)

    counts = report.get('counts', {})
    print('Apollo map directory: %s' % map_dir)
    print('  files: %s' % ', '.join(written))
    print('  projection: %s' % (report.get('projection') or
                                'local_x/local_y (no projection)'))
    print('  lanelets %d -> lanes %d (driving %d), roads %d, junctions %d, '
          'signals %d, stop signs %d, yield signs %d, crosswalks %d, '
          'overlaps %d' % tuple(counts.get(k, 0) for k in (
              'lanelets', 'lanes', 'lanes_city_driving', 'roads', 'junctions',
              'signals', 'stop_signs', 'yield_signs', 'crosswalks',
              'overlaps')))
    print('  warnings: %d (details in %s)' % (report.get('total_warnings', 0),
                                             report_path))
    for message in report.get('warnings', [])[:10]:
        print('    - ' + message)
    return 0


if __name__ == '__main__':
    sys.exit(main())
