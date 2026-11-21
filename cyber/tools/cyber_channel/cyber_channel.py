#!/usr/bin/env python3
# ****************************************************************************
# Copyright 2019 The Apollo Authors. All Rights Reserved.

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
"""cyber_channel: inspect Apollo channels on ROS 2.

Every Apollo channel is a ROS 2 topic of type apollo_msgs/msg/ProtoMessage;
payloads are decoded with the protobuf type carried in each sample. Channel
names may be given in Cyber form (e.g. /apollo/sensor/lidar-16/PointCloud2);
they are mapped with cyber.python.cyber_py3.naming.channel_to_topic().
"""

import math
import os
import sys
import threading
import time
from optparse import OptionParser

from cyber.python.cyber_py3 import cyber
from cyber.python.cyber_py3 import cyber_time
from cyber.python.cyber_py3 import proto_registry
from cyber.python.cyber_py3.naming import channel_to_topic

_QOS_NAMES = {
    'history': {0: 'system_default', 1: 'keep_last', 2: 'keep_all'},
    'reliability': {0: 'system_default', 1: 'reliable', 2: 'best_effort'},
    'durability': {0: 'system_default', 1: 'transient_local', 2: 'volatile'},
}


def _parse_window(parser, value):
    try:
        return int(value)
    except (TypeError, ValueError):
        parser.error("window size must be an integer")


def print_channel_type(channel_name):
    msgtype = cyber.ChannelUtils.get_msgtype(channel_name).decode('utf-8')
    if not msgtype:
        msgtype = 'unknown: no message received'
    print(channel_to_topic(channel_name), " type is [", msgtype, "]")


def _channel_cmd_type(argv):
    """
    Command-line parsing for 'cyber_channel type channelname' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_channel type channelname")
    (options, args) = parser.parse_args(args)
    if len(args) > 1:
        parser.error("you may only specify one input channel")
    elif len(args) == 0:
        parser.error("channelname must be specified")
    print_channel_type(args[0])


class CyberChannelBw(object):

    def __init__(self, window_size=100):
        self.lock = threading.Lock()
        self.sizes = []
        self.times = []
        self.window_size = window_size
        if window_size <= 0 or window_size > 50000:
            self.window_size = 100
        print("bw window_size: ", self.window_size)

    def callback_bw(self, ros_msg):
        with self.lock:
            self.times.append(time.time())
            self.sizes.append(len(ros_msg.data))
            if len(self.times) > self.window_size:
                self.times.pop(0)
                self.sizes.pop(0)

    def print_bw(self):
        with self.lock:
            if len(self.times) < 2:
                return
            n = len(self.times)
            tn = time.time()
            t0 = self.times[0]

            total = sum(self.sizes)
            bytes_per_s = total / (tn - t0)
            mean = total / n

            max_s = max(self.sizes)
            min_s = min(self.sizes)

        if bytes_per_s < 1000:
            bw, mean, min_s, max_s = [
                "%.2fB" % v for v in [bytes_per_s, mean, min_s, max_s]]
        elif bytes_per_s < 1000000:
            bw, mean, min_s, max_s = ["%.2fKB" % (v / 1000)
                                      for v in [bytes_per_s, mean, min_s, max_s]]
        else:
            bw, mean, min_s, max_s = ["%.2fMB" % (v / 1000000)
                                      for v in [bytes_per_s, mean, min_s, max_s]]

        print("average: %s/s\n\tmean: %s min: %s max: %s window: %s" %
              (bw, mean, min_s, max_s, n))
        sys.stdout.flush()


def channel_bw(channel_name, window_size):
    rt = CyberChannelBw(window_size)
    node_bw = cyber.Node("listener_node_bw")
    node_bw.create_ros_reader(channel_name, rt.callback_bw, qos_depth=100)
    print("reader to [%s]" % channel_to_topic(channel_name))
    while not cyber.is_shutdown():
        time.sleep(1.0)
        rt.print_bw()


def _channel_cmd_bw(argv):
    """
    Command-line parsing for 'cyber_channel bw' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_channel bw channelname")
    parser.add_option("-w", "--window",
                      dest="window_size", default=-1,
                      help="window size for calculating rate, max size is 50000")
    (options, args) = parser.parse_args(args)
    if len(args) == 0:
        parser.error("channelname must be specified")
    if len(args) > 1:
        parser.error("param is too much")
    channel_bw(args[0], _parse_window(parser, options.window_size))


