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
"""Validates Apollo's Lanelet2 support end to end.

  validate.py --work_dir /tmp/lanelet2_validation [--bin_dir <build tree>]

1. Official Lanelet2 example map (fetched if needed, skipped when offline):
   .osm -> Apollo map directory (base, routing and sim map); the result is
   compared with what the real Lanelet2 library reads from the same file,
   including Apollo's routing graph against lanelet2's.
2. The unit test maps of the adapter: the same comparison.
3. Round trip of the Apollo maps in this repository (demo, borregas_ave and
   the HD map test data): Apollo -> Lanelet2 with apollo_to_lanelet2.py, the
   .osm is loaded and routed on with the real Lanelet2 library, converted
   back with the adapter and compared with the original map.
4. --fuzz N: N randomly damaged copies of the example map must convert or be
   rejected without a crash.

The comparisons with the Lanelet2 library need `pip install lanelet2`; they
are skipped (and reported as skipped) without it. Exit code 0 when every
step that ran passed.
"""

import argparse
import json
import os
import random
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import fetch_example_map  # noqa: E402  pylint: disable=wrong-import-position
import lanelet2_to_apollo  # noqa: E402  pylint: disable=wrong-import-position

ORIGIN = '49.0,8.4'  # of the unit test maps, see make_test_maps.py


