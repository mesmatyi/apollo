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
"""Apollo closed-loop (software-in-the-loop) test on ROS 2, recorded to MCAP.

  tools/ros2/closed_loop/run_closed_loop.py --runs 3 --storage mcap

Each run:
  1. starts `ros2 bag record` of all topics (--storage mcap|sqlite3),
  2. starts the vehicle simulator and the Apollo stack (external command /
     routing, planning, control in one ROS 2 component container),
  3. sends a LaneFollowCommand to the goal,
  4. waits until the vehicle stops within --goal_tolerance of the goal
     (or --timeout, or a crash),
  5. shuts everything down (the bag is closed cleanly) and analyzes the bag.

Writes <output_dir>/run_<n>/{bag/,report.json,*.log} and
<output_dir>/summary.{json,md}. Exits non-zero unless every run passes.
"""

import argparse
import datetime
import json
import math
import os
import re
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import analyze_bag  # noqa: E402  pylint: disable=wrong-import-position
import vehicle_sim  # noqa: E402  pylint: disable=wrong-import-position


def default_goal(map_dir, lane):
    """Goal from the map's default_end_way_point.txt, else the lane end."""
    points = vehicle_sim.lane_points(vehicle_sim.load_map(map_dir), lane)
    path = os.path.join(map_dir, 'default_end_way_point.txt')
    if os.path.exists(path):
        with open(path) as f:
            text = f.read()
        xs = re.findall(r'\bx:\s*([-\d.eE+]+)', text)
        ys = re.findall(r'\by:\s*([-\d.eE+]+)', text)
        if xs and ys:
            gx, gy = float(xs[0]), float(ys[0])
            # heading of the lane at the point closest to the goal
            best = min(range(len(points) - 1),
                       key=lambda i: math.hypot(points[i][0] - gx,
                                                points[i][1] - gy))
            (x0, y0), (x1, y1) = points[best], points[best + 1]
            return gx, gy, math.atan2(y1 - y0, x1 - x0), points
    length = sum(math.hypot(b[0] - a[0], b[1] - a[1])
                 for a, b in zip(points, points[1:]))
    gx, gy, gh = vehicle_sim.pose_on_polyline(points, max(0.0, length - 1.0))
    return gx, gy, gh, points


def front_edge_to_center(apollo_root):
    """Distance from the vehicle reference point (rear axle center, which
    localization reports) to the front bumper."""
    from google.protobuf import text_format  # pylint: disable=import-outside-toplevel
    from modules.common_msgs.config_msgs import vehicle_config_pb2  # noqa
    param = vehicle_config_pb2.VehicleConfig()
    path = os.path.join(apollo_root, 'modules/common/data/vehicle_param.pb.txt')
    with open(path) as f:
        text_format.Parse(f.read(), param)
    return param.vehicle_param.front_edge_to_center


def front_point(x, y, heading, offset):
    return x + offset * math.cos(heading), y + offset * math.sin(heading)


def start(cmd, log_path, env):
    log = open(log_path, 'w')
    proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT, env=env,
                            start_new_session=True)
    proc.log_file = log
    return proc


def stop(proc, timeout=20.0):
    """SIGINT the whole process group, then SIGKILL after `timeout`."""
    if proc is None:
        return None
    if proc.poll() is None:
        try:
            os.killpg(proc.pid, signal.SIGINT)
        except ProcessLookupError:
            pass
        try:
            proc.wait(timeout)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
    proc.log_file.close()
    return proc.returncode


