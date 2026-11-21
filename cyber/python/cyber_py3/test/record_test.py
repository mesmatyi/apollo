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
"""Tests of the pure-Python .record implementation.

data/cpp_writer.record was written by the original C++ cyber::record::
RecordWriter (cyber/record/*.cc built standalone) from golden_ops() below,
so the writer test checks the Python output is byte-identical to Cyber's.
"""

import os
import struct

import pytest

from cyber.python.cyber_py3 import _cyber_protos
from cyber.python.cyber_py3 import proto_registry
from cyber.python.cyber_py3 import record

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
GOLDEN = os.path.join(DATA_DIR, 'cpp_writer.record')

CHAN_1 = "channel/chatter"
MSG_TYPE = "apollo.common.util.test.SimpleMessage"
PROTO_DESC = b"1234567890"
MSG_DATA = b"0123456789"
TIME = 999
SEC = 1000000000
T0 = 1600000000 * SEC


def golden_ops():
    """The write sequence of data/cpp_writer.record."""
    ops = [('channel', '/apollo/a', 'apollo.test.A', b'\x0a\x0b\x0c'),
           ('channel', 'channel/chatter',
            'apollo.cyber.proto.SimpleMessage', b''),
           # a message written before its channel (restore_video_record.py)
           ('msg', '/apollo/late', 5, b'\xde\xad\xbe\xef'),
           ('channel', '/apollo/late', 'apollo.test.Late', b'\x01')]
    for i in range(200):
        t = T0 + i * 150 * 1000000   # 30 s in total: two 20 s chunks
        ops.append(('msg', '/apollo/a', t, bytes([i % 256]) * (i % 7 + 1)))
        if i % 3 == 0:
            ops.append(('msg', 'channel/chatter', t + 7, b''))
    ops.append(('msg', '/apollo/a', T0 - 5, b'\x00'))  # out of order
    return ops


def harness_script(path):
    """golden_ops() in the format of the C++ harness used to make GOLDEN."""
    lines = ['open %s' % path]
    for op in golden_ops():
        if op[0] == 'channel':
            lines.append('channel %s %s %s' % (op[1], op[2],
                                               op[3].hex() or '-'))
        else:
            lines.append('msg %s %d %s' % (op[1], op[2], op[3].hex() or '-'))
    lines.append('close')
    return '\n'.join(lines) + '\n'


def write_ops(path, ops):
    writer = record.RecordWriter()
    assert writer.open(path)
    for op in ops:
        if op[0] == 'channel':
            assert writer.write_channel(op[1], op[2], op[3])
        else:
            assert writer.write_message(op[1], op[3], op[2])
    writer.close()


def sections(path):
    with open(path, 'rb') as f:
        data = f.read()
    pos, result = record.SECTION_LENGTH + record.HEADER_LENGTH, []
    while pos < len(data):
        kind, size = struct.unpack('<i4xq', data[pos:pos + 16])
        result.append((kind, size))
        pos += 16 + size
    return result


def test_record_writer_read(tmp_path):
    """The original cyber_py3 record test."""
    path = str(tmp_path / 'test02.record')
    fwriter = record.RecordWriter()
    fwriter.set_size_fileseg(0)
    fwriter.set_intervaltime_fileseg(0)

    assert fwriter.open(path)
    fwriter.write_channel(CHAN_1, MSG_TYPE, PROTO_DESC)
    fwriter.write_message(CHAN_1, MSG_DATA, TIME)

    assert fwriter.get_messagenumber(CHAN_1) == 1
    assert fwriter.get_messagetype(CHAN_1) == MSG_TYPE
    assert fwriter.get_protodesc(CHAN_1) == PROTO_DESC
    fwriter.close()

    fread = record.RecordReader(path)
    channel_list = fread.get_channellist()
    assert channel_list == [CHAN_1]

    header = _cyber_protos.Header()
    header.ParseFromString(fread.get_headerstring())
    assert header.major_version == 1
    assert header.minor_version == 0
    assert header.chunk_number == 1
    assert header.channel_number == 1
    assert header.message_number == 1
    assert header.begin_time == TIME and header.end_time == TIME
    assert header.size == os.path.getsize(path)
    assert header.is_complete

    messages = list(fread.read_messages())
    assert len(messages) == 1
    channelname, msg, datatype, timestamp = messages[0]
    assert (channelname, msg, datatype, timestamp) == \
        (CHAN_1, MSG_DATA, MSG_TYPE, TIME)
    assert messages[0].topic == CHAN_1 and messages[0].message == MSG_DATA
    assert fread.get_messagenumber(channelname) == 1
    assert fread.get_messagetype(channelname) == MSG_TYPE
    assert fread.get_protodesc(channelname) == PROTO_DESC


