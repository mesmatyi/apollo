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
# -*- coding: utf-8 -*-
"""Cyber RT Python API, implemented in pure Python on rclpy (ROS 2).

Transport contract (docs/ros2/architecture.md):

* every channel is a ROS 2 topic named ``naming.channel_to_topic(channel)``.
  Its type is the native ROS 2 type generated from the protobuf message
  (``apollo.planning.ADCTrajectory`` -> ``apollo_msgs/msg/PlanningADCTrajectory``,
  see ``native_types``), so plain ROS 2 nodes and tools read and write it.
  Protobuf types without a generated interface, and channels opted out with
  ``APOLLO_ROS_PROTO_CHANNELS``, use ``apollo_msgs/msg/ProtoMessage``
  (``type`` = protobuf full name, ``data`` = protobuf wire bytes);
* every service is a ROS 2 service of the native ``apollo_msgs/srv/...``
  type of its request/response pair, else ``apollo_msgs/srv/ProtoService``;
* a raw reader (no protobuf type given) follows whatever type the writers of
  the channel use and always hands protobuf wire bytes to its callback;
* writers are reliable / volatile / keep-last ``qos_depth``; readers are
  keep-last ``qos_depth`` (Cyber pending_queue_size, default 1) and adapt
  reliability/durability to the publishers already on the graph;
* nodes are named ``naming.to_node_name(name)``.

As with Cyber, callbacks run on background threads and ``Node.spin()`` only
blocks until shutdown. Threading model (mirrors Cyber's PyReader):

* every Node is spun by its own rclpy SingleThreadedExecutor thread (rclpy's
  MultiThreadedExecutor adds tens of ms of jitter and drops samples);
* a subscription callback only enqueues the sample; each reader runs the user
  callback on its own worker thread, keeping at most ``qos_depth`` pending
  samples (oldest dropped, like Cyber's pending queue). So one reader's
  callbacks never overlap while different readers run in parallel;
* each service has its own callback group and runs its callback on a worker
  thread (the executor awaits the result), so one service handles requests
  serially while different services, even of the same node, run in parallel;
* service clients, graph queries and sampling use one hidden helper node with
  its own executor thread (like the C++ runtime's aux node), so
  ``Client.send_request()`` works from any callback without deadlocking.

SIGINT puts the runtime in the shutting-down state (``is_shutdown()`` becomes
True) instead of raising KeyboardInterrupt; a second SIGINT raises it.
"""

import array
import atexit
import collections
import hashlib
import os
import signal
import socket
import sys
import threading
import time
import traceback

import rclpy
import rclpy.context
import rclpy.node
import rclpy.task
from rclpy.callback_groups import MutuallyExclusiveCallbackGroup
from rclpy.executors import SingleThreadedExecutor
from rclpy.qos import DurabilityPolicy
from rclpy.qos import HistoryPolicy
from rclpy.qos import QoSProfile
from rclpy.qos import ReliabilityPolicy

try:
    from rclpy.signals import SignalHandlerOptions
except ImportError:  # pragma: no cover - rclpy < Humble
    SignalHandlerOptions = None

from apollo_msgs.msg import ProtoMessage
from apollo_msgs.srv import ProtoService

from . import _cyber_protos
from . import cyber_timer
from . import native_types
from . import proto_registry
from .naming import channel_to_topic
from .naming import to_node_name

PROTO_MESSAGE_TYPE = 'apollo_msgs/msg/ProtoMessage'
PROTO_SERVICE_TYPE = 'apollo_msgs/srv/ProtoService'
RAWDATATYPE = 'RawData'
# cyber::Client::SendRequest() default timeout
DEFAULT_REQUEST_TIMEOUT_S = 5.0
# Pause after a service was first discovered, before the first request.
SERVICE_DISCOVERY_SETTLE_S = 0.2

# cyber/state.h
STATE_UNINITIALIZED = 0
STATE_INITIALIZED = 1
STATE_SHUTTING_DOWN = 2
STATE_SHUTDOWN = 3


def _to_str(value):
    if isinstance(value, (bytes, bytearray)):
        return bytes(value).decode('utf-8')
    return value


def _payload(data):
    """Wire bytes of a protobuf message or raw bytes-like object."""
    if isinstance(data, (bytes, bytearray, memoryview)):
        return bytes(data)
    if isinstance(data, str):
        return data.encode('utf-8')
    return data.SerializeToString()


def _to_array(data):
    # Assigning an array('B') skips the per-byte checks of the generated
    # message setter, which would be very slow for large payloads.
    return array.array('B', data)


def _from_array(data):
    try:
        return data.tobytes()
    except AttributeError:
        return bytes(data)


class Sample(object):
    """What a reader receives from a natively typed channel, shaped like
    apollo_msgs/msg/ProtoMessage: ``type`` is the protobuf type name,
    ``data`` the protobuf wire bytes; ``message`` is the protobuf message
    object itself."""

    __slots__ = ('type', 'message', 'seq', 'stamp_ns', '_data')

    def __init__(self, type_name, message, seq=0, stamp_ns=0):
        self.type = type_name
        self.message = message
        self.seq = seq
        self.stamp_ns = stamp_ns
        self._data = None

    @property
    def data(self):
        if self._data is None:
            self._data = self.message.SerializePartialToString()
        return self._data


def _descriptor_of_type(data_type):
    """Protobuf descriptor of a message class or type name (None for a raw
    reader/writer without a known type)."""
    if data_type is None:
        return None
    if isinstance(data_type, str):
        if not data_type or data_type == RAWDATATYPE:
            return None
        return native_types.registry().find_descriptor(data_type)
    return getattr(data_type, 'DESCRIPTOR', None)


def _native_converter(channel, data_type):
    """Converter for the native ROS 2 type of a channel, None when the
    channel uses the protobuf wire format."""
    if native_types.registry().use_proto_wire(_to_str(channel)):
        return None
    if isinstance(data_type, str):
        if not data_type or data_type == RAWDATATYPE:
            return None
        return native_types.get_converter_by_name(data_type)
    return native_types.get_converter(_descriptor_of_type(data_type))