class CyberChannelHz(object):

    """
    CyberChannelHz receives messages for a topic and computes frequency stats
    """

    def __init__(self, window_size):
        self.lock = threading.Lock()
        self.last_printed_tn = 0
        self.msg_t0 = -1.
        self.msg_tn = 0
        self.times = []

        if window_size <= 0 or window_size > 50000:
            window_size = 50000
        self.window_size = window_size
        print("hz window_size: ", window_size)

    def callback_hz(self, m):
        with self.lock:
            curr_time = cyber_time.Time.now().to_sec()

            if curr_time == 0:
                if len(self.times) > 0:
                    print("reset times.")
                    self.times = []
                return

            if self.msg_t0 < 0 or self.msg_t0 > curr_time:
                self.msg_t0 = curr_time
                self.msg_tn = curr_time
                self.times = []
            else:
                self.times.append(curr_time - self.msg_tn)
                self.msg_tn = curr_time

            if len(self.times) > self.window_size - 1:
                self.times.pop(0)

    def print_hz(self):
        """
        print the average publishing rate to screen
        """
        with self.lock:
            if not self.times:
                return
            elif self.msg_tn == self.last_printed_tn:
                print("no new messages")
                return
            n = len(self.times)
            mean = sum(self.times) / n
            rate = 1. / mean if mean > 0. else 0

            std_dev = math.sqrt(sum((x - mean) ** 2 for x in self.times) / n)

            max_delta = max(self.times)
            min_delta = min(self.times)

            self.last_printed_tn = self.msg_tn
        print("average rate: %.3f\n\tmin: %.3fs max: %.3fs std dev: %.5fs window: %s" %
              (rate, min_delta, max_delta, std_dev, n + 1))
        sys.stdout.flush()


def channel_hz(channel_name, window_size):
    rt = CyberChannelHz(window_size)
    node_hz = cyber.Node("listener_node_hz")
    node_hz.create_ros_reader(channel_name, rt.callback_hz, qos_depth=100)
    print("reader to [%s]" % channel_to_topic(channel_name))
    while not cyber.is_shutdown():
        time.sleep(1.0)
        rt.print_hz()


def _channel_cmd_hz(argv):
    """
    Command-line parsing for 'cyber_channel hz' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_channel hz channelname")
    parser.add_option("-w", "--window",
                      dest="window_size", default=-1,
                      help="window size for calculating rate, max size is 50000")
    (options, args) = parser.parse_args(args)
    if len(args) == 0:
        parser.error("channelname must be specified")
    if len(args) > 1:
        parser.error("param is too much")
    channel_hz(args[0], _parse_window(parser, options.window_size))


def print_role(role, kind):
    qos = role.qos_profile
    print("\trole\t\t", kind)
    print("\troleid\t\t", role.id)
    if role.host_name:
        print("\thostname\t", role.host_name)
    if role.process_id:
        print("\tprocessid\t", role.process_id)
    print("\tnodename\t", role.node_name)
    print("\tmsgtype\t\t", role.message_type or "unknown")
    print("\tqos\t\t %s, %s, %s%s" % (
        _QOS_NAMES['reliability'].get(qos.reliability, qos.reliability),
        _QOS_NAMES['durability'].get(qos.durability, qos.durability),
        _QOS_NAMES['history'].get(qos.history, qos.history),
        (", depth %d" % qos.depth) if qos.HasField('depth') else ""))


def _print_channel_info(channel):
    writers = cyber.ChannelUtils.get_writers_info(channel)
    readers = cyber.ChannelUtils.get_readers_info(channel)
    print(channel)
    for role in writers:
        print_role(role, "writer")
    for role in readers:
        print_role(role, "reader")


def channel_info(channel_name):
    channels = cyber.ChannelUtils.get_channels()
    if channel_name != "":
        topic = channel_to_topic(channel_name)
        if topic not in channels:
            print("channel [%s] is not active" % topic)
            return
        if writers_have_unknown_type(topic):
            # learn the protobuf type from one sample
            cyber.ChannelUtils.get_msgtype(topic, 1)
        _print_channel_info(topic)
    else:
        print("The number of channels is: ", len(channels))
        for channel in channels:
            _print_channel_info(channel)


def writers_have_unknown_type(topic):
    return any(not role.message_type
               for role in cyber.ChannelUtils.get_writers_info(topic))


def _channel_cmd_info(argv):
    """
    Command-line parsing for 'cyber_channel info' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_channel info channelname ")
    parser.add_option("-a", "--all",
                      dest="all_channels", default=False,
                      action="store_true",
                      help="display all channels info")

    (options, args) = parser.parse_args(args)
    if len(args) == 0 and not options.all_channels:
        parser.error("channelname must be specified")
    elif len(args) > 1:
        parser.error("you may only specify one topic name")
    elif len(args) == 1:
        channel_info(args[0])
    elif len(args) == 0 and options.all_channels:
        channel_info("")