def test_writer_matches_cpp_byte_for_byte(tmp_path):
    path = str(tmp_path / 'py.record')
    write_ops(path, golden_ops())
    with open(path, 'rb') as f, open(GOLDEN, 'rb') as g:
        assert f.read() == g.read()


def test_reader_on_cpp_file():
    reader = record.RecordReader(GOLDEN)
    assert reader.get_channellist() == ['/apollo/a', '/apollo/late',
                                        'channel/chatter']
    assert reader.get_messagenumber('/apollo/a') == 201
    assert reader.get_messagenumber('channel/chatter') == 67
    # counted by the file writer although written before the channel
    assert reader.get_messagenumber('/apollo/late') == 1
    assert reader.get_messagetype('/apollo/late') == 'apollo.test.Late'
    assert reader.get_protodesc('/apollo/a') == b'\x0a\x0b\x0c'
    assert reader.get_messagetype('nope') == ''
    assert reader.get_protodesc('nope') == b''
    assert reader.get_messagenumber('nope') == 0
    header = reader.get_header()
    # the t=5 message makes the 20 s chunk interval flush early: 3 chunks
    assert header.chunk_number == 3 and header.message_number == 269

    expected = [(op[1], op[3], op[2]) for op in golden_ops()
                if op[0] == 'msg']
    got = [(m.topic, m.message, m.timestamp) for m in reader.read_messages()]
    assert got == expected
    # Generator exhausted: like Cyber, read again only after reset().
    assert list(reader.read_messages()) == []
    reader.reset()
    assert len(list(reader.read_messages())) == len(expected)


@pytest.mark.parametrize('begin,end', [
    (0, 5), (T0 + 3 * SEC, T0 + 10 * SEC), (T0 + 25 * SEC, (1 << 64) - 1),
    (T0 + 29 * SEC, T0 + 29 * SEC + 1), (0, (1 << 64) - 1)])
def test_time_window(begin, end):
    """Same semantics as RecordReader::ReadMessage(begin, end): stops at the
    first message later than `end` in file order."""
    expected = []
    for op in golden_ops():
        if op[0] != 'msg':
            continue
        if op[2] > end:
            break
        if op[2] >= begin:
            expected.append(op[2])
    reader = record.RecordReader(GOLDEN)
    assert [m.timestamp for m in reader.read_messages(begin, end)] == expected


def test_chunk_raw_size_flush(tmp_path):
    path = str(tmp_path / 'big.record')
    writer = record.RecordWriter()
    assert writer.open(path)
    writer.write_channel('/big', 'apollo.test.Big', b'\xff')
    for i in range(5):
        assert writer.write_message('/big', bytes([i]) * (5 << 20), i + 1)
    writer.close()
    # 16 MB chunk_raw_size: chunk flushed after the 4th 5 MB message
    kinds = [kind for kind, _ in sections(path)]
    assert kinds == [record.SECTION_CHANNEL,
                     record.SECTION_CHUNK_HEADER, record.SECTION_CHUNK_BODY,
                     record.SECTION_CHUNK_HEADER, record.SECTION_CHUNK_BODY,
                     record.SECTION_INDEX]
    assert sections(path)[2][1] > 4 * (5 << 20)
    assert sections(path)[4][1] < 2 * (5 << 20)
    reader = record.RecordReader(path)
    assert [len(m.message) for m in reader.read_messages()] == [5 << 20] * 5


def test_empty_record(tmp_path):
    path = str(tmp_path / 'empty.record')
    writer = record.RecordWriter()
    assert writer.open(path)
    writer.close()
    assert os.path.getsize(path) == 2080  # header + empty index
    reader = record.RecordReader(path)
    assert reader.get_channellist() == []
    assert list(reader.read_messages()) == []