def _split_ros_arguments(argv):
    """The ROS 2 arguments of a command line: the `--ros-args ... [--]`
    sections (starting with the flag), without the rules that rename the
    node, and the node name given with `-r __node:=<name>` (or None).

    A process has many nodes: the name applies to the process node only, the
    other rules (remapping, namespace, parameters, parameter files) to every
    node, as in the C++ runtime."""
    ros_args = []
    in_ros_args = False
    for arg in argv:
        if not in_ros_args:
            if arg == '--ros-args':
                in_ros_args = True
                ros_args.append(arg)
            continue
        if arg == '--':
            in_ros_args = False
            continue
        ros_args.append(arg)
    node_args = []
    node_name = None
    index = 0
    while index < len(ros_args):
        arg = ros_args[index]
        if arg in ('-r', '--remap') and index + 1 < len(ros_args):
            rule = ros_args[index + 1]
            for prefix in ('__node:=', '__name:='):
                if rule.startswith(prefix):
                    node_name = rule[len(prefix):]
                    break
            else:
                node_args += [arg, rule]
            index += 2
            continue
        node_args.append(arg)
        index += 1
    if node_args == ['--ros-args']:
        node_args = []
    return ros_args, node_args, node_name


class _SpunNode(object):
    """An rclpy node spun by its own SingleThreadedExecutor thread.

    An ordinary ROS 2 node: parameter services, and the remapping rules,
    namespace and parameters given on the command line of the process."""

    def __init__(self, ros_name, context):
        self.node = rclpy.node.Node(
            ros_name, context=context, use_global_arguments=False,
            cli_args=_rt.node_args or None,
            automatically_declare_parameters_from_overrides=True)
        self.executor = SingleThreadedExecutor(context=context)
        self.executor.add_node(self.node)
        self.queues = []
        self.thread = threading.Thread(target=self._spin,
                                       name='cyber_py:' + ros_name,
                                       daemon=True)
        self.thread.start()

    def _spin(self):
        try:
            self.executor.spin()
        except Exception:  # pylint: disable=broad-except
            # ExternalShutdownException / context shut down: normal exit.
            if _rt.state == STATE_INITIALIZED and self.node is not None:
                traceback.print_exc()

    def destroy(self):
        own_thread = threading.current_thread() is self.thread
        for queue in self.queues:
            queue.close()
        try:
            self.executor.shutdown(timeout_sec=0 if own_thread else 1.0)
        except Exception:  # pylint: disable=broad-except
            pass
        try:
            self.node.destroy_node()
        except Exception:  # pylint: disable=broad-except
            pass
        if not own_thread:
            self.thread.join(timeout=1.0)


class _CallbackQueue(object):
    """Runs a reader's callbacks on its own thread.

    At most `depth` samples are pending; when the callback is slower than the
    writer the oldest pending sample is dropped (Cyber pending_queue_size).
    """

    def __init__(self, name, depth, handler):
        self._items = collections.deque(maxlen=max(1, int(depth)))
        self._cond = threading.Condition()
        self._closed = False
        self._handler = handler
        self._thread = threading.Thread(target=self._run,
                                        name='cyber_reader:' + name,
                                        daemon=True)
        self._thread.start()

    def push(self, item):
        with self._cond:
            self._items.append(item)
            self._cond.notify()

    def close(self):
        with self._cond:
            self._closed = True
            self._items.clear()
            self._cond.notify()

    def _run(self):
        while True:
            with self._cond:
                while not self._items and not self._closed:
                    self._cond.wait()
                if self._closed:
                    return
                item = self._items.popleft()
            try:
                self._handler(item)
            except Exception:  # pylint: disable=broad-except
                traceback.print_exc()


class _Runtime(object):
    """Process-wide state: rclpy context, spun nodes, signals."""

    def __init__(self):
        self.lock = threading.RLock()
        self.state = STATE_UNINITIALIZED
        self.context = None
        self.shutdown_event = threading.Event()
        self.init_time = 0.0
        self.module_name = ''
        self.nodes = []
        self.aux = None
        self.prev_sigint = None
        self.atexit_registered = False
        # ROS 2 arguments of the command line that apply to every node, and
        # the name given to the process with `-r __node:=<name>`.
        self.node_args = []
        self.process_node_name = None

    def spin_node(self, ros_name):
        with self.lock:
            if self.state != STATE_INITIALIZED:
                init()
            spun = _SpunNode(ros_name, self.context)
            self.nodes.append(spun)
            return spun

    def destroy_node(self, spun):
        with self.lock:
            if spun not in self.nodes:
                return
            self.nodes.remove(spun)
        spun.destroy()

    def aux_node(self):
        """The hidden helper node (clients, graph queries, sampling)."""
        with self.lock:
            if self.state != STATE_INITIALIZED:
                init()
            if self.aux is None:
                # Hidden (leading underscore) unless the command line names
                # the process with `-r __node:=<name>`, as launch_ros does.
                self.aux = self.spin_node(
                    self.process_node_name or '_cyber_py_%d' % os.getpid())
            return self.aux.node


_rt = _Runtime()
# topic -> protobuf type name, learnt from received samples
_channel_types = {}


def _on_sigint(signum, frame):
    if _rt.state != STATE_INITIALIZED:
        raise KeyboardInterrupt
    # Cyber's OnShutdown(): only switch state; the program decides when to
    # call shutdown(). A second Ctrl-C raises KeyboardInterrupt.
    _rt.state = STATE_SHUTTING_DOWN
    _rt.shutdown_event.set()


def _install_signal_handler():
    if threading.current_thread() is not threading.main_thread():
        return
    if _rt.prev_sigint is not None:
        return
    try:
        _rt.prev_sigint = signal.signal(signal.SIGINT, _on_sigint)
    except (ValueError, OSError):
        _rt.prev_sigint = None


def _restore_signal_handler():
    if _rt.prev_sigint is None:
        return
    if threading.current_thread() is not threading.main_thread():
        return
    try:
        if signal.getsignal(signal.SIGINT) is _on_sigint:
            signal.signal(signal.SIGINT, _rt.prev_sigint)
    except (ValueError, OSError, TypeError):
        pass
    _rt.prev_sigint = None


def _atexit():
    try:
        shutdown()
    except Exception:  # pylint: disable=broad-except
        pass


