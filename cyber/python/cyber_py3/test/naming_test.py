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
"""Channel -> topic and node name rules (docs/ros2/architecture.md)."""

import importlib.util
import os

import pytest

from conftest import APOLLO_ROOT

from cyber.python.cyber_py3.naming import channel_to_topic
from cyber.python.cyber_py3.naming import to_node_name

CHANNELS = [
    ('/apollo/sensor/lidar-16/PointCloud2', '/apollo/sensor/lidar_16/PointCloud2'),
    ('channel/chatter', '/channel/chatter'),
    ('/apollo/control', '/apollo/control'),
    ('//apollo///planning//', '/apollo/planning'),
    ('/apollo/3d/obstacles', '/apollo/_3d/obstacles'),
    ('4d.radar', '/_4d_radar'),
    ('/a b/c.d/e:f', '/a_b/c_d/e_f'),
    ('/', '/'),
    ('///', '/'),
    ('', ''),
    ('/apollo/_hidden', '/apollo/_hidden'),
    ('/apollo/sensor/camera/front_6mm/image/compressed',
     '/apollo/sensor/camera/front_6mm/image/compressed'),
    ('rel', '/rel'),
    # rule 5: names reserved for standard ROS 2 types move under /apollo
    ('/tf', '/apollo/tf'),
    ('tf', '/apollo/tf'),
    ('/tf/', '/apollo/tf'),
    ('/tf_static', '/apollo/tf_static'),
    ('/clock', '/apollo/clock'),
    ('/rosout', '/apollo/rosout'),
    ('/parameter_events', '/apollo/parameter_events'),
    ('/tf/extra', '/tf/extra'),
    ('/apollo/tf', '/apollo/tf'),
    ('/tf2', '/tf2'),
    ('/rosout_agg', '/rosout_agg'),
]

NODES = [
    ('listener', 'listener'),
    ('talker-1', 'talker_1'),
    ('3d obstacle', '_3d_obstacle'),
    ('', 'apollo_node'),
    ('a/b.c', 'a_b_c'),
    ('_x', '_x'),
]


@pytest.mark.parametrize('channel,topic', CHANNELS)
def test_channel_to_topic(channel, topic):
    assert channel_to_topic(channel) == topic


@pytest.mark.parametrize('channel,topic', CHANNELS)
def test_channel_to_topic_is_idempotent(channel, topic):
    assert channel_to_topic(topic) == topic


@pytest.mark.parametrize('name,node', NODES)
def test_to_node_name(name, node):
    assert to_node_name(name) == node


def _load_tools_common():
    path = os.path.join(APOLLO_ROOT, 'tools', 'ros2', 'apollo_ros2_common.py')
    if not os.path.exists(path):
        pytest.skip('tools/ros2/apollo_ros2_common.py not present')
    spec = importlib.util.spec_from_file_location('apollo_ros2_common', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_reserved_topics_match_tools_ros2_common():
    common = _load_tools_common()
    from cyber.python.cyber_py3 import naming
    assert naming.RESERVED_TOPICS == common.RESERVED_TOPICS


def test_matches_tools_ros2_common():
    common = _load_tools_common()
    corpus = [c for c, _ in CHANNELS] + [n for n, _ in NODES] + [
        '/apollo/canbus/chassis', 'a--b', '9', '/x/0/y', 'ü/é', '/a/./b',
        '//tf', 'tf_static/', '/clock/x']
    for name in corpus:
        assert channel_to_topic(name) == common.channel_to_topic(name), name
        assert to_node_name(name) == common.to_node_name(name), name


def test_suite_tests_the_source_tree():
    """conftest must make the source tree win over an installed copy."""
    if os.environ.get('CYBER_PY3_TEST_INSTALLED'):
        pytest.skip('testing the installed package')
    from cyber.python.cyber_py3 import naming
    assert os.path.realpath(naming.__file__).startswith(
        os.path.realpath(APOLLO_ROOT) + os.sep)
