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
"""Native ROS 2 types in the Python API.

* conversion: every generated type round-trips protobuf -> ROS 2 message ->
  CDR -> ROS 2 message -> protobuf, and the presence rules match the C++
  runtime (cyber/ros2/test/native_types_test.cc);
* transport: cyber_py3 readers, writers, services and clients talk to plain
  rclpy nodes (which know nothing about Apollo) and to the C++ Cyber
  executables.
"""

import math
import os
import random
import struct
import subprocess
import threading
import time

import pytest

from conftest import requires_cyber_protos
from conftest import requires_ros
from conftest import wait_until

pytestmark = [requires_ros, requires_cyber_protos]

CHATTER_ROS_TYPE = 'apollo_msgs/msg/CyberExamplesProtoChatter'
DRIVER_SRV_TYPE = 'apollo_msgs/srv/CyberExamplesProtoDriver'
PROTO_MESSAGE_TYPE = 'apollo_msgs/msg/ProtoMessage'


def _native():
    from cyber.python.cyber_py3 import native_types
    return native_types


def _have_native_types():
    try:
        return len(_native().registry()) > 0
    except Exception:  # pylint: disable=broad-except
        return False


def _examples():
    try:
        from cyber.examples.proto import examples_pb2
        return examples_pb2
    except ImportError:
        return None


requires_native = pytest.mark.skipif(
    not _have_native_types(),
    reason='apollo_msgs was built without native types')
requires_examples = pytest.mark.skipif(
    _examples() is None, reason='cyber.examples.proto is not installed')


def _cyber_binary(name):
    """Path of an installed C++ Cyber executable, or None."""
    try:
        import ament_index_python
        prefix = ament_index_python.get_package_prefix('cyber')
    except Exception:  # pylint: disable=broad-except
        return None
    path = os.path.join(prefix, 'lib', 'cyber', name)
    return path if os.access(path, os.X_OK) else None


# ---------------------------------------------------------------------------
# Random messages
# ---------------------------------------------------------------------------

def _is_map(field):
    return (field.message_type is not None and
            field.message_type.GetOptions().map_entry)


def _random_scalar(field, rng, use_default):
    from google.protobuf.descriptor import FieldDescriptor as FD
    if use_default:
        return field.default_value
    ranges = {FD.CPPTYPE_INT32: (-2**31, 2**31 - 1),
              FD.CPPTYPE_INT64: (-2**63, 2**63 - 1),
              FD.CPPTYPE_UINT32: (0, 2**32 - 1),
              FD.CPPTYPE_UINT64: (0, 2**64 - 1)}
    kind = field.cpp_type
    if kind in ranges:
        low, high = ranges[kind]
        return rng.choice([0, low, high,
                           rng.randint(max(low, -2**31), min(high, 2**31 - 1))])
    if kind == FD.CPPTYPE_DOUBLE:
        return rng.choice([0.0, float('inf'), float('nan'),
                           -1.7976931348623157e308, rng.uniform(-1e6, 1e6)])
    if kind == FD.CPPTYPE_FLOAT:
        value = rng.choice([0.0, float('inf'), float('nan'),
                            rng.uniform(-1e6, 1e6)])
        return struct.unpack('f', struct.pack('f', value))[0]
    if kind == FD.CPPTYPE_BOOL:
        return rng.random() < 0.5
    if kind == FD.CPPTYPE_ENUM:
        return rng.choice(field.enum_type.values).number
    if field.type == FD.TYPE_BYTES:
        return bytes(rng.randint(0, 255) for _ in range(rng.randint(0, 12)))
    return rng.choice(['', 'apollo', 'lane_1_-1', 'a b\tc', '\u8def\u5f84', 'x'])