##
# @brief init cyber environment.
# @param module_name Used as the log file name.
#
# @return Success is True, otherwise False.
def init(module_name="cyber_py"):
    """
    init cyber environment.
    """
    with _rt.lock:
        if _rt.state == STATE_INITIALIZED:
            return True
        context = rclpy.context.Context()
        kwargs = {}
        if SignalHandlerOptions is not None:
            kwargs['signal_handler_options'] = SignalHandlerOptions.NO
        # The ROS 2 arguments of the command line (`--ros-args ...`): global
        # ones (log level, ...) go to rclpy, the rest to every node.
        ros_args, node_args, node_name = _split_ros_arguments(sys.argv[1:])
        try:
            rclpy.init(args=[sys.argv[0] if sys.argv else 'cyber_py'] +
                       ros_args, context=context, **kwargs)
        except Exception as e:  # pylint: disable=broad-except
            sys.stderr.write('cyber.init(%s) failed: %s\n' % (module_name, e))
            return False
        _rt.node_args = node_args
        _rt.process_node_name = node_name
        _rt.context = context
        _rt.nodes = []
        _rt.aux = None
        _rt.module_name = module_name
        _rt.init_time = time.monotonic()
        _rt.shutdown_event.clear()
        _rt.state = STATE_INITIALIZED
        _install_signal_handler()
        if not _rt.atexit_registered:
            atexit.register(_atexit)
            _rt.atexit_registered = True
        return True


def ok():
    """
    is cyber envi ok.
    """
    context = _rt.context
    return (_rt.state == STATE_INITIALIZED and context is not None and
            context.ok())


def shutdown():
    """
    shutdown cyber envi.
    """
    with _rt.lock:
        if _rt.state in (STATE_UNINITIALIZED, STATE_SHUTDOWN):
            return
        _rt.state = STATE_SHUTDOWN
        _rt.shutdown_event.set()
        context, nodes = _rt.context, list(_rt.nodes)
        _rt.nodes = []
        _rt.aux = None
    cyber_timer._stop_all()  # pylint: disable=protected-access
    for spun in nodes:
        spun.destroy()
    if context is not None:
        try:
            rclpy.shutdown(context=context)
        except Exception:  # pylint: disable=broad-except
            pass
    _restore_signal_handler()


def is_shutdown():
    """
    is cyber shutdown.
    """
    state = _rt.state
    if state in (STATE_SHUTTING_DOWN, STATE_SHUTDOWN):
        return True
    context = _rt.context
    return state == STATE_INITIALIZED and context is not None and \
        not context.ok()


def waitforshutdown():
    """
    wait until the cyber is shutdown.
    """
    while not is_shutdown():
        _rt.shutdown_event.wait(0.1)


def _guarded_call(func, *args):
    """Runs a user callback; exceptions are printed, never propagated into
    the executor (they would stop it)."""
    try:
        return func(*args)
    except Exception:  # pylint: disable=broad-except
        traceback.print_exc()
        return None


def _qos_for_reader(node, topic, depth, discovery_wait_s=0.5):
    """Keep-last `depth`; reliability/durability compatible with the
    publishers known on the graph (reliable/volatile otherwise).

    A freshly started process may not have discovered the publishers yet;
    wait up to `discovery_wait_s` for them so that latched (transient-local)
    channels such as /tf_static are subscribed with a matching QoS."""
    reliability = ReliabilityPolicy.RELIABLE
    durability = DurabilityPolicy.VOLATILE
    infos = []
    deadline = time.monotonic() + max(0.0, discovery_wait_s)
    while True:
        try:
            infos = node.get_publishers_info_by_topic(topic)
        except Exception:  # pylint: disable=broad-except
            infos = []
        if infos or time.monotonic() >= deadline:
            break
        time.sleep(0.05)
    if infos:
        if any(i.qos_profile.reliability == ReliabilityPolicy.BEST_EFFORT
               for i in infos):
            reliability = ReliabilityPolicy.BEST_EFFORT
        if all(i.qos_profile.durability == DurabilityPolicy.TRANSIENT_LOCAL
               for i in infos):
            durability = DurabilityPolicy.TRANSIENT_LOCAL
    return QoSProfile(history=HistoryPolicy.KEEP_LAST,
                      depth=max(1, int(depth)),
                      reliability=reliability, durability=durability)


def _writer_qos(depth):
    return QoSProfile(history=HistoryPolicy.KEEP_LAST,
                      depth=max(1, int(depth)),
                      reliability=ReliabilityPolicy.RELIABLE,
                      durability=DurabilityPolicy.VOLATILE)

# //////////////////////////////class//////////////////////////////


class Writer(object):

    """
    Class for cyber writer wrapper.
    """

    def __init__(self, name, writer, data_type, converter=None):
        self.name = name
        self.writer = writer
        self.data_type = data_type
        self.topic = channel_to_topic(name)
        # Set when the channel has a native ROS 2 type.
        self._converter = converter
        self._seq = 0
        self._lock = threading.Lock()

    ##
    # @brief write message.
    #
    # @param data is a message type (or raw protobuf bytes).
    #
    # @return Success is True, otherwise False.
    def write(self, data):
        """
        writer message string
        """
        try:
            if self._converter is not None:
                msg = self._converter.to_ros(self._converter.as_message(data))
            else:
                msg = ProtoMessage()
                msg.type = self.data_type
                msg.data = _to_array(_payload(data))
                with self._lock:
                    self._seq += 1
                    msg.seq = self._seq
                msg.stamp_ns = time.time_ns()
            self.writer.publish(msg)
        except Exception as e:  # pylint: disable=broad-except
            sys.stderr.write('cyber: write to %s failed: %s\n' % (self.name, e))
            return False
        return True

    def get_subscription_count(self):
        """Number of matched readers (ROS 2 subscriptions)."""
        try:
            return self.writer.get_subscription_count()
        except Exception:  # pylint: disable=broad-except
            return 0


class Reader(object):

    """
    Class for cyber reader wrapper.
    """

    def __init__(self, name, reader, data_type):
        self.name = name
        self.reader = reader
        self.data_type = data_type
        self.topic = channel_to_topic(name)

    def get_publisher_count(self):
        """Number of matched writers (ROS 2 publishers)."""
        try:
            return self.reader.get_publisher_count()
        except Exception:  # pylint: disable=broad-except
            return 0


