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
"""cyber_channel / cyber_node / cyber_service and the rosbag2 converters."""

import os
import signal
import subprocess
import sys
import time

import pytest

from conftest import TOOLS_DIR
from conftest import requires_cyber_protos
from conftest import requires_ros
from conftest import spawn_python

pytestmark = [requires_ros, requires_cyber_protos]

CYBER_CHANNEL = os.path.join(TOOLS_DIR, 'cyber_channel', 'cyber_channel.py')
CYBER_NODE = os.path.join(TOOLS_DIR, 'cyber_node', 'cyber_node.py')
CYBER_SERVICE = os.path.join(TOOLS_DIR, 'cyber_service', 'cyber_service.py')
TO_BAG = os.path.join(TOOLS_DIR, 'cyber_recorder', 'record_to_rosbag2.py')
TO_RECORD = os.path.join(TOOLS_DIR, 'cyber_recorder', 'rosbag2_to_record.py')
GOLDEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data',
                      'cpp_writer.record')

TALKER = '''
import sys, time
from cyber.python.cyber_py3 import cyber
from cyber.proto.unit_test_pb2 import ChatterBenchmark
cyber.init()
node = cyber.Node("tool_talker_" + sys.argv[1])
writer = node.create_writer("/apollo/tool-test/" + sys.argv[1],
                            ChatterBenchmark)
node.create_service("tool_srv_" + sys.argv[1], ChatterBenchmark,
                    ChatterBenchmark, lambda r: r)
print("ready", flush=True)
seq = 0
while not cyber.is_shutdown():
    seq += 1
    writer.write(ChatterBenchmark(content="tool test", seq=seq))
    time.sleep(0.05)
cyber.shutdown()
'''


def run_tool(*args, timeout=60):
    return subprocess.run([sys.executable] + list(args),
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          universal_newlines=True, timeout=timeout,
                          env=dict(os.environ))


def run_until_sigint(seconds, *args):
    proc = subprocess.Popen([sys.executable, '-u'] + list(args),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, env=dict(os.environ))
    time.sleep(seconds)
    proc.send_signal(signal.SIGINT)
    out, err = proc.communicate(timeout=20)
    return proc.returncode, out, err


@pytest.fixture(scope='module')
def talker():
    name = 'p%d' % os.getpid()
    proc = spawn_python(TALKER, name)
    assert proc.stdout.readline().strip() == 'ready', proc.stderr.read()
    yield name
    alive = proc.poll() is None
    proc.terminate()
    _, err = proc.communicate(timeout=20)
    assert alive, 'talker died: %s' % err


def test_usage():
    result = run_tool(CYBER_CHANNEL)
    assert result.returncode != 0 and 'Commands:' in result.stdout


def test_channel_list_type_echo_info(talker):
    topic = '/apollo/tool_test/' + talker
    result = run_tool(CYBER_CHANNEL, 'list')
    assert result.returncode == 0, result.stderr
    assert topic in result.stdout.split()

    result = run_tool(CYBER_CHANNEL, 'type', '/apollo/tool-test/' + talker)
    assert 'apollo.cyber.proto.ChatterBenchmark' in result.stdout

    result = run_tool(CYBER_CHANNEL, 'echo', topic, '-n', '2')
    assert result.returncode == 0, result.stderr
    assert result.stdout.count('content: "tool test"') == 2

    result = run_tool(CYBER_CHANNEL, 'info', topic)
    assert result.returncode == 0, result.stderr
    assert 'tool_talker_' + talker in result.stdout
    assert 'apollo.cyber.proto.ChatterBenchmark' in result.stdout
    assert '_cyber_py_' not in result.stdout


def test_channel_hz_bw(talker):
    code, out, err = run_until_sigint(
        3.5, CYBER_CHANNEL, 'hz', '/apollo/tool-test/' + talker)
    assert code == 0, err
    rates = [float(line.split()[-1]) for line in out.splitlines()
             if line.startswith('average rate:')]
    assert rates and 10 < rates[-1] < 30, out
    code, out, err = run_until_sigint(
        3.5, CYBER_CHANNEL, 'bw', '/apollo/tool-test/' + talker)
    assert code == 0, err
    assert 'average:' in out


def test_node_tools(talker):
    result = run_tool(CYBER_NODE, 'list')
    assert result.returncode == 0, result.stderr
    assert 'tool_talker_' + talker in result.stdout.split()
    result = run_tool(CYBER_NODE, 'info', 'tool_talker_' + talker)
    assert '[Writing Channels]:\n/apollo/tool_test/' + talker in \
        result.stdout
    assert '/tool_srv_' + talker in result.stdout


def test_service_tools(talker):
    result = run_tool(CYBER_SERVICE, 'list')
    assert result.returncode == 0, result.stderr
    assert '/tool_srv_' + talker in result.stdout.split()
    result = run_tool(CYBER_SERVICE, 'info', 'tool_srv_' + talker)
    assert 'nodename\t tool_talker_' + talker in result.stdout


def _record_content(path):
    from cyber.python.cyber_py3 import record
    reader = record.RecordReader(path)
    channels = {c: (reader.get_messagetype(c), reader.get_messagenumber(c),
                    reader.get_protodesc(c))
                for c in reader.get_channellist()}
    messages = sorted((m.timestamp, m.topic, m.message)
                      for m in reader.read_messages())
    return channels, messages


@pytest.mark.parametrize('storage', ['mcap', 'sqlite3'])
def test_record_rosbag2_round_trip(tmp_path, storage):
    import rosbag2_py
    if storage not in rosbag2_py.get_registered_writers():
        pytest.skip('no %s storage plugin' % storage)
    bag = str(tmp_path / 'bag')
    result = run_tool(TO_BAG, GOLDEN, '-o', bag, '-s', storage)
    assert result.returncode == 0, result.stderr

    reader = rosbag2_py.SequentialReader()
    reader.open(rosbag2_py.StorageOptions(uri=bag, storage_id=storage),
                rosbag2_py.ConverterOptions('cdr', 'cdr'))
    topics = {t.name: t.type for t in reader.get_all_topics_and_types()}
    assert topics == {'/apollo/a': 'apollo_msgs/msg/ProtoMessage',
                      '/apollo/late': 'apollo_msgs/msg/ProtoMessage',
                      '/channel/chatter': 'apollo_msgs/msg/ProtoMessage'}
    from rclpy.serialization import deserialize_message
    from apollo_msgs.msg import ProtoMessage
    topic, data, stamp = reader.read_next()
    msg = deserialize_message(data, ProtoMessage)
    assert (topic, stamp, msg.stamp_ns, msg.type, msg.data.tobytes()) == \
        ('/apollo/late', 5, 5, 'apollo.test.Late', b'\xde\xad\xbe\xef')
    del reader

    out = str(tmp_path / 'back.record')
    result = run_tool(TO_RECORD, bag, '-o', out)
    assert result.returncode == 0, result.stderr
    # channel names (incl. the relative 'channel/chatter'), types,
    # descriptors and messages survive; rosbag2 orders by timestamp.
    assert _record_content(out) == _record_content(GOLDEN)


def test_record_to_rosbag2_channel_filter(tmp_path):
    bag = str(tmp_path / 'bag')
    result = run_tool(TO_BAG, GOLDEN, '-o', bag, '-c', 'channel/chatter')
    assert result.returncode == 0, result.stderr
    out = str(tmp_path / 'back.record')
    assert run_tool(TO_RECORD, bag, '-o', out).returncode == 0
    channels, messages = _record_content(out)
    assert list(channels) == ['channel/chatter']
    assert len(messages) == 67