def fill_randomly(message, rng, depth=0, budget=None):
    """Random content: random presence of optional fields, explicit
    defaults, extreme values, at most one member per oneof."""
    from google.protobuf.descriptor import FieldDescriptor as FD
    budget = [300] if budget is None else budget
    descriptor = message.DESCRIPTOR
    budget[0] -= 1
    leaf = depth >= 5 or budget[0] <= 0
    skipped = set()
    for oneof in descriptor.oneofs:
        if len(oneof.fields) == 1 and oneof.name.startswith('_'):
            continue                      # proto3 optional
        keep = rng.randint(-1, len(oneof.fields) - 1)
        for index, field in enumerate(oneof.fields):
            if index != keep:
                skipped.add(field.name)
    for field in descriptor.fields:
        if field.name in skipped:
            continue
        is_message = field.message_type is not None
        if _is_map(field):
            count = 0 if leaf else rng.randint(0, 3)
            key_field = field.message_type.fields_by_name['key']
            value_field = field.message_type.fields_by_name['value']
            if key_field.cpp_type == FD.CPPTYPE_BOOL:
                count = min(count, 2)
            container = getattr(message, field.name)
            for k in range(count):
                if key_field.cpp_type == FD.CPPTYPE_STRING:
                    key = 'key%d' % k
                elif key_field.cpp_type == FD.CPPTYPE_BOOL:
                    key = k == 0
                else:
                    key = k
                if value_field.message_type is not None:
                    container[key].SetInParent()
                    fill_randomly(container[key], rng, depth + 1, budget)
                else:
                    container[key] = _random_scalar(value_field, rng, False)
        elif field.label == FD.LABEL_REPEATED:
            count = 0 if (leaf and is_message) else rng.randint(0, 3)
            for _ in range(count):
                if is_message:
                    fill_randomly(getattr(message, field.name).add(), rng,
                                  depth + 1, budget)
                else:
                    getattr(message, field.name).append(
                        _random_scalar(field, rng, False))
        else:
            if field.label != FD.LABEL_REQUIRED and (
                    rng.random() < 0.4 or (leaf and is_message)):
                continue
            if is_message:
                sub = getattr(message, field.name)
                sub.SetInParent()
                fill_randomly(sub, rng, depth + 1, budget)
            else:
                setattr(message, field.name,
                        _random_scalar(field, rng, rng.random() < 0.2))


def _wire(message):
    """Canonical bytes: equal exactly when the messages are equal including
    field presence (NaN payloads and map order included)."""
    return message.SerializePartialToString(deterministic=True)


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

@requires_native
def test_registry_is_installed():
    native_types = _native()
    reg = native_types.registry()
    assert reg.enabled
    assert len(reg) > 700
    info = reg.find_by_proto_name('apollo.cyber.examples.proto.Chatter')
    assert info is not None and info.ros_type == CHATTER_ROS_TYPE
    assert reg.find_by_ros_type(CHATTER_ROS_TYPE) is info
    assert reg.find_by_proto_name('apollo.cyber.proto.SimpleMessage') is None
    service = reg.find_service('apollo.cyber.examples.proto.Driver',
                               'apollo.cyber.examples.proto.Driver')
    assert service is not None and service.ros_type == DRIVER_SRV_TYPE
    # Types nobody imported come from the installed descriptor set.
    assert reg.find_descriptor('apollo.planning.ADCTrajectory') is not None
    assert reg.message_class('apollo.planning.ADCTrajectory') is not None


@requires_native
def test_structure_hash_matches_the_registry():
    """The hash decides whether a process may use a native type; Python must
    compute what proto2ros.py wrote (and the C++ runtime computes)."""
    native_types = _native()
    reg = native_types.registry()
    for name in reg.proto_names():
        descriptor = reg.find_descriptor(name)
        assert descriptor is not None, name
        assert native_types.structure_hash(descriptor) == \
            reg.find_by_proto_name(name).hash, name


@requires_native
def test_every_type_round_trips():
    """protobuf -> ROS 2 message -> CDR -> ROS 2 message -> protobuf gives
    back the same message, including field presence, for every type."""
    from google.protobuf.descriptor import FieldDescriptor as FD
    from rclpy.serialization import deserialize_message
    from rclpy.serialization import serialize_message
    native_types = _native()
    reg = native_types.registry()
    names = reg.proto_names()
    assert len(names) > 700
    failures = []
    messages = 0
    for index, name in enumerate(names):
        descriptor = reg.find_descriptor(name)
        converter = native_types.get_converter(descriptor)
        if converter is None:
            failures.append(name + ': no converter')
            continue
        assert converter.ros_type == reg.find_by_proto_name(name).ros_type
        required = any(f.label == FD.LABEL_REQUIRED for f in descriptor.fields)
        for seed in range(1 if required else 0, 6):   # seed 0: empty message
            original = converter.message_class()
            if seed:
                fill_randomly(original, random.Random(index * 1000 + seed))
            try:
                ros = converter.to_ros(original)
                received = deserialize_message(serialize_message(ros),
                                               converter.ros_class)
                copy = converter.from_ros(received)
            except Exception as e:  # pylint: disable=broad-except
                failures.append('%s (seed %d): %r' % (name, seed, e))
                break
            if _wire(original) != _wire(copy):
                failures.append('%s (seed %d):\n%s---\n%s' % (
                    name, seed, str(original)[:400], str(copy)[:400]))
                break
            messages += 1
    assert not failures, '%d of %d types failed:\n%s' % (
        len(failures), len(names), '\n'.join(failures[:20]))
    print('%d types, %d messages round-tripped' % (len(names), messages))