class _Subscription(object):
    """The ROS 2 subscription of a reader.

    A typed reader knows the ROS 2 type of its channel. A raw reader does
    not: it follows the writers found on the ROS 2 graph, so its subscription
    is created once a writer is known (checked a few times per second)."""

    _RETRY_S = 0.2

    def __init__(self, ros_node, name, push, qos_depth, converter, raw):
        self._node = ros_node
        self._name = name
        self._topic = channel_to_topic(name)
        self._push = push
        self._qos_depth = qos_depth
        self._converter = converter
        self._raw = raw
        self._lock = threading.RLock()
        self._subscription = None
        self._timer = None
        self._closed = False
        self._seq = 0
        if not self._try_subscribe():
            self._timer = ros_node.create_timer(self._RETRY_S, self._retry)

    def _retry(self):
        if self._try_subscribe():
            with self._lock:
                timer, self._timer = self._timer, None
            if timer is not None:
                try:
                    timer.cancel()
                    self._node.destroy_timer(timer)
                except Exception:  # pylint: disable=broad-except
                    pass

    def _resolve(self):
        """(known, converter): the wire format of the channel."""
        if not self._raw:
            return True, self._converter
        reg = native_types.registry()
        has_proto_wire = False
        types = dict(self._node.get_topic_names_and_types()).get(
            self._topic, [])
        for ros_type in types:
            if ros_type == PROTO_MESSAGE_TYPE:
                has_proto_wire = True
                continue
            info = reg.find_by_ros_type(ros_type)
            if info is None or reg.use_proto_wire(self._name):
                continue
            converter = native_types.get_converter_by_name(info.proto_name)
            if converter is not None:
                return True, converter
        if has_proto_wire:
            return True, None
        local_type = _channel_types.get(self._topic)
        if local_type:
            return True, _native_converter(self._name, local_type)
        return False, None

    def _try_subscribe(self):
        with self._lock:
            if self._closed or self._subscription is not None:
                return True
            known, converter = self._resolve()
            if not known:
                return False
            qos = _qos_for_reader(self._node, self._topic, self._qos_depth)
            if converter is None:
                self._subscription = self._node.create_subscription(
                    ProtoMessage, self._topic, self._on_proto_message, qos)
            else:
                self._converter = converter
                _channel_types[self._topic] = converter.descriptor.full_name
                self._subscription = self._node.create_subscription(
                    converter.ros_class, self._topic, self._on_native_message,
                    qos)
            return True

    def _on_proto_message(self, ros_msg):
        _channel_types[self._topic] = ros_msg.type
        self._push(ros_msg)

    def _on_native_message(self, ros_msg):
        converter = self._converter
        try:
            message = converter.from_ros(ros_msg)
        except Exception as e:  # pylint: disable=broad-except
            sys.stderr.write('cyber: cannot convert %s on %s: %s\n' %
                             (converter.ros_type, self._name, e))
            return
        self._seq += 1
        self._push(Sample(converter.descriptor.full_name, message, self._seq,
                          time.time_ns()))

    def get_publisher_count(self):
        with self._lock:
            subscription = self._subscription
        if subscription is None:
            return 0
        return self._node.count_publishers(self._topic)

    def close(self):
        with self._lock:
            self._closed = True
            timer, self._timer = self._timer, None
            subscription, self._subscription = self._subscription, None
        for destroy, entity in ((self._node.destroy_timer, timer),
                                (self._node.destroy_subscription,
                                 subscription)):
            if entity is not None:
                try:
                    destroy(entity)
                except Exception:  # pylint: disable=broad-except
                    pass


class Client(object):

    """
    Class for cyber service client wrapper.
    """

    def __init__(self, client, data_type, name='', request_type='',
                 owner=None, converters=None):
        self.client = client
        self.data_type = data_type
        self.name = name
        self.request_type = request_type
        self._owner = owner
        # (request converter, response converter) of a native service type.
        self._converters = converters

    def destroy(self):
        """Destroys the underlying ROS 2 client."""
        owner, self._owner = self._owner, None
        if owner is not None:
            try:
                owner.destroy_client(self.client)
            except Exception:  # pylint: disable=broad-except
                pass

    def service_is_ready(self):
        try:
            return self.client.service_is_ready()
        except Exception:  # pylint: disable=broad-except
            return False

    def wait_for_service(self, timeout=None):
        """Waits until the service is available; False on timeout."""
        if self.service_is_ready():
            return True
        deadline = None if timeout is None else time.monotonic() + timeout
        while not self.service_is_ready():
            if is_shutdown() or (deadline is not None and
                                 time.monotonic() >= deadline):
                return False
            time.sleep(0.01)
        # The middleware reports a service as available as soon as this side
        # has matched the server; the server may need a moment more to match
        # this client, and a request sent before that is lost (Fast DDS).
        settle = SERVICE_DISCOVERY_SETTLE_S
        if deadline is not None:
            settle = min(settle, max(0.0, deadline - time.monotonic()))
        time.sleep(settle)
        return True

    ##
    # @brief send request message to service.
    #
    # @param data is a message type.
    # @param timeout seconds to wait for the response (Cyber default: 5).
    #
    # @return None or response from service.
    def send_request(self, data, timeout=DEFAULT_REQUEST_TIMEOUT_S):
        """
        send request to service
        """
        if not ok():
            return None
        deadline = time.monotonic() + timeout
        if self._converters is not None:
            try:
                request = self._converters[0].to_ros(
                    self._converters[0].as_message(data),
                    self.client.srv_type.Request())
            except Exception as e:  # pylint: disable=broad-except
                sys.stderr.write('cyber: bad request for %s: %s\n' %
                                 (self.name, e))
                return None
        else:
            request = ProtoService.Request()
            request.type = self.request_type or getattr(
                getattr(data, 'DESCRIPTOR', None), 'full_name', '')
            request.data = _to_array(_payload(data))
        if not self.wait_for_service(timeout):
            return None
        done = threading.Event()
        try:
            future = self.client.call_async(request)
        except Exception as e:  # pylint: disable=broad-except
            sys.stderr.write('cyber: request to %s failed: %s\n' %
                             (self.name, e))
            return None
        future.add_done_callback(lambda _: done.set())
        if not done.wait(max(0.0, deadline - time.monotonic())):
            try:
                self.client.remove_pending_request(future)
            except Exception:  # pylint: disable=broad-except
                pass
            return None
        response = future.result()
        if response is None:
            return None
        if self._converters is not None:
            message = self._converters[1].from_ros(response)
            if self.data_type is None or isinstance(self.data_type, str):
                return message.SerializePartialToString()
            if isinstance(message, self.data_type):
                return message
            result = self.data_type()
            result.ParseFromString(message.SerializePartialToString())
            return result
        if len(response.data) == 0:
            return None
        if self.data_type is None or isinstance(self.data_type, str):
            return _from_array(response.data)
        result = self.data_type()
        result.ParseFromString(_from_array(response.data))
        return result


