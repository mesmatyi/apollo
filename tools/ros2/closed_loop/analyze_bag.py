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
"""Analyzes a rosbag2 (MCAP or sqlite3) recording of an Apollo closed-loop run.

  analyze_bag.py <bag_dir> [--goal X Y] [--map_dir DIR --lane ID] [--json OUT]

Apollo topics carry apollo_msgs/msg/ProtoMessage; their protobuf payloads are
decoded to compute message rates, the driven path, the lateral deviation from
the reference lane, planning/control health and the distance to the goal.
"""

import argparse
import json
import math
import os
import sys

import rosbag2_py
from rclpy.serialization import deserialize_message

from apollo_msgs.msg import ProtoMessage
from modules.common_msgs.chassis_msgs import chassis_pb2
from modules.common_msgs.control_msgs import control_cmd_pb2
from modules.common_msgs.localization_msgs import localization_pb2
from modules.common_msgs.planning_msgs import planning_command_pb2
from modules.common_msgs.planning_msgs import planning_pb2

DECODERS = {
    'apollo.localization.LocalizationEstimate':
        localization_pb2.LocalizationEstimate,
    'apollo.planning.ADCTrajectory': planning_pb2.ADCTrajectory,
    'apollo.control.ControlCommand': control_cmd_pb2.ControlCommand,
    'apollo.canbus.Chassis': chassis_pb2.Chassis,
    'apollo.planning.PlanningCommand': planning_command_pb2.PlanningCommand,
}


def storage_id_of(bag_dir):
    meta = os.path.join(bag_dir, 'metadata.yaml')
    if os.path.exists(meta):
        with open(meta) as f:
            for line in f:
                if 'storage_identifier' in line:
                    return line.split(':', 1)[1].strip()
    return 'mcap'


def distance_to_polyline(x, y, points):
    best = float('inf')
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        dx, dy = x1 - x0, y1 - y0
        seg2 = dx * dx + dy * dy
        t = 0.0 if seg2 == 0 else max(0.0, min(1.0, ((x - x0) * dx +
                                                    (y - y0) * dy) / seg2))
        px, py = x0 + t * dx, y0 + t * dy
        best = min(best, math.hypot(x - px, y - py))
    return best


def analyze(bag_dir, goal=None, lane_points=None, front_edge_to_center=None):
    storage_id = storage_id_of(bag_dir)
    reader = rosbag2_py.SequentialReader()
    reader.open(rosbag2_py.StorageOptions(uri=bag_dir, storage_id=storage_id),
                rosbag2_py.ConverterOptions('', ''))
    topic_types = {t.name: t.type for t in reader.get_all_topics_and_types()}

    counts = {}
    first_t, last_t = {}, {}
    proto_types = {}
    poses = []
    speeds = []
    planning = {'messages': 0, 'with_trajectory': 0, 'estop': 0,
                'max_points': 0, 'command_received': False}
    control = {'messages': 0, 'max_abs_acceleration': 0.0,
               'max_abs_steering_target': 0.0}
    decode_errors = 0

    while reader.has_next():
        topic, data, t_ns = reader.read_next()
        counts[topic] = counts.get(topic, 0) + 1
        first_t.setdefault(topic, t_ns)
        last_t[topic] = t_ns
        if topic_types.get(topic) != 'apollo_msgs/msg/ProtoMessage':
            continue
        wire = deserialize_message(data, ProtoMessage)
        proto_types[topic] = wire.type
        cls = DECODERS.get(wire.type)
        if cls is None:
            continue
        msg = cls()
        try:
            msg.ParseFromString(bytes(wire.data))
        except Exception:  # pylint: disable=broad-except
            decode_errors += 1
            continue
        if topic == '/apollo/localization/pose':
            p = msg.pose.position
            poses.append((msg.header.timestamp_sec, p.x, p.y, msg.pose.heading))
        elif topic == '/apollo/canbus/chassis':
            speeds.append(msg.speed_mps)
        elif topic == '/apollo/planning':
            planning['messages'] += 1
            n = len(msg.trajectory_point)
            if n > 0:
                planning['with_trajectory'] += 1
            planning['max_points'] = max(planning['max_points'], n)
            if msg.estop.is_estop:
                planning['estop'] += 1
        elif topic == '/apollo/control':
            control['messages'] += 1
            control['max_abs_acceleration'] = max(
                control['max_abs_acceleration'], abs(msg.acceleration))
            control['max_abs_steering_target'] = max(
                control['max_abs_steering_target'], abs(msg.steering_target))
        elif topic == '/apollo/planning/command':
            planning['command_received'] = True

    rates = {}
    for topic, n in counts.items():
        span = (last_t[topic] - first_t[topic]) / 1e9
        rates[topic] = round((n - 1) / span, 2) if n > 1 and span > 0 else None

    path_length = 0.0
    for (_, x0, y0, _), (_, x1, y1, _) in zip(poses, poses[1:]):
        path_length += math.hypot(x1 - x0, y1 - y0)
    result = {
        'bag': bag_dir,
        'storage_id': storage_id,
        'topics': {t: {'messages': counts[t], 'rate_hz': rates[t],
                       'type': topic_types.get(t),
                       'proto_type': proto_types.get(t)}
                   for t in sorted(counts)},
        'planning': planning,
        'control': control,
        'decode_errors': decode_errors,
        'path_length_m': round(path_length, 2),
        'max_speed_mps': round(max(speeds), 3) if speeds else 0.0,
        'final_speed_mps': round(speeds[-1], 3) if speeds else None,
    }
    if poses:
        result['start'] = {'x': poses[0][1], 'y': poses[0][2]}
        result['end'] = {'x': poses[-1][1], 'y': poses[-1][2]}
    if goal and poses:
        _, x, y, heading = poses[-1]
        result['final_distance_to_goal_m'] = round(
            math.hypot(x - goal[0], y - goal[1]), 3)
        if front_edge_to_center is not None:
            fx = x + front_edge_to_center * math.cos(heading)
            fy = y + front_edge_to_center * math.sin(heading)
            result['final_front_distance_to_goal_m'] = round(
                math.hypot(fx - goal[0], fy - goal[1]), 3)
    if lane_points and poses:
        lat = [distance_to_polyline(x, y, lane_points)
               for _, x, y, _ in poses[::10]]
        result['lateral_deviation_m'] = {
            'max': round(max(lat), 3),
            'mean': round(sum(lat) / len(lat), 3)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('bag')
    parser.add_argument('--goal', type=float, nargs=2, metavar=('X', 'Y'))
    parser.add_argument('--map_dir')
    parser.add_argument('--lane')
    parser.add_argument('--front_edge_to_center', type=float,
                        help='also report the front bumper distance to goal')
    parser.add_argument('--json', help='write the result to this file')
    args = parser.parse_args()
    lane = None
    if args.map_dir and args.lane:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import vehicle_sim  # pylint: disable=import-outside-toplevel
        lane = vehicle_sim.lane_points(vehicle_sim.load_map(args.map_dir),
                                       args.lane)
    result = analyze(args.bag, args.goal, lane, args.front_edge_to_center)
    text = json.dumps(result, indent=2)
    if args.json:
        with open(args.json, 'w') as f:
            f.write(text + '\n')
    print(text)


if __name__ == '__main__':
    sys.exit(main())
