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
"""Convert Cyber .record files into a rosbag2 bag.

Every channel becomes the topic naming.channel_to_topic(channel) of type
apollo_msgs/msg/ProtoMessage (type = protobuf full name, seq = per-channel
message index, stamp_ns = record time, data = the protobuf bytes); the bag
timestamp of each message is its record time. Several records (e.g. the
segments of one recording) are merged into one bag, in the order given.

The original channel name, protobuf type and proto descriptor of every
channel are kept in the bag's custom metadata (keys apollo.channel.<topic>,
apollo.type.<topic>, apollo.proto_desc.<topic> = base64(zlib(desc))), so that
rosbag2_to_record.py restores the record losslessly.

Examples:
  record_to_rosbag2.py demo.record -o demo_bag
  record_to_rosbag2.py demo.record.0000* -o demo_bag -s sqlite3 \\
      -c /apollo/localization/pose -c /apollo/canbus/chassis
"""

import argparse
import array
import base64
import datetime
import os
import sys
import zlib

import rosbag2_py
from rclpy.serialization import serialize_message

from apollo_msgs.msg import ProtoMessage

from cyber.python.cyber_py3 import record
from cyber.python.cyber_py3.naming import channel_to_topic

PROTO_MESSAGE_TYPE = 'apollo_msgs/msg/ProtoMessage'
CUSTOM_CHANNEL = 'apollo.channel.'
CUSTOM_TYPE = 'apollo.type.'
CUSTOM_PROTO_DESC = 'apollo.proto_desc.'
UINT64_MAX = (1 << 64) - 1


def parse_time(value):
    """Nanoseconds since epoch, or a local time 'YYYY-mm-dd-HH:MM:SS' like
    cyber_recorder's --begin/--end."""
    if value is None:
        return None
    try:
        return int(value)
    except ValueError:
        pass
    try:
        dt = datetime.datetime.strptime(value, '%Y-%m-%d-%H:%M:%S')
    except ValueError:
        raise argparse.ArgumentTypeError(
            "invalid time '%s' (use nanoseconds or YYYY-mm-dd-HH:MM:SS)" %
            value)
    return int(dt.timestamp()) * 1000000000


def encode_proto_desc(desc):
    if not desc:
        return ''
    return base64.b64encode(zlib.compress(desc, 9)).decode('ascii')


def _wanted(channel, topic, include, exclude):
    names = (channel, topic)
    if include and not any(n in include for n in names):
        return False
    return not any(n in exclude for n in names)


def convert(records, output, storage_id=None, begin=0, end=UINT64_MAX,
            include=(), exclude=(), keep_proto_desc=True,
            max_bagfile_size=0, verbose=True):
    """Converts `records` into the bag directory `output`.

    Returns {topic: message count}.
    """
    include = set(channel_to_topic(c) for c in include) | set(include)
    exclude = set(channel_to_topic(c) for c in exclude) | set(exclude)
    readers = []
    channels = {}  # topic -> (channel, type, proto_desc)
    for path in records:
        reader = record.RecordReader(path)
        if not reader.is_valid():
            raise IOError('cannot read record file: %s' % path)
        readers.append((path, reader))
        for channel in reader.get_channellist():
            topic = channel_to_topic(channel)
            if not _wanted(channel, topic, include, exclude):
                continue
            info = (channel, reader.get_messagetype(channel),
                    reader.get_protodesc(channel))
            known = channels.setdefault(topic, info)
            if known[0] != channel:
                sys.stderr.write(
                    'warning: channels %s and %s both map to topic %s\n' %
                    (known[0], channel, topic))

    storage_options = rosbag2_py.StorageOptions(
        uri=output, storage_id=storage_id or rosbag2_py.get_default_storage_id(),
        max_bagfile_size=max_bagfile_size)
    custom = {}
    for topic, (channel, msg_type, desc) in channels.items():
        custom[CUSTOM_CHANNEL + topic] = channel
        custom[CUSTOM_TYPE + topic] = msg_type
        if keep_proto_desc:
            custom[CUSTOM_PROTO_DESC + topic] = encode_proto_desc(desc)
    if hasattr(storage_options, 'custom_data'):
        storage_options.custom_data = custom
    converter_options = rosbag2_py.ConverterOptions(
        input_serialization_format='cdr', output_serialization_format='cdr')
    writer = rosbag2_py.SequentialWriter()
    writer.open(storage_options, converter_options)
    for topic_id, topic in enumerate(sorted(channels)):
        writer.create_topic(rosbag2_py.TopicMetadata(
            topic_id, topic, PROTO_MESSAGE_TYPE, 'cdr'))

    counts = dict.fromkeys(channels, 0)
    skipped = set()
    msg = ProtoMessage()
    for path, reader in readers:
        if verbose:
            print('converting %s' % path)
        for channel, data, data_type, timestamp in reader.read_messages(
                begin, end):
            topic = channel_to_topic(channel)
            if topic not in counts:
                if topic in skipped or \
                        not _wanted(channel, topic, include, exclude):
                    skipped.add(topic)
                    continue
                # a message whose channel has no channel section
                counts[topic] = 0
                channels[topic] = (channel, data_type, b'')
                writer.create_topic(rosbag2_py.TopicMetadata(
                    len(channels), topic, PROTO_MESSAGE_TYPE, 'cdr'))
            msg.type = data_type or channels[topic][1]
            msg.seq = counts[topic]
            msg.stamp_ns = timestamp
            # array('B') skips the generated setter's per-byte checks
            msg.data = array.array('B', data)
            writer.write(topic, serialize_message(msg), timestamp)
            counts[topic] += 1
        reader.close()
    del writer  # closes the bag
    if verbose:
        total = sum(counts.values())
        print('wrote %d messages on %d topics to %s' %
              (total, len(counts), output))
    return counts


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Convert Cyber .record files into a rosbag2 bag of '
                    'apollo_msgs/msg/ProtoMessage topics.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split('Examples:', 1)[1].join(['Examples:', '']))
    parser.add_argument('records', nargs='+', help='input .record file(s)')
    parser.add_argument('-o', '--output', required=True,
                        help='output bag directory (must not exist)')
    parser.add_argument('-s', '--storage', default=None,
                        help='rosbag2 storage plugin (default: %s; '
                             'available: %s)' % (
                                 rosbag2_py.get_default_storage_id(),
                                 ', '.join(sorted(
                                     rosbag2_py.get_registered_writers()))))
    parser.add_argument('-b', '--begin', type=parse_time, default=0,
                        help='skip messages before this time')
    parser.add_argument('-e', '--end', type=parse_time, default=UINT64_MAX,
                        help='skip messages after this time')
    parser.add_argument('-c', '--channel', action='append', default=[],
                        help='only convert this channel (repeatable)')
    parser.add_argument('-k', '--exclude', action='append', default=[],
                        help='do not convert this channel (repeatable)')
    parser.add_argument('--max-bagfile-size', type=int, default=0,
                        help='split the bag at this size in bytes')
    parser.add_argument('--no-proto-desc', action='store_true',
                        help='do not store proto descriptors in the bag '
                             'metadata')
    args = parser.parse_args(argv)
    if os.path.exists(args.output):
        parser.error('output %s already exists' % args.output)
    convert(args.records, args.output, args.storage, args.begin, args.end,
            args.channel, args.exclude, not args.no_proto_desc,
            args.max_bagfile_size)


if __name__ == '__main__':
    main()