class Service(object):

    """
    Class for cyber service wrapper.
    """

    def __init__(self, name, service, req_data_type, res_data_type):
        self.name = name
        self.service = service
        self.req_data_type = req_data_type
        self.res_data_type = res_data_type


def _type_name(data_type):
    if isinstance(data_type, str):
        return data_type
    return data_type.DESCRIPTOR.full_name


def _native_service(name, request_data_type, response_data_type):
    """(service class, request converter, response converter) of the native
    ROS 2 service type of a request/response pair, None when the service
    uses apollo_msgs/srv/ProtoService."""
    if native_types.registry().use_proto_wire(_to_str(name)):
        return None
    return native_types.get_service_converters(
        _descriptor_of_type(request_data_type),
        _descriptor_of_type(response_data_type))


class Node(object):

    """
    Class for cyber Node wrapper.
    """

    def __init__(self, name):
        self.name = name
        self._spun = _rt.spin_node(to_node_name(name))
        self.node = self._spun.node
        self.list_writer = []
        self.list_reader = []
        self.subs = {}
        self.pubs = {}
        self.list_client = []
        self.list_service = []
        self.mutex = threading.Lock()
        self.callbacks = {}
        self.services = {}

    def __del__(self):
        try:
            self.shutdown()
        except Exception:  # pylint: disable=broad-except
            pass

    def shutdown(self):
        """Destroys the node and all its readers/writers/services/clients."""
        spun = getattr(self, '_spun', None)
        if spun is None:
            return
        self._spun = None
        for reader in getattr(self, 'list_reader', []):
            close = getattr(reader.reader, 'close', None)
            if close is not None:
                close()
        self.node = None
        for c in getattr(self, 'list_client', []):
            c.destroy()
        _rt.destroy_node(spun)

    def get_ros_node(self):
        """The underlying rclpy.node.Node."""
        return self.node

    ##
    # @brief register proto message by proto descriptor file.
    #
    # @param file_desc object about datatype.DESCRIPTOR.file .
    def register_message(self, file_desc):
        """
        register proto message desc file.
        """
        proto_registry.register_file(file_desc)

    ##
    # @brief create a channel writer for send message to another channel.
    #
    # @param name is the channel name.
    # @param data_type is message class for serialization (or the protobuf
    # full name for a raw-bytes writer)
    # @param qos_depth is a queue size, which defines the size of the cache.
    #
    # @return return the writer object.
    def create_writer(self, name, data_type, qos_depth=1):
        """
        create a channel writer for send message to another channel.
        """
        if not isinstance(data_type, str):
            self.register_message(data_type.DESCRIPTOR.file)
        datatype = _type_name(data_type)
        topic = channel_to_topic(name)
        converter = _native_converter(name, data_type)
        publisher = self.node.create_publisher(
            ProtoMessage if converter is None else converter.ros_class,
            topic, _writer_qos(qos_depth))
        if datatype and datatype != RAWDATATYPE:
            _channel_types[topic] = datatype
        writer = Writer(name, publisher, datatype, converter)
        with self.mutex:
            self.list_writer.append(writer)
            self.pubs[name] = writer
        return writer

    def _subscribe(self, name, handler, qos_depth, data_type=None):
        """Subscribes `name`; `handler(sample)` runs on a dedicated reader
        thread with at most `qos_depth` pending samples. A sample is an
        apollo_msgs/msg/ProtoMessage or, on a natively typed channel, a
        Sample; both have `type` (protobuf type name) and `data` (wire
        bytes). Without `data_type` the reader is raw: it follows the type
        the writers of the channel use."""
        queue = _CallbackQueue(name, qos_depth, handler)
        raw = _descriptor_of_type(data_type) is None
        try:
            subscription = _Subscription(
                self.node, name, queue.push, qos_depth,
                None if raw else _native_converter(name, data_type), raw)
        except Exception:
            queue.close()
            raise
        self._spun.queues.append(queue)
        return subscription

    ##
    # @brief create a channel reader for receive message from another channel.
    #
    # @param name the channel name to read.
    # @param data_type  message class for serialization ("RawData" for bytes)
    # @param callback function to call (fn(data)) when data is received. If
    # args is set, the function must accept the args as a second argument,
    # i.e. fn(data, args)
    # @param args additional arguments to pass to the callback
    # @param qos_depth reader queue depth (Cyber pending_queue_size)
    #
    # @return return the reader object, None if the channel is already read.
    def create_reader(self, name, data_type, callback, args=None,
                      qos_depth=1):
        """
        create a channel reader for receive message from another channel.
        """
        with self.mutex:
            if name in self.subs:
                return None
            self.subs[name] = None

        msg_class = None
        if isinstance(data_type, str):
            if data_type != RAWDATATYPE:
                msg_class = proto_registry.get_message_class(data_type)
        else:
            msg_class = data_type
            self.register_message(data_type.DESCRIPTOR.file)

        def on_message(ros_msg):
            message = getattr(ros_msg, 'message', None)
            if msg_class is None:
                data = _from_array(ros_msg.data)
            elif message is not None and isinstance(message, msg_class):
                data = message        # converted from the native ROS 2 type
            else:
                data = msg_class()
                try:
                    data.ParseFromString(_from_array(ros_msg.data))
                except Exception:  # pylint: disable=broad-except
                    sys.stderr.write('cyber: cannot parse %s as %s on %s\n' %
                                     (ros_msg.type,
                                      msg_class.DESCRIPTOR.full_name, name))
                    return
            if args is None:
                _guarded_call(callback, data)
            else:
                _guarded_call(callback, data, args)

        try:
            subscription = self._subscribe(
                name, on_message, qos_depth,
                msg_class if msg_class is not None else None)
        except Exception:
            with self.mutex:
                self.subs.pop(name, None)
            raise
        reader = Reader(name, subscription, data_type)
        with self.mutex:
            self.list_reader.append(reader)
            self.subs[name] = reader
        return reader

    def create_rawdata_reader(self, name, callback, args=None, qos_depth=1):
        """
        Create RawData reader:listener RawMessage
        """
        return self.create_reader(name, RAWDATATYPE, callback, args,
                                  qos_depth)

    def create_ros_reader(self, name, callback, qos_depth=10):
        """Raw reader whose callback receives the sample envelope: an
        apollo_msgs/msg/ProtoMessage or, on a natively typed channel, a
        Sample (protobuf type name, seq, stamp_ns and data). Used by tools."""
        subscription = self._subscribe(
            name, lambda ros_msg: _guarded_call(callback, ros_msg), qos_depth)
        reader = Reader(name, subscription, PROTO_MESSAGE_TYPE)
        with self.mutex:
            self.list_reader.append(reader)
        return reader

    ##
    # @brief create client for the c/s.
    #
    # @param name the service name.
    # @param request_data_type the request message type.
    # @param response_data_type the response message type.
    #
    # @return the client object.
    def create_client(self, name, request_data_type, response_data_type):
        for data_type in (request_data_type, response_data_type):
            if not isinstance(data_type, str):
                self.register_message(data_type.DESCRIPTOR.file)
        # Clients live on the helper node (own executor thread), so that a
        # request sent from any callback can always receive its response.
        aux = _rt.aux_node()
        native = _native_service(name, request_data_type, response_data_type)
        client = aux.create_client(
            ProtoService if native is None else native[0],
            channel_to_topic(name))
        c = Client(client, response_data_type, name,
                   _type_name(request_data_type), aux,
                   None if native is None else native[1:])
        with self.mutex:
            self.list_client.append(c)
        return c

    ##
    # @brief create client for the c/s.
    #
    # @param name the service name.
    # @param req_data_type the request message type.
    # @param res_data_type the response message type.
    # @param callback function to call (fn(data)) when data is received. If
    # args is set, the function must accept the args as a second argument,
    # i.e. fn(data, args)
    # @param args additional arguments to pass to the callback.
    #
    # @return return the service object.
    def create_service(self, name, req_data_type, res_data_type, callback,
                       args=None):
        with self.mutex:
            if name in self.services:
                return None
            self.services[name] = None
        for data_type in (req_data_type, res_data_type):
            if not isinstance(data_type, str):
                self.register_message(data_type.DESCRIPTOR.file)
        res_type_name = _type_name(res_data_type)
        native = _native_service(name, req_data_type, res_data_type)

        def handle(request):
            """Runs the user callback; returns the response (wire bytes, or
            the message object for a native service type)."""
            if native is not None:
                message = native[1].from_ros(request)
                if isinstance(req_data_type, str):
                    req = message.SerializePartialToString()
                elif isinstance(message, req_data_type):
                    req = message
                else:
                    req = req_data_type()
                    req.ParseFromString(message.SerializePartialToString())
            elif isinstance(req_data_type, str):
                req = _from_array(request.data)
            else:
                req = req_data_type()
                try:
                    req.ParseFromString(_from_array(request.data))
                except Exception:  # pylint: disable=broad-except
                    sys.stderr.write('cyber: bad request on service %s\n' %
                                     name)
                    return None
            if args is None:
                result = _guarded_call(callback, req)
            else:
                result = _guarded_call(callback, req, args)
            if result is None:
                return None
            return result if native is not None else _payload(result)

        async def on_request(request, response):
            # The callback may block (e.g. call another service of this
            # node): run it off the executor thread and await the result.
            future = rclpy.task.Future()

            def work():
                data = None
                try:
                    data = handle(request)
                finally:
                    future.set_result(data)

            threading.Thread(target=work, name='cyber_service:' + name,
                             daemon=True).start()
            data = await future
            if native is not None:
                if data is not None:
                    try:
                        native[2].to_ros(native[2].as_message(data), response)
                    except Exception as e:  # pylint: disable=broad-except
                        sys.stderr.write('cyber: bad response of service '
                                         '%s: %s\n' % (name, e))
                return response
            response.type = res_type_name
            if data is not None:
                response.data = _to_array(data)
            return response

        try:
            service = self.node.create_service(
                ProtoService if native is None else native[0],
                channel_to_topic(name), on_request,
                callback_group=MutuallyExclusiveCallbackGroup())
        except Exception:
            with self.mutex:
                self.services.pop(name, None)
            raise
        s = Service(name, service, req_data_type, res_data_type)
        with self.mutex:
            self.list_service.append(s)
            self.services[name] = s
        return s

    def spin(self):
        """
        Block until cyber is shut down (callbacks run in the background).
        """
        while not is_shutdown():
            _rt.shutdown_event.wait(0.1)