@requires_native
def test_every_service_has_converters():
    native_types = _native()
    reg = native_types.registry()
    services = reg.services()
    assert len(services) >= 14
    for service in services:
        converters = native_types.get_service_converters(
            reg.find_descriptor(service.request),
            reg.find_descriptor(service.response))
        assert converters is not None, service.ros_type
        service_class, request, response = converters
        original = request.message_class()
        fill_randomly(original, random.Random(7))
        ros = request.to_ros(original, service_class.Request())
        assert _wire(request.from_ros(ros)) == _wire(original), \
            service.ros_type
        assert response.ros_class is service_class.Response


@requires_native
@requires_examples
def test_presence_semantics():
    """What a plain ROS 2 node sees and what it can send."""
    from apollo_msgs.msg import CyberExamplesProtoChatter as RosChatter
    native_types = _native()
    Chatter = _examples().Chatter
    converter = native_types.get_converter(Chatter.DESCRIPTOR)
    assert converter is not None

    # Apollo -> ROS 2: values plus one presence bit per optional field.
    chatter = Chatter(timestamp=0, seq=7, content=b'\x00\x01\xff')
    ros = converter.to_ros(chatter)
    assert isinstance(ros, RosChatter)
    assert (ros.timestamp, ros.seq) == (0, 7)
    assert bytes(ros.content) == b'\x00\x01\xff'
    assert ros.proto_has_bits == (RosChatter.HAS_TIMESTAMP |
                                  RosChatter.HAS_SEQ | RosChatter.HAS_CONTENT)

    # ROS 2 -> Apollo from a publisher that never heard of presence bits: a
    # field is present when it differs from its default.
    plain = RosChatter(seq=9, content=[1, 2, 3])
    from_plain = converter.from_ros(plain)
    assert from_plain.HasField('seq') and from_plain.seq == 9
    assert from_plain.content == b'\x01\x02\x03'
    assert not from_plain.HasField('timestamp')
    assert not from_plain.HasField('lidar_timestamp')

    # ... and a publisher that sets a bit can send an explicit default.
    plain.proto_has_bits = RosChatter.HAS_LIDAR_TIMESTAMP
    with_bit = converter.from_ros(plain)
    assert with_bit.HasField('lidar_timestamp')
    assert with_bit.lidar_timestamp == 0


@requires_native
def test_untouched_ros_message_is_empty():
    """A newly constructed ROS 2 message converts to the empty protobuf
    message for every type whose defaults a .msg file can express."""
    from google.protobuf.descriptor import FieldDescriptor as FD
    native_types = _native()
    reg = native_types.registry()
    matching = 0
    names = reg.proto_names()
    for name in names:
        descriptor = reg.find_descriptor(name)
        converter = native_types.get_converter(descriptor)
        assert converter is not None, name
        if not converter.defaults_match:
            continue
        matching += 1
        if descriptor.GetOptions().map_entry or any(
                f.label == FD.LABEL_REQUIRED for f in descriptor.fields):
            continue
        message = converter.from_ros(converter.ros_class())
        assert message.ByteSize() == 0, '%s:\n%s' % (name, message)
    # The same count as the C++ test prints.
    assert matching > len(names) // 2
    print('%d of %d types have ROS 2 defaults equal to their protobuf '
          'defaults' % (matching, len(names)))


@requires_native
def test_nan_defaults():
    """Apollo's geometry types default to NaN, which a .msg file cannot
    say: a new ROS 2 message holds 0 there. A zero is a value (a standing
    obstacle has velocity 0, not NaN); NaN says "absent"."""
    from apollo_msgs.msg import PerceptionPerceptionObstacle as RosObstacle
    native_types = _native()
    reg = native_types.registry()
    converter = native_types.get_converter(
        reg.find_descriptor('apollo.perception.PerceptionObstacle'))
    assert converter is not None
    assert not converter.defaults_match

    # Apollo -> ROS 2: an absent velocity reads NaN, as it does in Apollo.
    shown = converter.to_ros(converter.message_class())
    assert math.isnan(shown.velocity.x)
    assert not shown.proto_has_bits & RosObstacle.HAS_VELOCITY
    assert converter.from_ros(shown).ByteSize() == 0

    # ROS 2 -> Apollo: a plain publisher's zero velocity is a zero velocity.
    plain = RosObstacle(id=7)
    assert plain.velocity.x == 0.0
    from_plain = converter.from_ros(plain)
    assert from_plain.HasField('velocity')
    assert from_plain.velocity.x == 0.0

    # ... and NaN says "no velocity".
    plain.velocity.x = plain.velocity.y = plain.velocity.z = float('nan')
    assert not converter.from_ros(plain).HasField('velocity')