def print_channel_list():
    channels = cyber.ChannelUtils.get_channels()
    print("The number of channels is: ", len(channels))
    for channel in channels:
        print(channel)


def _channel_cmd_list(argv):
    """
    Command-line parsing for 'cyber_channel list' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_channel list")
    (options, args) = parser.parse_args(args)
    if len(args) > 0:
        parser.error("param is too much")
    print_channel_list()


class CyberChannelecho(object):

    def __init__(self, channel_name, count=0):
        self.channel_name = channel_name
        self.count = count
        self.received = 0
        self.done = threading.Event()

    def callback(self, ros_msg):
        """
        Channel echo callback: decode the payload with its protobuf type.
        """
        if self.done.is_set():
            return
        data = ros_msg.data.tobytes()
        text = proto_registry.to_debug_string(ros_msg.type, data)
        if text or not data:
            print(text)
        else:
            print("[cannot decode %d bytes of type '%s': no python protobuf "
                  "module defines it]" % (len(data), ros_msg.type))
        sys.stdout.flush()
        self.received += 1
        if self.count and self.received >= self.count:
            self.done.set()


def channel_echo(channel_name, count=0):
    """
    Reader message.
    """
    node_echo = cyber.Node("listener_node_echo")
    echo_cb = CyberChannelecho(channel_name, count)
    node_echo.create_ros_reader(channel_name, echo_cb.callback, qos_depth=10)
    while not cyber.is_shutdown() and not echo_cb.done.wait(0.1):
        pass


def _channel_cmd_echo(argv):
    """
    Command-line parsing for 'cyber_channel echo' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_channel echo channelname")
    parser.add_option("-n", "--count", dest="count", type="int", default=0,
                      help="exit after printing COUNT messages")
    (options, args) = parser.parse_args(args)
    if len(args) > 1:
        parser.error("you may only specify one input channel")
    elif len(args) == 0:
        parser.error("channelname must be specified")
    channel_echo(args[0], options.count)


def _printallusage():
    print("""cyber_channel is a command-line tool for printing information about Apollo channels (ROS 2 topics).

Commands:
\tcyber_channel list\tlist active channels
\tcyber_channel info\tprint information about active channel
\tcyber_channel echo\tprint messages to screen
\tcyber_channel hz\tdisplay publishing rate of channel
\tcyber_channel bw\tdisplay bandwidth used by channel
\tcyber_channel type\tprint channel type

Type cyber_channel <command> -h for more detailed usage, e.g. 'cyber_channel echo -h'
""")
    sys.exit(getattr(os, 'EX_USAGE', 1))


_COMMANDS = {
    'list': _channel_cmd_list,
    'info': _channel_cmd_info,
    'echo': _channel_cmd_echo,
    'hz': _channel_cmd_hz,
    'bw': _channel_cmd_bw,
    'type': _channel_cmd_type,
}


def main(argv=None):
    argv = list(sys.argv if argv is None else argv)
    if len(argv) == 1 or argv[1] not in _COMMANDS:
        _printallusage()
    cyber.init("cyber_channel")
    try:
        _COMMANDS[argv[1]](argv)
    finally:
        cyber.shutdown()


if __name__ == '__main__':
    main()