# //////////////////////////// graph queries ////////////////////////////


def graph_node():
    """The process-wide (hidden) rclpy node used for graph queries."""
    return _rt.aux_node()


def wait_for_discovery(sleep_s):
    """Sleeps until `sleep_s` seconds have passed since init(), giving DDS
    discovery time to populate the graph (Cyber utils slept sleep_s)."""
    graph_node()
    remaining = _rt.init_time + float(sleep_s) - time.monotonic()
    if remaining > 0:
        time.sleep(remaining)


# After the initial `sleep_s` wait, graph queries keep polling while the
# answer still changes (DDS discovery can be slow on a loaded machine).
_SETTLE_QUIET_S = 0.5
_SETTLE_MAX_EXTRA_S = 3.0


def _settled(query, sleep_s):
    """query() once discovery had `sleep_s` seconds and its result stopped
    changing (quiet for _SETTLE_QUIET_S, at most _SETTLE_MAX_EXTRA_S more).
    An empty result is never considered settled: when the first discovery
    announcements are missed, Fast DDS only re-announces ~3 s later.
    With sleep_s == 0 the graph is queried once, immediately."""
    wait_for_discovery(sleep_s)
    result = query()
    if not sleep_s:
        return result
    deadline = time.monotonic() + _SETTLE_MAX_EXTRA_S
    stable_since = time.monotonic()
    while time.monotonic() < deadline:
        if result and time.monotonic() - stable_since >= _SETTLE_QUIET_S:
            break
        time.sleep(0.1)
        current = query()
        if current != result:
            result, stable_since = current, time.monotonic()
    return result