@requires_native
def test_oneof_from_plain_ros():
    """A oneof is only selected by a member the publisher set a bit for or
    changed, never by a member that merely holds zeros where protobuf
    defaults to NaN."""
    from apollo_msgs.msg import PlanningMainDecision as RosMainDecision
    native_types = _native()
    reg = native_types.registry()
    converter = native_types.get_converter(
        reg.find_descriptor('apollo.planning.MainDecision'))
    assert converter is not None

    none = converter.from_ros(RosMainDecision())
    assert none.WhichOneof('task') is None
    assert none.ByteSize() == 0

    stop = RosMainDecision()
    stop.stop.reason = 'pedestrian'
    assert converter.from_ros(stop).WhichOneof('task') == 'stop'

    # An empty member can be selected with its presence bit.
    cruise = RosMainDecision(proto_has_bits=RosMainDecision.HAS_CRUISE)
    assert converter.from_ros(cruise).WhichOneof('task') == 'cruise'


@requires_native
def test_conversion_cost():
    """Cost of converting a typical planning message (information only)."""
    native_types = _native()
    reg = native_types.registry()
    converter = native_types.get_converter(
        reg.find_descriptor('apollo.planning.ADCTrajectory'))
    trajectory = converter.message_class()
    trajectory.header.timestamp_sec = 1.5
    trajectory.header.module_name = 'planning'
    for i in range(100):
        point = trajectory.trajectory_point.add()
        point.path_point.x = 100.0 + i
        point.path_point.y = 200.0 - 0.1 * i
        point.path_point.theta = 0.01 * i
        point.path_point.kappa = 0.001
        point.path_point.s = float(i)
        point.v = 5.0
        point.a = 0.1
        point.relative_time = 0.1 * i
    rounds = 20
    t0 = time.perf_counter()
    for _ in range(rounds):
        ros = converter.to_ros(trajectory)
    t1 = time.perf_counter()
    for _ in range(rounds):
        copy = converter.from_ros(ros)
    t2 = time.perf_counter()
    assert _wire(copy) == _wire(trajectory)
    print('ADCTrajectory with 100 points: to ROS 2 %.2f ms, from ROS 2 '
          '%.2f ms' % ((t1 - t0) / rounds * 1e3, (t2 - t1) / rounds * 1e3))


# ---------------------------------------------------------------------------
# Transport: cyber_py3 <-> plain rclpy
# ---------------------------------------------------------------------------

class PlainRosPeer(object):
    """A plain rclpy node spun on its own thread. It knows nothing about
    Apollo except the generated message types."""

    def __init__(self, name):
        import rclpy
        from rclpy.executors import SingleThreadedExecutor
        self._context = rclpy.Context()
        rclpy.init(context=self._context)
        self.node = rclpy.create_node(name, context=self._context)
        self._executor = SingleThreadedExecutor(context=self._context)
        self._executor.add_node(self.node)
        self._stop = False
        self._thread = threading.Thread(target=self._spin, daemon=True)
        self._thread.start()

    def _spin(self):
        while not self._stop:
            self._executor.spin_once(timeout_sec=0.05)

    def topic_types(self, topic):
        return dict(self.node.get_topic_names_and_types()).get(topic, [])

    def service_types(self, service):
        return dict(self.node.get_service_names_and_types()).get(service, [])

    def close(self):
        import rclpy
        self._stop = True
        self._thread.join()
        self._executor.shutdown()
        self.node.destroy_node()
        rclpy.shutdown(context=self._context)


@pytest.fixture
def peer(unique):
    p = PlainRosPeer('plain_peer_' + unique)
    yield p
    p.close()