def test_segmentation(tmp_path):
    base = str(tmp_path / 'seg.record')
    writer = record.RecordWriter(0, 5)   # new file every 5 s
    assert writer.open(base)
    writer.write_channel('/a', 'apollo.test.A', b'desc')
    for i in range(30):
        writer.write_message('/a', b'x', T0 + i * SEC)
    writer.close()
    files = sorted(os.listdir(str(tmp_path)))
    assert len(files) == 5
    assert all(f.startswith('seg.record.0000') for f in files)
    total = 0
    for f in files:
        reader = record.RecordReader(str(tmp_path / f))
        assert reader.get_messagetype('/a') == 'apollo.test.A'
        assert reader.get_protodesc('/a') == b'desc'
        total += len(list(reader.read_messages()))
    assert total == 30


def test_setters_refused_once_open(tmp_path):
    writer = record.RecordWriter()
    assert writer.open(str(tmp_path / 'x.record'))
    assert not writer.set_size_fileseg(10)
    assert not writer.set_intervaltime_fileseg(10)
    writer.close()


def test_duplicate_channel_is_ignored(tmp_path):
    path = str(tmp_path / 'dup.record')
    writer = record.RecordWriter()
    assert writer.open(path)
    assert writer.write_channel('/a', 'T1', b'1')
    assert writer.write_channel('/a', 'T2', b'2')
    writer.close()
    reader = record.RecordReader(path)
    assert reader.get_messagetype('/a') == 'T1'
    assert reader.get_header().channel_number == 1


def test_incomplete_record_is_readable(tmp_path):
    """A record whose writer died (no index, header not final)."""
    path = str(tmp_path / 'broken.record')
    write_ops(path, golden_ops())
    index_pos = record.RecordReader(path).get_header().index_position
    with open(path, 'rb') as f:
        data = f.read()
    # Restore the header written at open() and drop the index.
    header = record.default_header(0, 0)
    body = header.SerializeToString()
    head = struct.pack('<i4xq', record.SECTION_HEADER, len(body)) + body
    head += b'0' + b'\0' * (record.HEADER_LENGTH - len(body) - 1)
    with open(path, 'wb') as f:
        f.write(head + data[len(head):index_pos])
    reader = record.RecordReader(path)
    assert not reader.get_header().is_complete
    # Channels are recovered from the channel sections (the C++ reader
    # would report none).
    assert reader.get_channellist() == ['/apollo/a', '/apollo/late',
                                        'channel/chatter']
    assert reader.get_messagenumber('/apollo/a') == 201
    assert len(list(reader.read_messages())) == 269


def test_missing_file():
    reader = record.RecordReader('/nonexistent/file.record')
    assert not reader.is_valid()
    assert list(reader.read_messages()) == []
    assert reader.get_channellist() == []


def test_not_a_record(tmp_path):
    path = tmp_path / 'junk.record'
    path.write_bytes(b'\x07' * 100)
    reader = record.RecordReader(str(path))
    assert not reader.is_valid()
    assert list(reader.read_messages()) == []


def test_write_protobuf_registers_channel(tmp_path):
    path = str(tmp_path / 'pb.record')
    msg = _cyber_protos.Channel(name='n', message_type='t', proto_desc=b'd')
    writer = record.RecordWriter()
    assert writer.open(path)
    assert writer.write_message('/pb', msg, 10, False)
    assert writer.write_message('/pb', msg.SerializeToString(), 11)
    writer.close()
    reader = record.RecordReader(path)
    assert reader.get_messagetype('/pb') == 'apollo.cyber.proto.Channel'
    assert reader.get_messagenumber('/pb') == 2
    desc = reader.get_protodesc('/pb')
    assert desc == proto_registry.get_proto_desc(msg)
    # The stored descriptor is enough to decode the messages.
    messages = list(reader.read_messages())
    decoded = proto_registry.parse_message(messages[0].data_type,
                                           messages[0].message, desc)
    assert decoded.name == 'n' and decoded.proto_desc == b'd'


def test_context_managers(tmp_path):
    path = str(tmp_path / 'ctx.record')
    with record.RecordWriter() as writer:
        assert writer.open(path)
        writer.write_channel('/a', 'T', b'')
        writer.write_message('/a', b'1', 1)
    with record.RecordReader(path) as reader:
        assert [m.message for m in reader.read_messages()] == [b'1']