class Run(object):
    def __init__(self, args):
        self.args = args
        self.rows = []  # (step, result, details)
        self.failed = False
        try:
            import lanelet2  # noqa  pylint: disable=import-outside-toplevel
            self.have_lanelet2 = True
        except ImportError:
            self.have_lanelet2 = False

    def record(self, step, ok, details):
        result = 'PASS' if ok else 'FAIL'
        if ok is None:
            result = 'SKIP'
        self.failed |= ok is False
        self.rows.append((step, result, details))
        print('%-4s %s: %s' % (result, step, details), flush=True)

    def tool(self, script, arguments, log):
        """Runs one of the Python tools; returns (exit code, output)."""
        command = [sys.executable, os.path.join(HERE, script)] + arguments
        result = subprocess.run(command, capture_output=True, text=True,
                                check=False)
        with open(log, 'w') as f:
            f.write('$ %s\n%s%s' % (' '.join(command), result.stdout,
                                    result.stderr))
        return result.returncode, result.stdout + result.stderr

    def convert(self, osm, map_dir, flags=()):
        arguments = ['--osm', osm, '--output_dir', map_dir,
                     '--apollo_root', self.args.apollo_root]
        for directory in self.args.bin_dir:
            arguments += ['--bin_dir', directory]
        code, output = self.tool('lanelet2_to_apollo.py',
                                 arguments + list(flags),
                                 map_dir + '_convert.log')
        if code != 0:
            return None
        with open(os.path.join(map_dir, 'lanelet2_conversion.json')) as f:
            return json.load(f)

    def check_import(self, name, osm, map_dir, extra=()):
        if not self.have_lanelet2:
            self.record(name + ': compare with the Lanelet2 library', None,
                        'python package lanelet2 is not installed')
            return
        json_path = map_dir + '_check_import.json'
        code, _ = self.tool(
            'check_with_lanelet2.py',
            ['import', '--osm', osm, '--apollo_map', map_dir, '--routing_map',
             os.path.join(map_dir, 'routing_map.bin'), '--json', json_path] +
            list(extra), map_dir + '_check_import.log')
        stats = {}
        if os.path.exists(json_path):
            with open(json_path) as f:
                stats = json.load(f).get('stats', {})
        self.record(
            name + ': compare with the Lanelet2 library', code == 0,
            '%s lanelets, %s drivable, %s two-way; routing graph forward '
            'edges (apollo / lanelet2) %s, lane changes %s; bounds within '
            '%.1e m; centerline vs lanelet2 median %.3f m, max %.3f m; '
            'traffic light/lane pairs %s, stop position within %.3f m '
            '(log: %s)' % (
                stats.get('lanelets'),
                stats.get('drivable lanelets (lanelet2)'),
                stats.get('two-way lanelets (lanelet2)'),
                stats.get('routing graph forward edges (apollo / lanelet2)'),
                stats.get('routing graph lane change edges (apollo / '
                          'lanelet2)'),
                stats.get('max bound error [m]', float('nan')),
                stats.get('centerline vs lanelet2: median deviation [m]',
                          float('nan')),
                stats.get('centerline vs lanelet2: max deviation [m]',
                          float('nan')),
                stats.get('traffic light/lane pairs'),
                stats.get('max stop position difference [m]', float('nan')),
                map_dir + '_check_import.log'))

    def import_map(self, name, osm, flags=()):
        map_dir = os.path.join(self.args.work_dir, name)
        report = self.convert(osm, map_dir, flags)
        if report is None:
            self.record(name + ': Lanelet2 -> Apollo map directory', False,
                        'conversion failed, see %s_convert.log' % map_dir)
            return None
        counts = report['counts']
        files = sorted(os.listdir(map_dir))
        complete = all(n in files for n in (
            'base_map.bin', 'base_map.txt', 'routing_map.bin',
            'routing_map.txt', 'sim_map.bin', 'sim_map.txt',
            'default_end_way_point.txt'))
        self.record(
            name + ': Lanelet2 -> Apollo map directory', complete,
            'lanelets %d -> lanes %d (driving %d, of which inverted %d, '
            'biking %d, sidewalk %d), ignored %d; roads %d; junctions %d '
            '(inferred %d); regulatory elements %d -> signals %d, stop '
            'signs %d, yield signs %d (fallback elements ignored %d); '
            'crosswalks %d; overlaps %d; warnings %d' % tuple(
                [counts.get(k, 0) for k in (
                    'lanelets', 'lanes', 'lanes_city_driving',
                    'lanes_inverted', 'lanes_biking', 'lanes_sidewalk',
                    'lanelets_ignored', 'roads', 'junctions',
                    'junctions_inferred', 'regulatory_elements', 'signals',
                    'stop_signs', 'yield_signs',
                    'regulatory_elements_fallback_ignored', 'crosswalks',
                    'overlaps')] + [report.get('total_warnings', 0)]))
        return map_dir

    def round_trip(self, name, apollo_map, export_flags=(), suffix='',
                   compare_flags=()):
        work = os.path.join(self.args.work_dir, name + suffix)
        osm = work + '.osm'
        export_json = work + '_export.json'
        code, _ = self.tool('apollo_to_lanelet2.py',
                            ['--map', apollo_map, '--output', osm,
                             '--report', export_json] + list(export_flags),
                            work + '_export.log')
        title = name + suffix
        if code != 0:
            self.record(title + ': Apollo -> Lanelet2', False,
                        'export failed, see %s_export.log' % work)
            return
        with open(export_json) as f:
            export = json.load(f)
        report = export['report']
        self.record(
            title + ': Apollo -> Lanelet2', True,
            '%d lanes -> %d nodes, %d ways, %d relations; neighbor bounds '
            'shared %d (not shared %d); successor links %d (not '
            'expressible %d); bounds moved > 0.1 m: %d, max %.2f m' % (
                report['lanes'], report['nodes'], report['ways'],
                report['relations'], report['neighbor_bounds_shared'],
                report['neighbor_bounds_not_shared'],
                report['successor_links'],
                report['successor_links_not_expressed'],
                report['bounds_shifted_over_0.1m'],
                report['bound_shift_max_m']))

        if self.have_lanelet2:
            json_path = work + '_check_export.json'
            arguments = ['export', '--osm', osm, '--apollo_map', apollo_map,
                         '--export_report', export_json, '--json', json_path]
            if report.get('projection'):
                arguments += ['--proj', report['projection']]
            code, _ = self.tool('check_with_lanelet2.py', arguments,
                                work + '_check_export.log')
            stats = {}
            if os.path.exists(json_path):
                with open(json_path) as f:
                    stats = json.load(f).get('stats', {})
            self.record(
                title + ': the Lanelet2 library loads and routes on the '
                'export', code == 0,
                'load errors %s; %s lanelets + %s crosswalk lanelets, %s '
                'regulatory elements; successor edges %s (not followers: '
                '%s, routable %s); lane change edges %s; adjacent edges %s '
                '(log: %s)' % (
                    stats.get('lanelet2 load errors'),
                    stats.get('lanelets (without crosswalks)'),
                    stats.get('crosswalk lanelets'),
                    stats.get('regulatory elements'),
                    stats.get('successor edges'),
                    stats.get('successors that are not followers'),
                    stats.get('successors lanelet2 can route to'),
                    stats.get('lane change edges'),
                    stats.get('adjacent edges'), work + '_check_export.log'))
        else:
            self.record(title + ': the Lanelet2 library loads and routes on '
                        'the export', None, 'lanelet2 is not installed')

        back = self.convert(osm, work + '_back')
        if back is None:
            self.record(title + ': Lanelet2 -> Apollo again', False,
                        'conversion failed, see %s_back_convert.log' % work)
            return
        json_path = work + '_compare.json'
        code, _ = self.tool(
            'compare_maps.py',
            ['--reference', apollo_map, '--candidate', work + '_back',
             '--export_report', export_json, '--json', json_path] +
            list(compare_flags), work + '_compare.log')
        with open(json_path) as f:
            result = json.load(f)
        stats = result['stats']
        expected = sum(len(v) for v in result['expected'].values())
        self.record(
            title + ': round trip compared with the original', code == 0,
            'lanes %s; length diff max %.2e m; centerline deviation max '
            '%.2e m; half width diff median/max %s m; successors %s, '
            'forward neighbors %s+%s, reverse neighbors %s; signals %s, '
            'stop signs %s, crosswalks %s, junctions %s; %d expected '
            'differences, %d failures (log: %s)' % (
                stats.get('lanes (reference / candidate)'),
                stats.get('max lane length difference [m]', float('nan')),
                stats.get('max centerline deviation [m]', float('nan')),
                stats.get('half width difference: median / max [m]'),
                stats.get('successors (reference)'),
                stats.get('left forward neighbors (reference)'),
                stats.get('right forward neighbors (reference)'),
                stats.get('left reverse neighbors (reference)'),
                stats.get('signals (reference / candidate)'),
                stats.get('stop signs (reference / candidate)'),
                stats.get('crosswalks (reference / candidate)'),
                stats.get('junctions (reference / candidate)'), expected,
                sum(len(v) for v in result['failures'].values()),
                work + '_compare.log'))

    def fuzz(self, osm, count):
        """Damaged maps must be converted or rejected, never crash."""
        converter = lanelet2_to_apollo.find_tool('lanelet2_map_converter',
                                                 self.args.bin_dir)
        with open(osm) as f:
            lines = f.read().split('\n')
        rng = random.Random(self.args.seed)
        outcomes = {'converted': 0, 'rejected': 0, 'crashed': 0}
        env = dict(os.environ, APOLLO_ROOT=self.args.apollo_root,
                   GLOG_minloglevel='3')
        work = os.path.join(self.args.work_dir, 'fuzz')
        os.makedirs(work, exist_ok=True)
        for i in range(count):
            mutated = list(lines)
            for _ in range(rng.randint(1, 40)):
                k = rng.randrange(len(mutated))
                kind = rng.random()
                if kind < 0.4:
                    # Drop a node, a way point, a member or a tag. One map in
                    # ten also loses arbitrary lines (no longer XML).
                    if i % 10 != 0 and not mutated[k].rstrip().endswith('/>'):
                        continue
                    del mutated[k]
                elif kind < 0.6:
                    mutated[k] = re.sub(r"ref='\d+'", "ref='%d'" %
                                        rng.randrange(10 ** 6), mutated[k])
                elif kind < 0.8:
                    mutated[k] = re.sub(r"v='[^']*'", "v='%s'" % rng.choice(
                        ['', 'x', 'yes', '-1', 'lanelet', 'traffic_light',
                         'dashed', '1e400']), mutated[k])
                else:
                    mutated[k] = re.sub(r"(lat|lon)='[^']*'", r"\1='%s'" %
                                        rng.choice(['nan', '91', '-181', '']),
                                        mutated[k])
            path = os.path.join(work, 'fuzz_%03d.osm' % i)
            with open(path, 'w') as f:
                f.write('\n'.join(mutated))
            result = subprocess.run(
                [converter, '--osm_file=' + path,
                 '--output_dir=' + os.path.join(work, 'out')],
                capture_output=True, env=env, check=False)
            if result.returncode == 0:
                outcomes['converted'] += 1
                os.remove(path)
            elif result.returncode in (1, 2):
                outcomes['rejected'] += 1
                os.remove(path)
            else:
                outcomes['crashed'] += 1  # the file is kept
        self.record('robustness: %d damaged copies of the example map' %
                    count, outcomes['crashed'] == 0,
                    '%(converted)d converted, %(rejected)d rejected, '
                    '%(crashed)d crashed' % outcomes)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--work_dir', required=True)
    parser.add_argument('--apollo_root',
                        default=os.environ.get('APOLLO_ROOT', '/apollo'))
    parser.add_argument('--bin_dir', action='append', default=[])
    parser.add_argument('--example_map', default='',
                        help='mapping_example.osm (default: look it up or '
                        'download it, see fetch_example_map.py)')
    parser.add_argument('--fuzz', type=int, default=0,
                        help='number of damaged maps to try')
    parser.add_argument('--seed', type=int, default=1)
    args = parser.parse_args()
    args.apollo_root = os.path.abspath(args.apollo_root)
    args.work_dir = os.path.abspath(args.work_dir)
    os.makedirs(args.work_dir, exist_ok=True)
    run = Run(args)

    # 1. the official example map
    example = args.example_map or fetch_example_map.find_example_map(
        os.path.join(args.work_dir, 'download'))
    if example:
        map_dir = run.import_map('mapping_example', example)
        if map_dir:
            run.check_import('mapping_example', example, map_dir)
    else:
        run.record('mapping_example', None, 'the official Lanelet2 example '
                   'map is not available (offline?)')

    # 2. the unit test maps
    testdata = os.path.join(args.apollo_root, 'modules', 'map', 'hdmap',
                            'adapter', 'lanelet2', 'testdata')
    for name in ('two_lane_road', 'curve', 'intersection'):
        osm = os.path.join(testdata, name + '.osm')
        map_dir = run.import_map(name, osm, ['--lanelet2_origin=' + ORIGIN])
        if map_dir:
            run.check_import(name, osm, map_dir)

    # 3. round trips of the Apollo maps of the repository
    data = os.path.join(args.apollo_root, 'modules', 'map')
    # the demo map has no usable projection in its header: UTM zone 10
    run.round_trip('demo', os.path.join(data, 'data', 'demo'),
                   ['--utm_zone', '10'])
    # the same with centerlines recomputed from the bounds by the adapter
    run.round_trip('demo', os.path.join(data, 'data', 'demo'),
                   ['--utm_zone', '10', '--centerline', 'none'],
                   suffix='_computed_centerline',
                   compare_flags=['--center_tolerance', '0.2',
                                  '--length_tolerance', '0.1',
                                  '--width_tolerance', '0.1'])
    run.round_trip('borregas_ave', os.path.join(data, 'data', 'borregas_ave'))
    run.round_trip('hdmap_test_data',
                   os.path.join(data, 'hdmap', 'test-data', 'base_map.bin'))

    # 4. robustness
    if args.fuzz and example:
        run.fuzz(example, args.fuzz)

    with open(os.path.join(args.work_dir, 'summary.md'), 'w') as f:
        f.write('| step | result | details |\n|---|---|---|\n')
        for step, result, details in run.rows:
            f.write('| %s | %s | %s |\n' % (step, result, details))
    print('\n%d steps: %d passed, %d failed, %d skipped; summary: %s' % (
        len(run.rows), sum(1 for r in run.rows if r[1] == 'PASS'),
        sum(1 for r in run.rows if r[1] == 'FAIL'),
        sum(1 for r in run.rows if r[1] == 'SKIP'),
        os.path.join(args.work_dir, 'summary.md')))
    return 1 if run.failed else 0


if __name__ == '__main__':
    sys.exit(main())
