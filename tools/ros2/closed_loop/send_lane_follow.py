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
"""Sends an Apollo LaneFollowCommand to the external command service.

  send_lane_follow.py X Y [--heading H] [--speed V] [--id N]

Prints the CommandStatus and exits 0 when the command was accepted.
"""

import argparse
import sys
import time

from cyber.python.cyber_py3 import cyber
from modules.common_msgs.external_command_msgs import command_status_pb2
from modules.common_msgs.external_command_msgs import lane_follow_command_pb2

SERVICE = '/apollo/external_command/lane_follow'


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('x', type=float)
    parser.add_argument('y', type=float)
    parser.add_argument('--heading', type=float)
    parser.add_argument('--speed', type=float, help='target speed (m/s)')
    parser.add_argument('--id', type=int, default=1, help='command id')
    parser.add_argument('--timeout', type=float, default=10.0)
    parser.add_argument('--retries', type=int, default=5)
    args = parser.parse_args()

    cyber.init('closed_loop_command')
    node = cyber.Node('closed_loop_command')
    client = node.create_client(SERVICE,
                                lane_follow_command_pb2.LaneFollowCommand,
                                command_status_pb2.CommandStatus)
    cmd = lane_follow_command_pb2.LaneFollowCommand()
    cmd.header.module_name = 'closed_loop_test'
    cmd.command_id = args.id
    cmd.end_pose.x = args.x
    cmd.end_pose.y = args.y
    if args.heading is not None:
        cmd.end_pose.heading = args.heading
    if args.speed is not None:
        cmd.target_speed = args.speed
    accepted = False
    for attempt in range(args.retries):
        cmd.header.timestamp_sec = time.time()
        status = client.send_request(cmd, timeout=args.timeout)
        print('attempt %d: %s' % (attempt + 1,
                                  status if status is None else
                                  str(status).replace('\n', ' ')), flush=True)
        if status is not None and status.status in (
                command_status_pb2.CommandStatusType.RUNNING,
                command_status_pb2.CommandStatusType.FINISHED):
            accepted = True
            break
        time.sleep(2.0)
    cyber.shutdown()
    return 0 if accepted else 1


if __name__ == '__main__':
    sys.exit(main())