def read_status(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def run_once(args, index, goal, lane_points):
    run_dir = os.path.join(args.output_dir, 'run_%02d' % index)
    os.makedirs(run_dir, exist_ok=True)
    env = dict(os.environ)
    env['ROS_DOMAIN_ID'] = str(args.domain_id_base + index)
    env['APOLLO_ROOT'] = args.apollo_root
    env['OMP_NUM_THREADS'] = '1'
    env['GLOG_log_dir'] = run_dir
    env.setdefault('GLOG_alsologtostderr', '1')
    status_file = os.path.join(run_dir, 'vehicle_status.json')
    bag_dir = os.path.join(run_dir, 'bag')
    result = {'run': index, 'run_dir': run_dir,
              'domain_id': int(env['ROS_DOMAIN_ID']),
              'started': datetime.datetime.now().isoformat(timespec='seconds')}
    procs = {}
    t0 = time.monotonic()
    try:
        procs['bag'] = start(['ros2', 'bag', 'record', '-a', '-s', args.storage,
                              '-o', bag_dir], os.path.join(run_dir, 'bag.log'),
                             env)
        time.sleep(2.0)
        procs['sim'] = start([sys.executable, os.path.join(HERE, 'vehicle_sim.py'),
                              '--map_dir', args.map_dir,
                              '--start_lane', args.lane,
                              '--start_s', str(args.start_s),
                              '--status_file', status_file],
                             os.path.join(run_dir, 'vehicle_sim.log'), env)
        procs['stack'] = start(['ros2', 'launch',
                                os.path.join(HERE, 'closed_loop.launch.py'),
                                'apollo_root:=' + args.apollo_root,
                                'map_dir:=' + args.map_dir],
                               os.path.join(run_dir, 'stack.log'), env)

        # 1. wait until control is producing commands (stack is up)
        ready_deadline = time.monotonic() + args.startup_timeout
        while time.monotonic() < ready_deadline:
            st = read_status(status_file)
            if st and st.get('control_commands', 0) > 100:
                break
            if procs['stack'].poll() is not None:
                raise RuntimeError('Apollo stack exited during start-up')
            time.sleep(0.5)
        else:
            raise RuntimeError('stack not ready after %ds' % args.startup_timeout)
        result['startup_s'] = round(time.monotonic() - t0, 1)
        time.sleep(2.0)

        # 2. send the driving command
        if args.command_with == 'ros2cli':
            # The stock ROS 2 command line, which knows nothing about Apollo
            # but the generated service type. CommandStatus.status: 1 =
            # RUNNING, 2 = FINISHED (the route was accepted).
            request = ('{header: {module_name: closed_loop_test}, '
                       'command_id: %d, end_pose: {x: %r, y: %r, '
                       'heading: %r}, target_speed: %r}' % (
                           index + 1, goal[0], goal[1], goal[2],
                           args.target_speed))
            send = subprocess.run(
                ['ros2', 'service', 'call',
                 '/apollo/external_command/lane_follow',
                 'apollo_msgs/srv/ExternalCommandLaneFollowCommand', request],
                env=env, capture_output=True, text=True, timeout=120)
            accepted = send.returncode == 0 and re.search(
                r'\bstatus=[12]\b', send.stdout) is not None
        else:
            send = subprocess.run(
                [sys.executable, os.path.join(HERE, 'send_lane_follow.py'),
                 str(goal[0]), str(goal[1]), '--heading', str(goal[2]),
                 '--speed', str(args.target_speed), '--id', str(index + 1)],
                env=env, capture_output=True, text=True, timeout=120)
            accepted = send.returncode == 0
        with open(os.path.join(run_dir, 'command.log'), 'w') as f:
            f.write(send.stdout + send.stderr)
        if not accepted:
            raise RuntimeError('lane follow command was not accepted')
        t_cmd = time.monotonic()

        # 3. drive until stopped at the goal
        stopped_since = None
        best = float('inf')
        reason = 'timeout'
        while time.monotonic() - t_cmd < args.timeout:
            if procs['stack'].poll() is not None:
                reason = 'Apollo stack exited (code %s)' % procs['stack'].returncode
                break
            if procs['sim'].poll() is not None:
                reason = 'vehicle simulator exited'
                break
            st = read_status(status_file)
            if st:
                # Planning stops the vehicle with its front bumper at the
                # destination stop line; localization reports the rear axle.
                fx, fy = front_point(st['x'], st['y'], st['heading'],
                                     args.front_edge_to_center)
                dist = math.hypot(fx - goal[0], fy - goal[1])
                best = min(best, dist)
                if dist <= args.goal_tolerance and st['speed'] < 0.2:
                    stopped_since = stopped_since or time.monotonic()
                    if time.monotonic() - stopped_since >= 2.0:
                        reason = 'reached'
                        break
                else:
                    stopped_since = None
            time.sleep(0.5)
        result['drive_s'] = round(time.monotonic() - t_cmd, 1)
        result['outcome'] = reason
        result['closest_approach_m'] = round(best, 3)
    except Exception as e:  # pylint: disable=broad-except
        result['outcome'] = 'error: %s' % e
    finally:
        for name in ('stack', 'sim', 'bag'):
            result['%s_exit_code' % name] = stop(procs.get(name))

    final = read_status(status_file) or {}
    result['vehicle'] = final
    try:
        result['bag_analysis'] = analyze_bag.analyze(
            bag_dir, goal[:2], lane_points,
            front_edge_to_center=args.front_edge_to_center)
    except Exception as e:  # pylint: disable=broad-except
        result['bag_analysis'] = {'error': str(e)}

    bag = result['bag_analysis']
    checks = {
        'reached_goal': result.get('outcome') == 'reached',
        'bag_storage_is_%s' % args.storage: bag.get('storage_id') == args.storage,
        'planning_published_trajectories':
            bag.get('planning', {}).get('with_trajectory', 0) > 0,
        'control_published_commands': bag.get('control', {}).get('messages', 0) > 0,
        'no_planning_estop': bag.get('planning', {}).get('estop', 1) == 0,
        'no_decode_errors': bag.get('decode_errors', 1) == 0,
        'lateral_deviation_below_%.1fm' % args.max_lateral_deviation:
            bag.get('lateral_deviation_m', {}).get('max', 1e9)
            <= args.max_lateral_deviation,
        'stack_exited_cleanly': result.get('stack_exit_code') in (0, -2, 130),
    }
    result['checks'] = checks
    result['passed'] = all(checks.values())
    with open(os.path.join(run_dir, 'report.json'), 'w') as f:
        json.dump(result, f, indent=2)
    return result


def write_summary(args, goal, results):
    summary = {
        'scenario': {'map_dir': args.map_dir, 'lane': args.lane,
                     'start_s': args.start_s,
                     'goal': {'x': goal[0], 'y': goal[1], 'heading': goal[2]},
                     'target_speed': args.target_speed,
                     'storage': args.storage},
        'runs': len(results),
        'passed': sum(1 for r in results if r['passed']),
        'results': results,
    }
    with open(os.path.join(args.output_dir, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=2)
    lines = [
        '# Apollo closed-loop test (ROS 2, %s recording)' % args.storage, '',
        'Scenario: lane `%s` of `%s`, goal (%.2f, %.2f), target %.1f m/s.' % (
            args.lane, args.map_dir, goal[0], goal[1], args.target_speed), '',
        '| run | result | outcome | drive time (s) | path (m) | front bumper to goal (m) '
        '| max lateral dev (m) | max speed (m/s) | planning msgs | control msgs '
        '| bag |',
        '|---|---|---|---|---|---|---|---|---|---|---|']
    for r in results:
        b = r.get('bag_analysis', {})
        lines.append('| %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |' % (
            r['run'], 'PASS' if r['passed'] else 'FAIL', r.get('outcome'),
            r.get('drive_s', '-'), b.get('path_length_m', '-'),
            b.get('final_front_distance_to_goal_m', '-'),
            b.get('lateral_deviation_m', {}).get('max', '-'),
            b.get('max_speed_mps', '-'),
            b.get('planning', {}).get('messages', '-'),
            b.get('control', {}).get('messages', '-'),
            os.path.relpath(os.path.join(r['run_dir'], 'bag'), args.output_dir)))
    failed = [(r['run'], [k for k, v in r['checks'].items() if not v])
              for r in results if not r['passed']]
    lines += ['', '**%d / %d runs passed.**' % (summary['passed'], len(results))]
    for run, keys in failed:
        lines.append('- run %d failed: %s' % (run, ', '.join(keys)))
    with open(os.path.join(args.output_dir, 'summary.md'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--runs', type=int, default=1,
                        help='number of closed-loop runs (loop count)')
    parser.add_argument('--storage', default='mcap', choices=['mcap', 'sqlite3'],
                        help='rosbag2 storage of the recording')
    parser.add_argument('--output_dir', default=os.path.join(
        os.getcwd(), 'closed_loop_' +
        datetime.datetime.now().strftime('%Y%m%d_%H%M%S')))
    parser.add_argument('--apollo_root',
                        default=os.environ.get('APOLLO_ROOT', '/apollo'))
    parser.add_argument('--map_dir', default='')
    parser.add_argument('--lane', default='1_-1')
    parser.add_argument('--start_s', type=float, default=2.0)
    parser.add_argument('--goal', type=float, nargs=3,
                        metavar=('X', 'Y', 'HEADING'),
                        help='default: the map default_end_way_point')
    parser.add_argument('--target_speed', type=float, default=5.0)
    parser.add_argument('--goal_tolerance', type=float, default=3.0)
    parser.add_argument('--max_lateral_deviation', type=float, default=1.0)
    parser.add_argument('--timeout', type=float, default=180.0,
                        help='max driving time per run (s)')
    parser.add_argument('--startup_timeout', type=float, default=120.0)
    parser.add_argument('--domain_id_base', type=int, default=60)
    parser.add_argument('--command_with', default='cyber',
                        choices=['cyber', 'ros2cli'],
                        help='send the lane follow command with the Cyber '
                             'Python client (send_lane_follow.py) or with '
                             '`ros2 service call` and the native service type')
    args = parser.parse_args()

    args.apollo_root = os.path.abspath(args.apollo_root)
    args.front_edge_to_center = front_edge_to_center(args.apollo_root)
    if not args.map_dir:
        args.map_dir = os.path.join(args.apollo_root, 'modules/map/data/demo')
    args.map_dir = vehicle_sim.apollo_path(args.map_dir)
    os.makedirs(args.output_dir, exist_ok=True)

    gx, gy, gh, lane_points = default_goal(args.map_dir, args.lane)
    goal = tuple(args.goal) if args.goal else (gx, gy, gh)
    print('closed loop: %d run(s), goal (%.2f, %.2f, %.3f), %s recording -> %s'
          % (args.runs, goal[0], goal[1], goal[2], args.storage,
             args.output_dir), flush=True)
    results = []
    for i in range(args.runs):
        print('--- run %d/%d' % (i + 1, args.runs), flush=True)
        r = run_once(args, i, goal, lane_points)
        b = r.get('bag_analysis', {})
        print('    %s: %s, drive %ss, front bumper to goal %s m, max lateral %s m' % (
            'PASS' if r['passed'] else 'FAIL', r.get('outcome'),
            r.get('drive_s'), b.get('final_front_distance_to_goal_m'),
            b.get('lateral_deviation_m', {}).get('max')), flush=True)
        results.append(r)
    summary = write_summary(args, goal, results)
    return 0 if summary['passed'] == summary['runs'] else 1


if __name__ == '__main__':
    sys.exit(main())