@requires_native
@requires_examples
def test_writer_publishes_native_type(cyber_env, peer, unique):
    from apollo_msgs.msg import CyberExamplesProtoChatter as RosChatter
    Chatter = _examples().Chatter
    topic = '/test/py_native/out_' + unique
    received = []
    peer.node.create_subscription(RosChatter, topic, received.append, 10)

    node = cyber_env.Node('py_native_talker')
    writer = node.create_writer(topic, Chatter)
    assert wait_until(lambda: writer.get_subscription_count() == 1)
    assert peer.topic_types(topic) == [CHATTER_ROS_TYPE]

    assert writer.write(Chatter(timestamp=123456789, seq=1, content=b'payload'))
    assert wait_until(lambda: len(received) == 1)
    got = received[0]
    assert (got.timestamp, got.seq, got.lidar_timestamp) == (123456789, 1, 0)
    assert bytes(got.content) == b'payload'
    assert got.proto_has_bits == (RosChatter.HAS_TIMESTAMP |
                                  RosChatter.HAS_SEQ | RosChatter.HAS_CONTENT)


@requires_native
@requires_examples
def test_reader_receives_from_plain_publisher(cyber_env, peer, unique):
    from apollo_msgs.msg import CyberExamplesProtoChatter as RosChatter
    Chatter = _examples().Chatter
    topic = '/test/py_native/in_' + unique
    publisher = peer.node.create_publisher(RosChatter, topic, 10)

    node = cyber_env.Node('py_native_listener')
    received = []
    reader = node.create_reader(topic, Chatter, received.append)
    assert wait_until(lambda: publisher.get_subscription_count() == 1)
    assert reader.get_publisher_count() == 1

    publisher.publish(RosChatter(seq=5, content=[114, 111, 115]))
    assert wait_until(lambda: len(received) == 1)
    assert isinstance(received[0], Chatter)
    assert received[0].seq == 5 and received[0].content == b'ros'
    assert not received[0].HasField('timestamp')


@requires_native
@requires_examples
def test_raw_reader_follows_the_graph(cyber_env, peer, unique):
    """A raw reader takes the type of a channel from the ROS 2 graph, also
    when the writer appears after the reader, and gets protobuf bytes."""
    from apollo_msgs.msg import CyberExamplesProtoChatter as RosChatter
    Chatter = _examples().Chatter
    topic = '/test/py_native/raw_in_' + unique
    node = cyber_env.Node('py_native_raw_listener')
    received = []
    node.create_rawdata_reader(topic, received.append)
    time.sleep(0.3)                       # no writer yet: nothing to follow

    publisher = peer.node.create_publisher(RosChatter, topic, 10)
    assert wait_until(lambda: publisher.get_subscription_count() == 1)
    publisher.publish(RosChatter(seq=77))
    assert wait_until(lambda: len(received) == 1)
    assert isinstance(received[0], bytes)
    assert Chatter.FromString(received[0]).seq == 77
    assert cyber_env.ChannelUtils.get_msgtype(topic, 0) == \
        b'apollo.cyber.examples.proto.Chatter'


@requires_native
@requires_examples
def test_raw_writer_publishes_native_type(cyber_env, peer, unique):
    """A writer that only names its protobuf type (player, bridge) and
    writes wire bytes publishes the native type."""
    from apollo_msgs.msg import CyberExamplesProtoChatter as RosChatter
    Chatter = _examples().Chatter
    topic = '/test/py_native/raw_out_' + unique
    received = []
    peer.node.create_subscription(RosChatter, topic, received.append, 10)
    node = cyber_env.Node('py_native_raw_talker')
    writer = node.create_writer(topic, 'apollo.cyber.examples.proto.Chatter')
    assert wait_until(lambda: writer.get_subscription_count() == 1)
    assert peer.topic_types(topic) == [CHATTER_ROS_TYPE]
    assert writer.write(Chatter(seq=31).SerializeToString())
    assert wait_until(lambda: len(received) == 1)
    assert received[0].seq == 31


@requires_native
@requires_examples
def test_service_answers_plain_client(cyber_env, peer, unique):
    from apollo_msgs.srv import CyberExamplesProtoDriver as RosDriver
    Driver = _examples().Driver
    name = '/test/py_native/driver_' + unique

    def serve(request):
        return Driver(msg_id=request.msg_id + 1,
                      content='re: ' + request.content)

    node = cyber_env.Node('py_native_server')
    assert node.create_service(name, Driver, Driver, serve) is not None

    client = peer.node.create_client(RosDriver, name)
    assert client.wait_for_service(timeout_sec=10.0)
    assert peer.service_types(name) == [DRIVER_SRV_TYPE]
    # A request sent in the instant a service is discovered can be lost
    # before the server has matched the client (Fast DDS); cyber clients
    # settle for a moment, a plain client has to do the same.
    time.sleep(0.3)
    future = client.call_async(RosDriver.Request(msg_id=41, content='hello'))
    assert wait_until(future.done)
    response = future.result()
    assert (response.msg_id, response.content) == (42, 're: hello')
    assert response.proto_has_bits == (RosDriver.Response.HAS_MSG_ID |
                                       RosDriver.Response.HAS_CONTENT)

    # The Cyber client of the same process uses the same native service.
    cyber_client = node.create_client(name, Driver, Driver)
    answer = cyber_client.send_request(Driver(msg_id=1, content='cyber'))
    assert isinstance(answer, Driver)
    assert (answer.msg_id, answer.content) == (2, 're: cyber')
    assert not answer.HasField('timestamp')