def _found(lookup, sleep_s):
    """lookup() after the discovery wait, retried for up to
    _SETTLE_MAX_EXTRA_S while it returns nothing (unless sleep_s == 0)."""
    wait_for_discovery(sleep_s)
    result = lookup()
    deadline = time.monotonic() + (_SETTLE_MAX_EXTRA_S if sleep_s else 0)
    while not result and time.monotonic() < deadline:
        time.sleep(0.1)
        result = lookup()
    return result


def _is_internal_node(name):
    # Hidden ROS 2 nodes start with '_' (ours, ros2cli daemons); names that
    # to_node_name() prefixed because of a leading digit stay visible.
    if name.startswith('_') and not name[1:2].isdigit():
        return True
    return name.startswith('apollo_cyber_') and name[13:].isdigit()


def _endpoints(infos):
    """Topic endpoints minus those of internal helper nodes (e.g. the
    sampling subscription of get_msgtype())."""
    return [i for i in infos if not _is_internal_node(i.node_name)]


def _full_node_name(name, namespace):
    if not namespace or namespace == '/':
        return name
    return namespace.rstrip('/') + '/' + name


def _graph_nodes():
    """[(full name, name, namespace)] of the visible nodes on the graph."""
    result = []
    for name, namespace in graph_node().get_node_names_and_namespaces():
        if _is_internal_node(name):
            continue
        result.append((_full_node_name(name, namespace), name, namespace))
    return result


def _find_node(node_name):
    node_name = _to_str(node_name)
    candidates = (node_name, node_name.lstrip('/'), to_node_name(node_name))
    for full, name, namespace in _graph_nodes():
        if full in candidates or full.lstrip('/') in candidates or \
                name in candidates:
            return full, name, namespace
    return None


def _graph_servers():
    """[(service name, full node name, node name)] of the Apollo services
    *served* on the graph (get_service_names_and_types() also lists names
    that only have clients)."""
    node = graph_node()
    result = []
    for name, namespace in node.get_node_names_and_namespaces():
        try:
            entries = node.get_service_names_and_types_by_node(name, namespace)
        except Exception:  # pylint: disable=broad-except
            continue
        for service, types in entries:
            if _is_apollo_service(types):
                result.append(
                    (service, _full_node_name(name, namespace), name))
    return result


def _is_apollo_channel(ros_types):
    """True when a topic with these ROS 2 types is an Apollo channel: a
    native Apollo type or apollo_msgs/msg/ProtoMessage."""
    reg = native_types.registry()
    return any(reg.is_apollo_topic_type(t) for t in ros_types)


def _is_apollo_service(ros_types):
    reg = native_types.registry()
    return any(reg.is_apollo_service_type(t) for t in ros_types)


def channel_message_type(topic, ros_types=None):
    """Protobuf type name of a channel ('' when unknown): from the native
    ROS 2 type on the graph, else what this process learnt from samples."""
    if ros_types is None:
        ros_types = dict(graph_node().get_topic_names_and_types()).get(
            topic, [])
    return (native_types.registry().proto_type_of(ros_types) or
            _channel_types.get(topic, ''))


def _local_node_names():
    with _rt.lock:
        return set(spun.node.get_name() for spun in _rt.nodes)


def _gid_hash(gid):
    return int.from_bytes(hashlib.blake2b(bytes(gid), digest_size=8).digest(),
                          'little')


def _qos_to_cyber(qos, attr_qos):
    if qos.history in (HistoryPolicy.KEEP_LAST, HistoryPolicy.KEEP_ALL):
        attr_qos.history = int(qos.history)
    else:
        attr_qos.history = _cyber_protos.QosHistoryPolicy[
            'HISTORY_SYSTEM_DEFAULT']
    if qos.depth:
        attr_qos.depth = int(qos.depth)
    if qos.reliability in (ReliabilityPolicy.RELIABLE,
                           ReliabilityPolicy.BEST_EFFORT):
        attr_qos.reliability = int(qos.reliability)
    if qos.durability in (DurabilityPolicy.TRANSIENT_LOCAL,
                          DurabilityPolicy.VOLATILE):
        attr_qos.durability = int(qos.durability)


def endpoint_role_attributes(topic, info):
    """cyber.proto.RoleAttributes describing a ROS 2 TopicEndpointInfo."""
    attr = _cyber_protos.RoleAttributes()
    attr.channel_name = topic
    attr.node_name = _full_node_name(info.node_name, info.node_namespace)
    attr.message_type = channel_message_type(
        topic, [getattr(info, 'topic_type', '')])
    attr.id = _gid_hash(info.endpoint_gid)
    _qos_to_cyber(info.qos_profile, attr.qos_profile)
    if info.node_name in _local_node_names():
        attr.host_name = socket.gethostname()
        attr.process_id = os.getpid()
    return attr


def sample_message(channel_name, timeout=1.0):
    """Waits up to `timeout` s for one sample of a channel and returns its
    envelope (an apollo_msgs/msg/ProtoMessage, or a Sample on a natively
    typed channel: both have `type` and `data`), or None."""
    node = graph_node()
    received = []
    event = threading.Event()

    def on_sample(sample):
        received.append(sample)
        event.set()

    subscription = _Subscription(node, _to_str(channel_name), on_sample, 1,
                                 None, True)
    try:
        event.wait(max(0.0, timeout))
    finally:
        subscription.close()
    return received[0] if received else None


