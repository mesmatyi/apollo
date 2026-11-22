#!/usr/bin/env python3
# ****************************************************************************
# Copyright 2019 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ****************************************************************************
"""Readers, writers, services, clients and graph queries on ROS 2."""

import threading
import time

import pytest

from conftest import requires_cyber_protos
from conftest import requires_ros
from conftest import spawn_python
from conftest import wait_until

pytestmark = [requires_ros, requires_cyber_protos]


def _simple():
    from cyber.proto.simple_pb2 import SimpleMessage
    return SimpleMessage


def _chatter():
    from cyber.proto.unit_test_pb2 import ChatterBenchmark
    return ChatterBenchmark


def test_read_write(cyber_env, unique):
    """The original cyber_py3 test, plus delivery checks."""
    cyber = cyber_env
    SimpleMessage = _simple()
    channel = 'channel/chatter_' + unique
    received = []
    reader_node = cyber.Node("listener")
    reader = reader_node.create_reader(channel, SimpleMessage,
                                       received.append)
    assert reader.name == channel
    assert reader.data_type == SimpleMessage
    assert reader.topic == '/' + channel
    assert SimpleMessage.DESCRIPTOR.full_name == \
        "apollo.cyber.proto.SimpleMessage"
    # one reader per channel and node
    assert reader_node.create_reader(channel, SimpleMessage, print) is None

    writer_node = cyber.Node("writer")
    writer = writer_node.create_writer(channel, SimpleMessage, 7)
    assert writer.name == channel
    assert writer.data_type == "apollo.cyber.proto.SimpleMessage"
    assert wait_until(lambda: writer.get_subscription_count() == 1)

    msg = SimpleMessage()
    msg.text = "talker:send Alex!"
    msg.integer = 0
    assert writer.write(msg)
    assert wait_until(lambda: len(received) == 1)
    assert isinstance(received[0], SimpleMessage)
    assert received[0] == msg


def test_callback_args_rawdata_and_threads(cyber_env, unique):
    cyber = cyber_env
    SimpleMessage = _simple()
    channel = '/apollo/test/raw-' + unique
    with_args, raw = [], []
    node = cyber.Node("reader_node")
    node.create_reader(channel, SimpleMessage,
                       lambda m, a: with_args.append(
                           (m.integer, a, threading.current_thread().name)),
                       args={'k': 1})
    other = cyber.Node("raw_node")  # noqa: F841
    other.create_rawdata_reader(channel, raw.append)
    # keep the Node referenced: like the old wrapper, a garbage-collected
    # Node destroys its readers/writers
    writer_node = cyber.Node("w")
    writer = writer_node.create_writer(channel, SimpleMessage)
    assert writer.topic == '/apollo/test/raw_' + unique
    assert wait_until(lambda: writer.get_subscription_count() == 2)
    for i in range(5):
        assert writer.write(SimpleMessage(integer=i))
        time.sleep(0.02)
    assert writer.write(SimpleMessage(integer=5).SerializeToString())
    assert wait_until(lambda: len(with_args) == 6 and len(raw) == 6)
    assert [i for i, _, _ in with_args] == list(range(6))
    assert all(a == {'k': 1} for _, a, _ in with_args)
    # user callbacks run on the reader's own thread
    assert all(t.startswith('cyber_reader:') for _, _, t in with_args)
    assert SimpleMessage.FromString(raw[3]).integer == 3


def test_slow_reader_keeps_latest(cyber_env, unique):
    """pending_queue_size (qos_depth) 1: a slow callback skips to the most
    recent sample instead of queueing everything."""
    cyber = cyber_env
    SimpleMessage = _simple()
    channel = 'slow_' + unique
    seen = []

    def slow(msg):
        seen.append(msg.integer)
        time.sleep(0.3)

    reader_node = cyber.Node("slow")
    reader_node.create_reader(channel, SimpleMessage, slow)
    writer_node = cyber.Node("fast")
    writer = writer_node.create_writer(channel, SimpleMessage, 10)
    assert wait_until(lambda: writer.get_subscription_count() == 1)
    for i in range(10):
        writer.write(SimpleMessage(integer=i))
        time.sleep(0.01)
    time.sleep(1.0)
    assert seen[0] == 0 and seen[-1] == 9 and len(seen) < 10


def test_callback_exception_does_not_stop_delivery(cyber_env, unique):
    cyber = cyber_env
    SimpleMessage = _simple()
    channel = 'boom_' + unique
    seen = []

    def callback(msg):
        seen.append(msg.integer)
        raise ValueError('boom')

    reader_node = cyber.Node("boom")
    reader_node.create_reader(channel, SimpleMessage, callback, qos_depth=10)
    writer_node = cyber.Node("w")
    writer = writer_node.create_writer(channel, SimpleMessage, 10)
    assert wait_until(lambda: writer.get_subscription_count() == 1)
    for i in range(3):
        writer.write(SimpleMessage(integer=i))
    assert wait_until(lambda: seen == [0, 1, 2])