@requires_native
@requires_examples
def test_client_calls_plain_service(cyber_env, peer, unique):
    from apollo_msgs.srv import CyberExamplesProtoDriver as RosDriver
    Driver = _examples().Driver
    name = '/test/py_native/ros_driver_' + unique

    def serve(request, response):
        response.msg_id = request.msg_id * 2
        response.content = request.content + '!'
        return response

    peer.node.create_service(RosDriver, name, serve)
    node = cyber_env.Node('py_native_client')
    client = node.create_client(name, Driver, Driver)
    answer = client.send_request(Driver(msg_id=21, content='ping'))
    assert isinstance(answer, Driver)
    assert (answer.msg_id, answer.content) == (42, 'ping!')


@requires_native
def test_unregistered_types_keep_the_protobuf_wire(cyber_env, peer, unique):
    """Types without a generated interface use apollo_msgs/msg/ProtoMessage."""
    from cyber.proto.simple_pb2 import SimpleMessage
    topic = '/test/py_native/unregistered_' + unique
    node = cyber_env.Node('py_native_fallback')
    writer = node.create_writer(topic, SimpleMessage)
    assert writer is not None
    assert wait_until(lambda: bool(peer.topic_types(topic)))
    assert peer.topic_types(topic) == [PROTO_MESSAGE_TYPE]


@requires_native
@requires_examples
def test_opt_out_environment(unique):
    """APOLLO_ROS_PROTO_CHANNELS and APOLLO_ROS_WIRE_FORMAT keep channels on
    the protobuf wire format (checked in fresh processes: the variables are
    read once)."""
    from conftest import run_python
    code = r'''
import sys, time
from cyber.python.cyber_py3 import cyber
from cyber.examples.proto.examples_pb2 import Chatter
cyber.init('opt_out')
node = cyber.Node('opt_out_node')
node.create_writer('/test/protowire/a_%s', Chatter)
node.create_writer('/test/native/a_%s', Chatter)
time.sleep(0.5)
types = dict(node.get_ros_node().get_topic_names_and_types())
print(types['/test/protowire/a_%s'][0], types['/test/native/a_%s'][0])
cyber.shutdown()
''' % ((unique,) * 4)
    env = dict(os.environ, APOLLO_ROS_PROTO_CHANNELS='^/test/protowire/')
    out = subprocess.run(['python3', '-c', code], env=env, timeout=60,
                         stdout=subprocess.PIPE, universal_newlines=True)
    assert out.stdout.split() == [PROTO_MESSAGE_TYPE, CHATTER_ROS_TYPE], \
        out.stdout
    env = dict(os.environ, APOLLO_ROS_WIRE_FORMAT='proto')
    out = subprocess.run(['python3', '-c', code], env=env, timeout=60,
                         stdout=subprocess.PIPE, universal_newlines=True)
    assert out.stdout.split() == [PROTO_MESSAGE_TYPE, PROTO_MESSAGE_TYPE], \
        out.stdout
    assert run_python('print(1)').returncode == 0


@requires_native
@requires_examples
def test_graph_queries_know_native_channels(cyber_env, peer, unique):
    """Channel topology: native Apollo types are channels, other ROS 2 topics
    are not, and the protobuf type comes from the ROS 2 type."""
    from apollo_msgs.msg import CyberExamplesProtoChatter as RosChatter
    from rcl_interfaces.msg import Log
    apollo_topic = '/test/py_native/topology_' + unique
    other_topic = '/test/py_native/not_apollo_' + unique
    peer.node.create_publisher(RosChatter, apollo_topic, 10)
    peer.node.create_publisher(Log, other_topic, 10)
    channels = cyber_env.ChannelUtils.get_channels(1)
    assert apollo_topic in channels
    assert other_topic not in channels
    assert cyber_env.ChannelUtils.get_msgtype(apollo_topic, 0) == \
        b'apollo.cyber.examples.proto.Chatter'
    writers = cyber_env.ChannelUtils.get_writers_info(apollo_topic)
    assert len(writers) == 1
    assert writers[0].message_type == 'apollo.cyber.examples.proto.Chatter'