class ChannelUtils(object):

    @staticmethod
    ##
    # @brief Parse rawmsg from rawmsg data by message type.
    #
    # @param msg_type message type.
    # @param rawmsgdata rawmsg data.
    #
    # @return a human readable form of this message (utf-8 bytes). For
    # debugging and other purposes.
    def get_debugstring_rawmsgdata(msg_type, rawmsgdata):
        return proto_registry.to_debug_string(
            _to_str(msg_type), rawmsgdata).encode('utf-8')

    @staticmethod
    ##
    # @brief Get the protobuf message type of a channel.
    #
    # A natively typed channel names its protobuf type through its ROS 2
    # type. On the protobuf wire format (apollo_msgs/msg/ProtoMessage) the
    # type is learnt from a sample: a cached one when this process already
    # received the channel, otherwise by waiting up to max(sleep_s, 0.5)
    # seconds for a message.
    #
    # @param channel_name channel name.
    # @param sleep_s wait time.
    #
    # @return the message type of this channel (bytes, b'' if unknown).
    def get_msgtype(channel_name, sleep_s=2):
        topic = channel_to_topic(_to_str(channel_name))
        msg_type = _found(lambda: channel_message_type(topic), sleep_s)
        if not msg_type:
            msg = sample_message(topic, max(float(sleep_s), 0.5))
            msg_type = msg.type if msg is not None else ''
        return msg_type.encode('utf-8')

    @staticmethod
    ##
    # @brief Get all active channel names
    #
    # @param sleep_s wait time for topo discovery.
    #
    # @return all active channel (ROS 2 topic) names.
    def get_channels(sleep_s=2):
        return _settled(lambda: sorted(
            name for name, types in graph_node().get_topic_names_and_types()
            if _is_apollo_channel(types)), sleep_s)

    @staticmethod
    ##
    # @brief Get the active channel info.
    #
    # @param sleep_s wait time for topo discovery.
    #
    # @return all active channels info. {'channel1':[], 'channel2':[]}, the
    # values being serialized cyber.proto.RoleAttributes (writers first).
    def get_channels_info(sleep_s=2):
        node = graph_node()
        result = {}
        for topic in ChannelUtils.get_channels(sleep_s):
            roles = []
            for info in _endpoints(node.get_publishers_info_by_topic(topic)):
                roles.append(
                    endpoint_role_attributes(topic, info).SerializeToString())
            for info in _endpoints(
                    node.get_subscriptions_info_by_topic(topic)):
                roles.append(
                    endpoint_role_attributes(topic, info).SerializeToString())
            result[topic] = roles
        return result

    @staticmethod
    def get_writers_info(channel_name, sleep_s=0):
        """[cyber.proto.RoleAttributes] of the writers of a channel."""
        wait_for_discovery(sleep_s)
        topic = channel_to_topic(_to_str(channel_name))
        return [endpoint_role_attributes(topic, info) for info in
                _endpoints(graph_node().get_publishers_info_by_topic(topic))]

    @staticmethod
    def get_readers_info(channel_name, sleep_s=0):
        """[cyber.proto.RoleAttributes] of the readers of a channel."""
        wait_for_discovery(sleep_s)
        topic = channel_to_topic(_to_str(channel_name))
        return [endpoint_role_attributes(topic, info) for info in
                _endpoints(
                    graph_node().get_subscriptions_info_by_topic(topic))]


class NodeUtils(object):

    @staticmethod
    ##
    # @brief Get all active node names.
    #
    # @param sleep_s wait time for topo discovery.
    #
    # @return all active node names.
    def get_nodes(sleep_s=2):
        return _settled(
            lambda: sorted(set(full for full, _, _ in _graph_nodes())),
            sleep_s)

    @staticmethod
    ##
    # @brief Get node attribute by the node name.
    #
    # @param node_name node name.
    # @param sleep_s wait time for topo discovery.
    #
    # @return the node's attribute (serialized cyber.proto.RoleAttributes),
    # b'' when there is no such node.
    def get_node_attr(node_name, sleep_s=2):
        found = _found(lambda: _find_node(node_name), sleep_s)
        if found is None:
            return b''
        attr = _cyber_protos.RoleAttributes()
        attr.node_name = found[0]
        attr.node_id = int.from_bytes(hashlib.blake2b(
            found[0].encode('utf-8'), digest_size=8).digest(), 'little')
        if found[1] in _local_node_names():
            attr.host_name = socket.gethostname()
            attr.process_id = os.getpid()
        return attr.SerializeToString()

    @staticmethod
    def _channels_of_node(node_name, sleep_s, writers):
        found = _found(lambda: _find_node(node_name), sleep_s)
        if found is None:
            return []
        node = graph_node()
        query = (node.get_publisher_names_and_types_by_node if writers else
                 node.get_subscriber_names_and_types_by_node)
        try:
            entries = query(found[1], found[2])
        except Exception:  # pylint: disable=broad-except
            return []
        return [name for name, types in entries if _is_apollo_channel(types)]

    @staticmethod
    ##
    # @brief Get node's reader channel names
    #
    # @param node_name the node name.
    # @param sleep_s wait time for topo discovery.
    #
    # @return node's reader channel names.
    def get_readersofnode(node_name, sleep_s=2):
        return NodeUtils._channels_of_node(node_name, sleep_s, False)

    @staticmethod
    ##
    # @brief Get node's writer channel names.
    #
    # @param node_name the node name.
    # @param sleep_s wait time for topo discovery.
    #
    # @return node's writer channel names.
    def get_writersofnode(node_name, sleep_s=2):
        return NodeUtils._channels_of_node(node_name, sleep_s, True)

    @staticmethod
    def get_servicesofnode(node_name, sleep_s=2):
        """Names of the Apollo services served by a node."""
        found = _found(lambda: _find_node(node_name), sleep_s)
        if found is None:
            return []
        try:
            entries = graph_node().get_service_names_and_types_by_node(
                found[1], found[2])
        except Exception:  # pylint: disable=broad-except
            return []
        return [name for name, types in entries if _is_apollo_service(types)]


class ServiceUtils(object):

    @staticmethod
    ##
    # @brief Get all active service names.
    #
    # @param sleep_s wait time for topo discovery.
    #
    # @return all active service names.
    def get_services(sleep_s=2):
        return _settled(lambda: sorted(set(
            service for service, _, _ in _graph_servers())), sleep_s)

    @staticmethod
    ##
    # @brief Get service attribute by the service name.
    #
    # @param service_name service name.
    # @param sleep_s wait time for topo discovery.
    #
    # @return the service's attribute (serialized cyber.proto.RoleAttributes
    # naming the serving node), b'' when there is no such service.
    def get_service_attr(service_name, sleep_s=2):
        service = channel_to_topic(_to_str(service_name))
        servers = _found(lambda: [entry for entry in _graph_servers()
                                  if entry[0] == service], sleep_s)
        for name, full, node_name in servers:
            attr = _cyber_protos.RoleAttributes()
            attr.service_name = service
            attr.node_name = full
            if node_name in _local_node_names():
                attr.host_name = socket.gethostname()
                attr.process_id = os.getpid()
            return attr.SerializeToString()
        return b''