def test_service_round_trip(cyber_env, unique):
    cyber = cyber_env
    Chatter = _chatter()
    name = 'server_' + unique

    def callback(data, args):
        return Chatter(content="svr: " + args, seq=data.seq + 2)

    node = cyber.Node("service_node")
    service = node.create_service(name, Chatter, Chatter, callback, "ok")
    assert service is not None
    assert node.create_service(name, Chatter, Chatter, callback) is None
    client_node = cyber.Node("client_node")
    client = client_node.create_client(name, Chatter, Chatter)
    response = client.send_request(Chatter(content="clt", seq=40))
    assert isinstance(response, Chatter)
    assert response.seq == 42 and response.content == "svr: ok"


def test_client_without_service_times_out(cyber_env, unique):
    cyber = cyber_env
    Chatter = _chatter()
    node = cyber.Node("lonely")
    client = node.create_client('nobody_' + unique, Chatter, Chatter)
    start = time.monotonic()
    assert client.send_request(Chatter(), timeout=0.5) is None
    assert time.monotonic() - start < 2.0


def test_nested_requests_do_not_deadlock(cyber_env, unique):
    """A service callback and a reader callback may call other services."""
    cyber = cyber_env
    Chatter = _chatter()
    node = cyber.Node("nested")
    node.create_service('inner_' + unique, Chatter, Chatter,
                        lambda r: Chatter(seq=r.seq * 10))
    inner = node.create_client('inner_' + unique, Chatter, Chatter)
    node.create_service(
        'outer_' + unique, Chatter, Chatter,
        lambda r: Chatter(seq=inner.send_request(Chatter(seq=r.seq)).seq + 1))
    outer = node.create_client('outer_' + unique, Chatter, Chatter)
    assert outer.send_request(Chatter(seq=4)).seq == 41

    results = []
    node.create_reader('trigger_' + unique, Chatter,
                       lambda m: results.append(outer.send_request(m).seq))
    writer = node.create_writer('trigger_' + unique, Chatter)
    assert wait_until(lambda: writer.get_subscription_count() == 1)
    writer.write(Chatter(seq=7))
    assert wait_until(lambda: results == [71])


def test_graph_queries(cyber_env, unique):
    cyber = cyber_env
    Chatter = _chatter()
    channel = '/apollo/graph/test-' + unique
    topic = '/apollo/graph/test_' + unique
    talker = cyber.Node("graph_talker_" + unique)
    listener = cyber.Node("graph-listener-" + unique)
    writer = talker.create_writer(channel, Chatter)
    listener.create_reader(channel, Chatter, lambda m: None)
    talker.create_service('graph_srv_' + unique, Chatter, Chatter,
                          lambda r: r)
    listener.create_client('graph_client_only_' + unique, Chatter, Chatter)
    assert wait_until(lambda: writer.get_subscription_count() == 1)

    assert topic in cyber.ChannelUtils.get_channels(0)
    nodes = cyber.NodeUtils.get_nodes(0)
    assert "graph_talker_" + unique in nodes
    assert "graph_listener_" + unique in nodes
    assert not any(n.startswith('_cyber_py_') for n in nodes)
    assert cyber.NodeUtils.get_writersofnode("graph_talker_" + unique, 0) \
        == [topic]
    # Cyber names are accepted too
    assert cyber.NodeUtils.get_readersofnode("graph-listener-" + unique, 0) \
        == [topic]
    assert cyber.NodeUtils.get_servicesofnode("graph_talker_" + unique, 0) \
        == ['/graph_srv_' + unique]
    assert cyber.NodeUtils.get_node_attr("no_such_node", 0) == b''

    from cyber.proto.role_attributes_pb2 import RoleAttributes
    attr = RoleAttributes.FromString(
        cyber.NodeUtils.get_node_attr("graph_talker_" + unique, 0))
    assert attr.node_name == "graph_talker_" + unique and attr.process_id > 0

    services = cyber.ServiceUtils.get_services(0)
    assert '/graph_srv_' + unique in services
    assert '/graph_client_only_' + unique not in services  # servers only
    attr = RoleAttributes.FromString(
        cyber.ServiceUtils.get_service_attr('graph_srv_' + unique, 0))
    assert attr.service_name == '/graph_srv_' + unique
    assert attr.node_name == "graph_talker_" + unique
    assert cyber.ServiceUtils.get_service_attr('nope_' + unique, 0) == b''

    info = cyber.ChannelUtils.get_channels_info(0)
    roles = [RoleAttributes.FromString(r) for r in info[topic]]
    assert [r.node_name for r in roles] == ["graph_talker_" + unique,
                                            "graph_listener_" + unique]
    assert roles[0].qos_profile.reliability == 1  # RELIABILITY_RELIABLE

    # the protobuf type is learnt from a sample
    def publish():
        while not done.is_set():
            writer.write(Chatter(seq=1))
            time.sleep(0.05)
    done = threading.Event()
    thread = threading.Thread(target=publish)
    thread.start()
    try:
        assert cyber.ChannelUtils.get_msgtype(channel, 2) == \
            b'apollo.cyber.proto.ChatterBenchmark'
    finally:
        done.set()
        thread.join()
    assert cyber.ChannelUtils.get_msgtype('/no/such/channel', 0) == b''
    text = cyber.ChannelUtils.get_debugstring_rawmsgdata(
        'apollo.cyber.proto.ChatterBenchmark',
        Chatter(seq=5, content='x').SerializeToString())
    assert text == b'seq: 5\ncontent: "x"\n'


