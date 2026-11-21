#!/usr/bin/env python3

# ****************************************************************************
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
# ****************************************************************************
"""Cyber channel / node name -> ROS 2 name mapping.

These rules are the team contract (docs/ros2/architecture.md) and must stay
identical to cyber::ros2::ChannelToTopic()/ToNodeName() (cyber/ros2/naming.cc)
and tools/ros2/apollo_ros2_common.py. They are duplicated here on purpose so
that cyber_py3 has no runtime dependency on tools/.

Channel -> topic:
  1. Replace every character outside [A-Za-z0-9_/] with '_'.
  2. Prefix '/' if the name is not absolute.
  3. Collapse repeated '/' and drop any trailing '/'.
  4. Prefix any '/'-separated token that starts with a digit with '_'.
  5. If the result is exactly one of RESERVED_TOPICS (names ROS 2 reserves
     for standard message types), prefix it with '/apollo'; e.g. the Apollo
     channel '/tf' becomes '/apollo/tf' while '/tf/extra' is unchanged.

Node names: characters outside [A-Za-z0-9_] become '_', a leading digit gets a
'_' prefix and an empty name becomes 'apollo_node'.
"""

import re

_TOPIC_INVALID = re.compile(r'[^A-Za-z0-9_/]')
_NODE_INVALID = re.compile(r'[^A-Za-z0-9_]')

DEFAULT_NODE_NAME = 'apollo_node'
RESERVED_TOPICS = ('/tf', '/tf_static', '/clock', '/rosout',
                   '/parameter_events')


def channel_to_topic(channel):
    """Maps a Cyber channel name onto a valid ROS 2 topic/service name.

    >>> channel_to_topic('/apollo/sensor/lidar-16/PointCloud2')
    '/apollo/sensor/lidar_16/PointCloud2'
    >>> channel_to_topic('channel/chatter')
    '/channel/chatter'
    >>> channel_to_topic('/tf')
    '/apollo/tf'
    """
    if not channel:
        return channel
    name = _TOPIC_INVALID.sub('_', channel)
    if not name.startswith('/'):
        name = '/' + name
    tokens = [t for t in name.split('/') if t]
    tokens = ['_' + t if t[0].isdigit() else t for t in tokens]
    topic = '/' + '/'.join(tokens)
    if topic in RESERVED_TOPICS:
        topic = '/apollo' + topic
    return topic


def to_node_name(name):
    """Maps a Cyber node / component name onto a valid ROS 2 node name.

    >>> to_node_name('3d-obstacle tracker')
    '_3d_obstacle_tracker'
    """
    if not name:
        return DEFAULT_NODE_NAME
    name = _NODE_INVALID.sub('_', name)
    if name[0].isdigit():
        name = '_' + name
    return name
