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
"""Vehicle simulator for Apollo closed-loop (software-in-the-loop) tests.

Closes the loop around planning and control without hardware:

  /apollo/control (ControlCommand) --> kinematic bicycle model -->
      /apollo/localization/pose (LocalizationEstimate, 100 Hz)
      /apollo/canbus/chassis    (Chassis, 100 Hz)
      /apollo/prediction        (empty PredictionObstacles, 10 Hz; triggers
                                 planning, which fuses chassis/localization)

The vehicle accepts control's `acceleration` (m/s^2) and `steering_target`
(% of the maximum steering-wheel angle), with first-order actuator lag, and
uses the vehicle parameters (wheel base, steer ratio, limits) of
modules/common/data/vehicle_param.pb.txt. The start pose is taken from a
lane of the HD map.

Runs on ROS 2 through the Cyber Python API (cyber_py3 on rclpy).
"""

import argparse
import json
import math
import os
import sys
import threading
import time

from google.protobuf import text_format

from cyber.python.cyber_py3 import cyber
from modules.common_msgs.chassis_msgs import chassis_pb2
from modules.common_msgs.config_msgs import vehicle_config_pb2
from modules.common_msgs.control_msgs import control_cmd_pb2
from modules.common_msgs.localization_msgs import localization_pb2
from modules.common_msgs.map_msgs import map_pb2
from modules.common_msgs.prediction_msgs import prediction_obstacle_pb2


def apollo_path(path):
    """Resolves /apollo/... against APOLLO_ROOT when /apollo is not present."""
    if path.startswith('/apollo/') and not os.path.exists(path):
        root = os.environ.get('APOLLO_ROOT', '')
        if root:
            candidate = os.path.join(root, path[len('/apollo/'):])
            if os.path.exists(candidate):
                return candidate
    return path


def load_map(map_dir):
    """Loads base_map.{bin,txt,xml-free variants} of an Apollo map dir."""
    for name in ('base_map.bin', 'base_map.txt'):
        path = os.path.join(map_dir, name)
        if not os.path.exists(path):
            continue
        hdmap = map_pb2.Map()
        if name.endswith('.bin'):
            with open(path, 'rb') as f:
                hdmap.ParseFromString(f.read())
        else:
            with open(path) as f:
                text_format.Parse(f.read(), hdmap)
        return hdmap
    raise FileNotFoundError('no base_map.bin/txt in %s' % map_dir)


def lane_points(hdmap, lane_id):
    for lane in hdmap.lane:
        if lane.id.id == lane_id:
            points = []
            for segment in lane.central_curve.segment:
                for p in segment.line_segment.point:
                    points.append((p.x, p.y))
            return points
    raise KeyError('lane %s not in map' % lane_id)


def pose_on_polyline(points, s):
    """(x, y, heading) at arc length s along a polyline."""
    travelled = 0.0
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        seg = math.hypot(x1 - x0, y1 - y0)
        if seg <= 1e-9:
            continue
        if travelled + seg >= s:
            r = (s - travelled) / seg
            return (x0 + r * (x1 - x0), y0 + r * (y1 - y0),
                    math.atan2(y1 - y0, x1 - x0))
        travelled += seg
    (x0, y0), (x1, y1) = points[-2], points[-1]
    return x1, y1, math.atan2(y1 - y0, x1 - x0)


def normalize_angle(a):
    return math.atan2(math.sin(a), math.cos(a))


def heading_to_quaternion(heading):
    """Apollo localization orientation: vehicle frame is Right-Forward-Up, so
    the quaternion's yaw is heading - pi/2. Returns (qw, qx, qy, qz)."""
    yaw = heading - math.pi / 2.0
    return math.cos(yaw / 2.0), 0.0, 0.0, math.sin(yaw / 2.0)