def test_node_shutdown_removes_endpoints(cyber_env, unique):
    cyber = cyber_env
    Chatter = _chatter()
    node = cyber.Node("short_lived_" + unique)
    node.create_writer('short_' + unique, Chatter)
    assert wait_until(lambda: "short_lived_" + unique in
                      cyber.NodeUtils.get_nodes(0))
    node.shutdown()
    assert wait_until(lambda: "short_lived_" + unique not in
                      cyber.NodeUtils.get_nodes(0))


TALKER = '''
import sys, time
from cyber.python.cyber_py3 import cyber
from cyber.proto.unit_test_pb2 import ChatterBenchmark
cyber.init("talker_sample")
node = cyber.Node("talker_" + sys.argv[1])
writer = node.create_writer("channel/chatter_" + sys.argv[1],
                            ChatterBenchmark, 6)
node.create_service("server_" + sys.argv[1], ChatterBenchmark,
                    ChatterBenchmark,
                    lambda r: ChatterBenchmark(content="svr", seq=r.seq + 2))
seq = 0
while not cyber.is_shutdown():
    seq += 1
    writer.write(ChatterBenchmark(content="py:talker", seq=seq))
    time.sleep(0.05)
cyber.shutdown()
'''


@pytest.fixture
def talker_process(unique):
    proc = spawn_python(TALKER, unique)
    yield proc
    proc.terminate()
    proc.communicate(timeout=20)


def test_talker_listener_across_processes(cyber_env, unique,
                                          talker_process):
    cyber = cyber_env
    Chatter = _chatter()
    received = []
    node = cyber.Node("listener_" + unique)
    node.create_reader("channel/chatter_" + unique, Chatter, received.append)
    assert wait_until(lambda: len(received) >= 5, timeout=20), \
        talker_process.stderr.read() if talker_process.poll() else ''
    seqs = [m.seq for m in received[:5]]
    assert seqs == list(range(seqs[0], seqs[0] + 5))
    assert received[0].content == "py:talker"

    client = node.create_client("server_" + unique, Chatter, Chatter)
    response = client.send_request(Chatter(seq=40))
    assert response is not None and response.seq == 42
    assert "talker_" + unique in cyber.NodeUtils.get_nodes(0)


def test_nodes_are_ordinary_ros_nodes(unique):
    """Parameter services on every node, and the ROS 2 arguments of the
    command line: remapping, parameters, the name of the process."""
    from conftest import run_python
    code = r'''
import sys, time
from cyber.python.cyber_py3 import cyber
from cyber.proto.simple_pb2 import SimpleMessage
cyber.init('ros_args')
node = cyber.Node('ros_args_node')
writer = node.create_writer('/test/ros_args/out', SimpleMessage)
ros_node = node.get_ros_node()
time.sleep(0.5)
services = dict(ros_node.get_service_names_and_types())
print(sorted(n for n in dict(ros_node.get_topic_names_and_types())
             if 'ros_args' in n or 'renamed' in n))
print('/ros_args_node/list_parameters' in services,
      '/ros_args_node/set_parameters' in services)
print(ros_node.get_parameter('answer').value,
      ros_node.get_parameter('use_sim_time').value)
print(cyber.graph_node().get_name(), ros_node.get_name())
cyber.shutdown()
'''
    result = run_python(
        code, '--ros-args', '-r', '/test/ros_args/out:=/test/renamed/out',
        '-r', '__node:=py_process_' + unique, '-p', 'answer:=42')
    assert result.returncode == 0, result.stderr
    lines = result.stdout.strip().splitlines()
    assert lines[0] == "['/test/renamed/out']"
    assert lines[1] == 'True True'
    assert lines[2] == '42 False'
    assert lines[3] == 'py_process_%s ros_args_node' % unique
