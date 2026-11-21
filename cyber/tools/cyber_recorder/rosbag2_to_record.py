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
"""Convert a rosbag2 bag of Apollo topics into a Cyber .record file.

Only topics of type apollo_msgs/msg/ProtoMessage are converted (others are
skipped). The channel name, protobuf type and proto descriptor of each
channel come from the bag metadata written by record_to_rosbag2.py when
present; otherwise the channel is the topic name, the type is the `type`
field of the samples and the descriptor is built from the installed python
protobuf modules (empty when the type is unknown). Record times are the bag
timestamps.

Examples:
  rosbag2_to_record.py demo_bag -o demo.record
  rosbag2_to_record.py demo_bag -o pose.record -c /apollo/localization/pose
"""

import argparse
import base64
import os
import sys
import zlib

import rosbag2_py
from rclpy.serialization import deserialize_message

from apollo_msgs.msg import ProtoMessage

from cyber.python.cyber_py3 import proto_registry
from cyber.python.cyber_py3 import record
from cyber.python.cyber_py3.naming import channel_to_topic

PROTO_MESSAGE_TYPE = 'apollo_msgs/msg/ProtoMessage'
CUSTOM_CHANNEL = 'apollo.channel.'
CUSTOM_TYPE = 'apollo.type.'
CUSTOM_PROTO_DESC = 'apollo.proto_desc.'
UINT64_MAX = (1 << 64) - 1


def _decode_proto_desc(text):
    # base64(zlib(desc)), '' for an empty descriptor (record_to_rosbag2.py)
    if not text:
        return b''
    return zlib.decompress(base64.b64decode(text))


def _lookup_proto_desc(msg_type):
    if not msg_type or proto_registry.get_message_class(msg_type) is None:
        return b''
    return proto_registry.get_proto_desc(msg_type)


def convert(bag, output, storage_id='', begin=0, end=UINT64_MAX,
            include=(), exclude=(), segment_size_kb=0, segment_interval_s=0,
            verbose=True):
    """Converts the bag `bag` into the record `output`.

    Returns {channel: message count}.
    """
    include = set(channel_to_topic(c) for c in include)
    exclude = set(channel_to_topic(c) for c in exclude)
    reader = rosbag2_py.SequentialReader()
    reader.open(rosbag2_py.StorageOptions(uri=bag, storage_id=storage_id),
                rosbag2_py.ConverterOptions(
                    input_serialization_format='cdr',
                    output_serialization_format='cdr'))
    custom = dict(getattr(reader.get_metadata(), 'custom_data', {}) or {})
    topic_types = {t.name: t.type for t in reader.get_all_topics_and_types()}
    topics = sorted(t for t, ty in topic_types.items()
                    if ty == PROTO_MESSAGE_TYPE and
                    (not include or t in include) and t not in exclude)
    skipped = sorted(t for t, ty in topic_types.items()
                     if ty != PROTO_MESSAGE_TYPE)
    if skipped and verbose:
        sys.stderr.write('skipping non-Apollo topics: %s\n' %
                         ', '.join(skipped))
    if not topics:
        sys.stderr.write('no apollo_msgs/msg/ProtoMessage topic to convert\n')
    else:
        reader.set_filter(rosbag2_py.StorageFilter(topics=topics))

    writer = record.RecordWriter(segment_size_kb, segment_interval_s)
    if not writer.open(output):
        raise IOError('cannot open %s for writing' % output)
    channel_of_topic = {}
    counts = {}
    while topics and reader.has_next():
        topic, data, timestamp = reader.read_next()
        if timestamp < begin or timestamp > end:
            continue
        msg = deserialize_message(data, ProtoMessage)
        channel = channel_of_topic.get(topic)
        if channel is None:
            channel = custom.get(CUSTOM_CHANNEL + topic, topic)
            msg_type = custom.get(CUSTOM_TYPE + topic) or msg.type
            if CUSTOM_PROTO_DESC + topic in custom:
                desc = _decode_proto_desc(custom[CUSTOM_PROTO_DESC + topic])
            else:
                desc = _lookup_proto_desc(msg_type)
            writer.write_channel(channel, msg_type, desc)
            channel_of_topic[topic] = channel
            counts[channel] = 0
        writer.write_message(channel, msg.data.tobytes(), timestamp)
        counts[channel] += 1
    writer.close()
    if verbose:
        print('wrote %d messages on %d channels to %s' %
              (sum(counts.values()), len(counts), output))
    return counts


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Convert a rosbag2 bag of apollo_msgs/msg/ProtoMessage '
                    'topics into a Cyber .record file.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split('Examples:', 1)[1].join(['Examples:', '']))
    parser.add_argument('bag', help='input bag directory (or bag file)')
    parser.add_argument('-o', '--output', required=True,
                        help='output .record file')
    parser.add_argument('-s', '--storage', default='',
                        help='rosbag2 storage plugin (default: detect)')
    parser.add_argument('-b', '--begin', type=int, default=0,
                        help='skip messages before this time (ns)')
    parser.add_argument('-e', '--end', type=int, default=UINT64_MAX,
                        help='skip messages after this time (ns)')
    parser.add_argument('-c', '--channel', action='append', default=[],
                        help='only convert this channel/topic (repeatable)')
    parser.add_argument('-k', '--exclude', action='append', default=[],
                        help='do not convert this channel/topic (repeatable)')
    parser.add_argument('--segment-size-kb', type=int, default=0,
                        help='split the record every N KB (0: never)')
    parser.add_argument('--segment-interval', type=int, default=0,
                        help='split the record every N seconds (0: never)')
    args = parser.parse_args(argv)
    if not os.path.exists(args.bag):
        parser.error('bag %s does not exist' % args.bag)
    convert(args.bag, args.output, args.storage, args.begin, args.end,
            args.channel, args.exclude, args.segment_size_kb,
            args.segment_interval)


if __name__ == '__main__':
    main()