# ---------------------------------------------------------------------------
# Transport: cyber_py3 <-> C++ Cyber executables
# ---------------------------------------------------------------------------

def _spawn(binary, tmp_path, *args):
    env = dict(os.environ, GLOG_alsologtostderr='1')
    log = open(os.path.join(str(tmp_path), os.path.basename(binary) + '.log'),
               'w')
    process = subprocess.Popen([binary] + list(args), cwd=str(tmp_path),
                               stdout=log, stderr=subprocess.STDOUT, env=env)
    return process, log


def _stop(process, log):
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
    log.close()


def _log_text(tmp_path, binary):
    with open(os.path.join(str(tmp_path), os.path.basename(binary) + '.log'),
              errors='replace') as f:
        return f.read()


@requires_native
@requires_examples
def test_reader_receives_from_cpp_talker(cyber_env, tmp_path):
    talker = _cyber_binary('cyber_example_talker')
    if talker is None:
        pytest.skip('cyber_example_talker is not installed')
    Chatter = _examples().Chatter
    received = []
    node = cyber_env.Node('py_listener_of_cpp')
    node.create_reader('channel/chatter', Chatter, received.append)
    process, log = _spawn(talker, tmp_path)
    try:
        assert wait_until(lambda: len(received) >= 2, timeout=20)
    finally:
        _stop(process, log)
    assert all(isinstance(m, Chatter) for m in received)
    assert received[0].HasField('seq') and received[0].content
    assert received[1].seq == received[0].seq + 1
    assert cyber_env.ChannelUtils.get_msgtype('channel/chatter', 0) == \
        b'apollo.cyber.examples.proto.Chatter'


@requires_native
@requires_examples
def test_cpp_listener_receives_from_writer(cyber_env, tmp_path):
    listener = _cyber_binary('cyber_example_listener')
    if listener is None:
        pytest.skip('cyber_example_listener is not installed')
    Chatter = _examples().Chatter
    process, log = _spawn(listener, tmp_path)
    try:
        node = cyber_env.Node('py_talker_to_cpp')
        writer = node.create_writer('channel/chatter', Chatter)
        assert wait_until(lambda: writer.get_subscription_count() >= 1,
                          timeout=20)
        writer.write(Chatter(seq=4242, content=b'from python'))
        assert wait_until(
            lambda: 'seq-> 4242' in _log_text(tmp_path, listener), timeout=10)
        assert 'msgcontent->from python' in _log_text(tmp_path, listener)
    finally:
        _stop(process, log)


@requires_native
@requires_examples
def test_client_calls_cpp_service(cyber_env, tmp_path):
    service = _cyber_binary('cyber_example_service')
    if service is None:
        pytest.skip('cyber_example_service is not installed')
    Driver = _examples().Driver
    process, log = _spawn(service, tmp_path)
    try:
        node = cyber_env.Node('py_client_of_cpp')
        client = node.create_client('test_server', Driver, Driver)
        assert client.wait_for_service(20)
        answer = client.send_request(Driver(msg_id=7, content='python'))
        assert isinstance(answer, Driver)
        assert answer.HasField('msg_id')
    finally:
        _stop(process, log)


@requires_native
def test_cpp_recorder_round_trip(cyber_env, tmp_path, unique):
    """A random planning message written by Python is recorded by the C++
    cyber_recorder (native type -> protobuf bytes in the .record file) and
    played back by it (protobuf bytes -> native type) to a Python reader:
    the message survives both conversions in both languages unchanged."""
    recorder = _cyber_binary('cyber_recorder')
    if recorder is None:
        pytest.skip('cyber_recorder is not installed')
    native_types = _native()
    reg = native_types.registry()
    type_name = 'apollo.planning.ADCTrajectory'
    message_class = reg.message_class(type_name)
    original = message_class()
    fill_randomly(original, random.Random(20261122))
    assert original.ByteSize() > 100
    channel = '/test/py_native/recorded_' + unique
    record_file = os.path.join(str(tmp_path), 'native.record')

    node = cyber_env.Node('py_recorded_talker')
    writer = node.create_writer(channel, type_name)
    process, log = _spawn(recorder, tmp_path, 'record', '-c', channel, '-o',
                          record_file)
    try:
        assert wait_until(lambda: writer.get_subscription_count() >= 1,
                          timeout=20)
        for _ in range(3):
            writer.write(original)
            time.sleep(0.2)
        time.sleep(1.0)
    finally:
        _stop(process, log)
    # The recorder appends a sequence number and a time stamp to the name.
    import glob
    files = sorted(glob.glob(record_file + '*'))
    assert len(files) == 1, _log_text(tmp_path, recorder)
    record_file = files[0]

    from cyber.python.cyber_py3 import record
    reader = record.RecordReader(record_file)
    recorded = [m for m in reader.read_messages() if m.topic == channel]
    assert len(recorded) == 3, _log_text(tmp_path, recorder)
    assert reader.get_messagetype(channel) == type_name
    for item in recorded:
        assert _wire(message_class.FromString(item.message)) == \
            _wire(original)

    # Play it back: a typed Python reader gets the same message again.
    played = []
    listener = cyber_env.Node('py_recorded_listener')
    listener.create_reader(channel, type_name, played.append)
    process, log = _spawn(recorder, tmp_path, 'play', '-f', record_file, '-l')
    try:
        assert wait_until(lambda: len(played) >= 1, timeout=30), \
            _log_text(tmp_path, recorder)
    finally:
        _stop(process, log)
    assert _wire(played[0]) == _wire(original)