class VehicleSim(object):

    def __init__(self, args):
        self.args = args
        param = vehicle_config_pb2.VehicleConfig()
        with open(apollo_path(args.vehicle_param)) as f:
            text_format.Parse(f.read(), param)
        vp = param.vehicle_param
        self.wheel_base = vp.wheel_base
        self.max_steer = vp.max_steer_angle / vp.steer_ratio  # front wheel
        self.max_acc = vp.max_acceleration
        self.max_dec = vp.max_deceleration

        hdmap = load_map(apollo_path(args.map_dir))
        x, y, heading = pose_on_polyline(lane_points(hdmap, args.start_lane),
                                         args.start_s)
        self.x, self.y, self.heading = x, y, heading
        self.v = 0.0
        self.a = 0.0
        self.steer = 0.0  # front wheel angle (rad)
        self.yaw_rate = 0.0

        self.lock = threading.Lock()
        self.cmd = None
        self.cmd_count = 0
        self.seq = 0
        self.stats = {'control_commands': 0, 'max_speed': 0.0,
                      'distance': 0.0}

        self.node = cyber.Node('closed_loop_vehicle_sim')
        self.loc_writer = self.node.create_writer(
            '/apollo/localization/pose', localization_pb2.LocalizationEstimate,
            10)
        self.chassis_writer = self.node.create_writer(
            '/apollo/canbus/chassis', chassis_pb2.Chassis, 10)
        self.prediction_writer = self.node.create_writer(
            '/apollo/prediction', prediction_obstacle_pb2.PredictionObstacles,
            10)
        self.node.create_reader('/apollo/control',
                                control_cmd_pb2.ControlCommand,
                                self.on_control, qos_depth=10)

    def on_control(self, cmd):
        with self.lock:
            self.cmd = cmd
            self.stats['control_commands'] += 1

    def step(self, dt):
        with self.lock:
            cmd = self.cmd
        a_cmd = 0.0
        steer_cmd = self.steer
        driving = cmd is not None and (
            not cmd.HasField('gear_location') or
            cmd.gear_location == chassis_pb2.Chassis.GEAR_DRIVE)
        if cmd is not None:
            if cmd.HasField('acceleration'):
                a_cmd = cmd.acceleration
            elif cmd.HasField('throttle') or cmd.HasField('brake'):
                a_cmd = (cmd.throttle * self.max_acc -
                         cmd.brake * -self.max_dec) / 100.0
            if cmd.HasField('steering_target'):
                steer_cmd = cmd.steering_target / 100.0 * self.max_steer
        if not driving:
            a_cmd = min(a_cmd, 0.0) if self.v > 0 else 0.0
        a_cmd = max(self.max_dec, min(self.max_acc, a_cmd))
        steer_cmd = max(-self.max_steer, min(self.max_steer, steer_cmd))

        # first-order actuator dynamics
        self.a += (a_cmd - self.a) * min(1.0, dt / self.args.acc_lag)
        self.steer += (steer_cmd - self.steer) * min(1.0,
                                                     dt / self.args.steer_lag)

        v_prev = self.v
        self.v = max(0.0, self.v + self.a * dt)
        if self.v == 0.0 and self.a < 0.0:
            self.a = 0.0
        v_mid = 0.5 * (v_prev + self.v)
        self.yaw_rate = v_mid / self.wheel_base * math.tan(self.steer)
        heading_mid = self.heading + 0.5 * self.yaw_rate * dt
        self.x += v_mid * math.cos(heading_mid) * dt
        self.y += v_mid * math.sin(heading_mid) * dt
        self.heading = normalize_angle(self.heading + self.yaw_rate * dt)
        self.stats['distance'] += v_mid * dt
        self.stats['max_speed'] = max(self.stats['max_speed'], self.v)

    def publish(self, now, with_prediction):
        self.seq += 1
        loc = localization_pb2.LocalizationEstimate()
        loc.header.timestamp_sec = now
        loc.header.module_name = 'closed_loop_vehicle_sim'
        loc.header.sequence_num = self.seq
        loc.measurement_time = now
        pose = loc.pose
        pose.position.x, pose.position.y, pose.position.z = self.x, self.y, 0.0
        qw, qx, qy, qz = heading_to_quaternion(self.heading)
        pose.orientation.qw, pose.orientation.qx = qw, qx
        pose.orientation.qy, pose.orientation.qz = qy, qz
        pose.heading = self.heading
        c, s = math.cos(self.heading), math.sin(self.heading)
        pose.linear_velocity.x = self.v * c
        pose.linear_velocity.y = self.v * s
        pose.linear_velocity.z = 0.0
        pose.linear_acceleration.x = self.a * c
        pose.linear_acceleration.y = self.a * s
        pose.linear_acceleration.z = 0.0
        pose.angular_velocity.x = pose.angular_velocity.y = 0.0
        pose.angular_velocity.z = self.yaw_rate
        # vehicle reference frame (RFU): forward is +y
        pose.linear_acceleration_vrf.x = 0.0
        pose.linear_acceleration_vrf.y = self.a
        pose.linear_acceleration_vrf.z = 0.0
        pose.angular_velocity_vrf.x = pose.angular_velocity_vrf.y = 0.0
        pose.angular_velocity_vrf.z = self.yaw_rate
        pose.euler_angles.x = pose.euler_angles.y = 0.0
        pose.euler_angles.z = self.heading - math.pi / 2.0
        self.loc_writer.write(loc)

        chassis = chassis_pb2.Chassis()
        chassis.header.timestamp_sec = now
        chassis.header.module_name = 'closed_loop_vehicle_sim'
        chassis.header.sequence_num = self.seq
        chassis.engine_started = True
        chassis.speed_mps = self.v
        chassis.driving_mode = chassis_pb2.Chassis.COMPLETE_AUTO_DRIVE
        chassis.gear_location = chassis_pb2.Chassis.GEAR_DRIVE
        chassis.error_code = chassis_pb2.Chassis.NO_ERROR
        chassis.throttle_percentage = max(0.0, self.a / self.max_acc * 100.0)
        chassis.brake_percentage = max(0.0, self.a / self.max_dec * 100.0)
        chassis.steering_percentage = self.steer / self.max_steer * 100.0
        chassis.parking_brake = False
        self.chassis_writer.write(chassis)

        if with_prediction:
            prediction = prediction_obstacle_pb2.PredictionObstacles()
            prediction.header.timestamp_sec = now
            prediction.header.module_name = 'prediction'
            prediction.header.sequence_num = self.seq
            prediction.start_timestamp = now
            prediction.end_timestamp = now
            self.prediction_writer.write(prediction)

    def status(self):
        return {'x': self.x, 'y': self.y, 'heading': self.heading,
                'speed': self.v, **self.stats}

    def run(self):
        dt = 1.0 / self.args.rate
        tick = 0
        next_t = time.monotonic()
        last_status = 0.0
        while not cyber.is_shutdown():
            now = time.time()
            self.step(dt)
            self.publish(now, tick % max(1, int(self.args.rate / 10)) == 0)
            tick += 1
            if self.args.status_file and now - last_status > 0.2:
                last_status = now
                tmp = self.args.status_file + '.tmp'
                with open(tmp, 'w') as f:
                    json.dump(self.status(), f)
                os.replace(tmp, self.args.status_file)
            next_t += dt
            delay = next_t - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            else:
                next_t = time.monotonic()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--map_dir',
                        default='/apollo/modules/map/data/demo')
    parser.add_argument('--start_lane', default='1_-1')
    parser.add_argument('--start_s', type=float, default=2.0)
    parser.add_argument('--vehicle_param',
                        default='/apollo/modules/common/data/'
                                'vehicle_param.pb.txt')
    parser.add_argument('--rate', type=float, default=100.0,
                        help='simulation / publish rate (Hz)')
    parser.add_argument('--acc_lag', type=float, default=0.2,
                        help='longitudinal actuator time constant (s)')
    parser.add_argument('--steer_lag', type=float, default=0.1,
                        help='steering actuator time constant (s)')
    parser.add_argument('--status_file', default='',
                        help='write the vehicle state here (JSON, 5 Hz)')
    args = parser.parse_args()

    cyber.init('closed_loop_vehicle_sim')
    sim = VehicleSim(args)
    print('vehicle sim: start pose x=%.2f y=%.2f heading=%.3f on lane %s' %
          (sim.x, sim.y, sim.heading, args.start_lane), flush=True)
    try:
        sim.run()
    finally:
        cyber.shutdown()


if __name__ == '__main__':
    sys.exit(main())