# ---------------------------------------------------------------------------
# Tools: .record <-> rosbag2 with native types
# ---------------------------------------------------------------------------

@requires_native
@pytest.mark.parametrize('wire', ['native', 'proto'])
def test_record_rosbag2_with_native_types(tmp_path, wire):
    """record_to_rosbag2.py writes natively typed topics (what a live system
    records with `ros2 bag record`), rosbag2_to_record.py reads them back:
    the messages are the same protobuf messages again."""
    import rosbag2_py
    from conftest import TOOLS_DIR
    from cyber.python.cyber_py3 import proto_registry
    from cyber.python.cyber_py3 import record
    native_types = _native()
    reg = native_types.registry()
    types = {'/apollo/planning': 'apollo.planning.ADCTrajectory',
             '/apollo/canbus/chassis': 'apollo.canbus.Chassis',
             '/apollo/test/unregistered': 'apollo.cyber.proto.SimpleMessage'}
    source = str(tmp_path / 'source.record')
    writer = record.RecordWriter()
    assert writer.open(source)
    originals = []
    stamp = 1000
    for channel, type_name in sorted(types.items()):
        message_class = proto_registry.get_message_class(type_name)
        assert message_class is not None, type_name
        writer.write_channel(channel, type_name,
                             proto_registry.get_proto_desc(message_class))
        for seed in range(1, 4):
            message = message_class()
            fill_randomly(message, random.Random(seed))
            stamp += 1
            writer.write_message(channel, message.SerializeToString(), stamp)
            originals.append((stamp, channel, _wire(message)))
    writer.close()

    bag = str(tmp_path / 'bag')
    to_bag = os.path.join(TOOLS_DIR, 'cyber_recorder', 'record_to_rosbag2.py')
    result = subprocess.run(
        ['python3', to_bag, source, '-o', bag, '-s', 'mcap', '--wire', wire],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        universal_newlines=True, timeout=120)
    assert result.returncode == 0, result.stderr

    reader = rosbag2_py.SequentialReader()
    reader.open(rosbag2_py.StorageOptions(uri=bag, storage_id='mcap'),
                rosbag2_py.ConverterOptions('cdr', 'cdr'))
    topics = {t.name: t.type for t in reader.get_all_topics_and_types()}
    del reader
    if wire == 'native':
        assert topics == {
            '/apollo/planning': 'apollo_msgs/msg/PlanningADCTrajectory',
            '/apollo/canbus/chassis': 'apollo_msgs/msg/CanbusChassis',
            '/apollo/test/unregistered': PROTO_MESSAGE_TYPE}
    else:
        assert set(topics.values()) == {PROTO_MESSAGE_TYPE}

    back = str(tmp_path / 'back.record')
    to_record = os.path.join(TOOLS_DIR, 'cyber_recorder',
                             'rosbag2_to_record.py')
    result = subprocess.run(['python3', to_record, bag, '-o', back],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=120)
    assert result.returncode == 0, result.stderr
    restored = record.RecordReader(back)
    assert sorted(restored.get_channellist()) == sorted(types)
    for channel, type_name in types.items():
        assert restored.get_messagetype(channel) == type_name
    got = []
    for item in restored.read_messages():
        message = reg.message_class(types[item.topic])()
        message.MergeFromString(item.message)
        got.append((item.timestamp, item.topic, _wire(message)))
    assert sorted(got) == sorted(originals)
