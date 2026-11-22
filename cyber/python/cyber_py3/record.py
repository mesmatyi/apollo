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
"""Cyber .record files, implemented in pure Python.

This is a re-implementation of cyber/record/{file/record_file_reader,
file/record_file_writer,record_reader,record_writer}.cc and produces the same
bytes as the C++ writer. It needs only the protobuf runtime (no ROS 2, no C++
extension).

On-disk format (little endian)::

    Section{int32 type; 4 bytes zero padding; int64 size}   16 bytes
    Header (cyber.proto.Header), padded to 2048 bytes        (rewritten on close)
    [Section Channel | Section ChunkHeader + Section ChunkBody]*
    Section Index                                            (on close)

The 2048-byte header area is filled with b'0' followed by NUL bytes, exactly
like the C++ writer. Chunks are never compressed by Cyber (Header.compress is
always COMPRESS_NONE); the reader nevertheless inflates BZ2 bodies (and LZ4
bodies when the `lz4` package is installed) should a file declare them.
"""

import collections
import os
import struct
import sys
import threading
import time as _time

from google.protobuf.message import DecodeError

from . import _cyber_protos as _pb

PyBagMessage = collections.namedtuple('PyBagMessage',
                                      'topic message data_type timestamp')

HEADER_LENGTH = 2048
_SECTION = struct.Struct('<i4xq')  # struct Section {SectionType; int64_t}
SECTION_LENGTH = _SECTION.size     # 16
_HEADER_PADDING = b'0' + b'\0' * (HEADER_LENGTH - 1)  # char blank[] = {'0'}
_UINT64_MAX = (1 << 64) - 1

SECTION_HEADER = _pb.SectionType['SECTION_HEADER']
SECTION_CHUNK_HEADER = _pb.SectionType['SECTION_CHUNK_HEADER']
SECTION_CHUNK_BODY = _pb.SectionType['SECTION_CHUNK_BODY']
SECTION_INDEX = _pb.SectionType['SECTION_INDEX']
SECTION_CHANNEL = _pb.SectionType['SECTION_CHANNEL']

_SECTION_OF_TYPE = {
    _pb.Header: SECTION_HEADER,
    _pb.ChunkHeader: SECTION_CHUNK_HEADER,
    _pb.ChunkBody: SECTION_CHUNK_BODY,
    _pb.Index: SECTION_INDEX,
    _pb.Channel: SECTION_CHANNEL,
}

# HeaderBuilder constants (cyber/record/header_builder.h)
MAJOR_VERSION = 1
MINOR_VERSION = 0
CHUNK_INTERVAL = 20 * 1000 * 1000 * 1000      # 20 s
SEGMENT_INTERVAL = 60 * 1000 * 1000 * 1000    # 60 s
CHUNK_RAW_SIZE = 16 * 1024 * 1024             # 16 MB
SEGMENT_RAW_SIZE = 2048 * 1024 * 1024         # 2 GB


def _log(msg):
    sys.stderr.write('[cyber_py3.record] %s\n' % msg)


def _to_bytes(data):
    if isinstance(data, bytes):
        return data
    if isinstance(data, (bytearray, memoryview)):
        return bytes(data)
    if isinstance(data, str):
        return data.encode('utf-8')
    raise TypeError('expected bytes, got %s' % type(data).__name__)


def _to_str(data):
    if isinstance(data, (bytes, bytearray)):
        return bytes(data).decode('utf-8')
    return data


def default_header(segment_interval=SEGMENT_INTERVAL,
                   segment_raw_size=SEGMENT_RAW_SIZE,
                   chunk_interval=CHUNK_INTERVAL,
                   chunk_raw_size=CHUNK_RAW_SIZE):
    """Returns HeaderBuilder::GetHeader() (with optional overrides)."""
    header = _pb.Header()
    header.major_version = MAJOR_VERSION
    header.minor_version = MINOR_VERSION
    header.compress = _pb.CompressType['COMPRESS_NONE']
    header.chunk_interval = chunk_interval
    header.segment_interval = segment_interval
    header.index_position = 0
    header.chunk_number = 0
    header.channel_number = 0
    header.begin_time = 0
    header.end_time = 0
    header.message_number = 0
    header.size = 0
    header.is_complete = False
    header.chunk_raw_size = chunk_raw_size
    header.segment_raw_size = segment_raw_size
    return header


def _decompress(compress, data):
    if compress == _pb.CompressType['COMPRESS_BZ2']:
        import bz2
        return bz2.decompress(data)
    if compress == _pb.CompressType['COMPRESS_LZ4']:
        try:
            import lz4.frame
        except ImportError:
            raise IOError('LZ4 compressed record needs the "lz4" package')
        return lz4.frame.decompress(data)
    return data


# ---------------------------------------------------------------------------
# File level (record_file_reader.cc / record_file_writer.cc)
# ---------------------------------------------------------------------------


class RecordFileReader(object):
    """Section level reader of one .record file."""

    def __init__(self):
        self.path = ''
        self.header = _pb.Header()
        self.index = _pb.Index()
        self._file = None
        self.end_of_file = False

    def open(self, path):
        self.path = path
        if not os.path.exists(path):
            _log('File not exist, file: %s' % path)
            return False
        try:
            self._file = open(path, 'rb')
        except OSError as e:
            _log('Open file failed, file: %s, %s' % (path, e))
            return False
        self.end_of_file = False
        if not self._read_header():
            _log('Read header section fail, file: %s' % path)
            return False
        return True

    def close(self):
        if self._file is not None:
            self._file.close()
            self._file = None

    def reset(self):
        self._file.seek(SECTION_LENGTH + HEADER_LENGTH)
        self.end_of_file = False
        return True

    def position(self):
        return self._file.tell()

    def _read_header(self):
        section = self.read_section_header()
        if section is None:
            _log('Read header section fail, file is broken or it is not a '
                 'record file.')
            return False
        if section[0] != SECTION_HEADER:
            _log('Check section type failed, expect: %d, actual: %d' %
                 (SECTION_HEADER, section[0]))
            return False
        header = self.read_section(section[1], _pb.Header)
        if header is None:
            return False
        self.header = header
        self._file.seek(SECTION_LENGTH + HEADER_LENGTH)
        return True

    def read_index(self):
        if not self.header.is_complete:
            _log('Record file is not complete.')
            return False
        self._file.seek(self.header.index_position)
        section = self.read_section_header()
        if section is None or section[0] != SECTION_INDEX:
            _log('Read index section fail, maybe file is broken.')
            return False
        index = self.read_section(section[1], _pb.Index)
        if index is None:
            return False
        self.index = index
        self.reset()
        return True

    def read_section_header(self):
        """Returns (type, size) or None at end of file / on error."""
        data = self._file.read(SECTION_LENGTH)
        if not data:
            self.end_of_file = True
            return None
        if len(data) != SECTION_LENGTH:
            _log('Read fd failed, expect count: %d, actual count: %d' %
                 (SECTION_LENGTH, len(data)))
            return None
        return _SECTION.unpack(data)

    def read_raw(self, size):
        if size < 0:
            return None
        data = self._file.read(size)
        if len(data) != size:
            _log('Truncated section, expect: %d, actual: %d' %
                 (size, len(data)))
            self.end_of_file = True
            return None
        return data

    def read_section(self, size, message_type, compress=0):
        data = self.read_raw(size)
        if data is None:
            return None
        message = message_type()
        try:
            message.ParseFromString(_decompress(compress, data))
        except (DecodeError, IOError, OSError) as e:
            _log('Parse section message failed: %s' % e)
            return None
        return message

    def skip_section(self, size):
        self._file.seek(size, os.SEEK_CUR)
        return True


class _Chunk(object):

    def __init__(self):
        self.clear()

    def clear(self):
        self.body = _pb.ChunkBody()
        self.header = _pb.ChunkHeader()
        self.header.begin_time = 0
        self.header.end_time = 0
        self.header.message_number = 0
        self.header.raw_size = 0

    def add(self, message):
        self.body.messages.add().CopyFrom(message)
        header = self.header
        if header.begin_time == 0:
            header.begin_time = message.time
        if header.begin_time > message.time:
            header.begin_time = message.time
        if header.end_time < message.time:
            header.end_time = message.time
        header.message_number += 1
        header.raw_size += len(message.content)

    def empty(self):
        return self.header.message_number == 0


class RecordFileWriter(object):
    """Section level writer of one .record file.

    Unlike the C++ writer, chunks are flushed synchronously (no flush thread),
    so the section order in the file is deterministic.
    """

    def __init__(self):
        self.path = ''
        self.header = _pb.Header()
        self.index = _pb.Index()
        self._file = None
        self._chunk = None
        self._is_writing = False
        self._channel_message_number = collections.OrderedDict()

    def open(self, path):
        self.path = path
        if os.path.exists(path):
            _log('File exist and overwrite, file: %s' % path)
        try:
            self._file = open(path, 'wb')
        except OSError as e:
            _log('Open file failed, file: %s, %s' % (path, e))
            return False
        self._chunk = _Chunk()
        self._is_writing = True
        return True

    def close(self):
        if not self._is_writing:
            return
        if not self._chunk.empty():
            self._write_chunk(self._chunk.header, self._chunk.body)
            self._chunk.clear()
        self._is_writing = False
        if not self._write_index():
            _log('Write index section failed, file: %s' % self.path)
        self.header.is_complete = True
        if not self.write_header(self.header):
            _log('Overwrite header section failed, file: %s' % self.path)
        self._file.close()
        self._file = None

    def _write_section(self, message):
        section_type = _SECTION_OF_TYPE[type(message)]
        if section_type == SECTION_HEADER:
            self._file.seek(0)
        data = message.SerializeToString()
        self._file.write(_SECTION.pack(section_type, len(data)))
        self._file.write(data)
        if section_type == SECTION_HEADER:
            if len(data) > HEADER_LENGTH:
                _log('Header section is larger than %d bytes' % HEADER_LENGTH)
                return False
            self._file.write(_HEADER_PADDING[:HEADER_LENGTH - len(data)])
        self.header.size = self._file.tell()
        return True

    def write_header(self, header):
        if header is not self.header:
            self.header.CopyFrom(header)
        return self._write_section(self.header)

    def _write_index(self):
        for single_index in self.index.indexes:
            if single_index.type == SECTION_CHANNEL:
                cache = single_index.channel_cache
                if cache.name in self._channel_message_number:
                    cache.message_number = \
                        self._channel_message_number[cache.name]
        self.header.index_position = self._file.tell()
        return self._write_section(self.index)

    def write_channel(self, channel):
        pos = self._file.tell()
        if not self._write_section(channel):
            return False
        self.header.channel_number += 1
        single_index = self.index.indexes.add()
        single_index.type = SECTION_CHANNEL
        single_index.position = pos
        cache = single_index.channel_cache
        cache.name = channel.name
        cache.message_number = 0
        cache.message_type = channel.message_type
        cache.proto_desc = channel.proto_desc
        return True

    def _write_chunk(self, chunk_header, chunk_body):
        pos = self._file.tell()
        if not self._write_section(chunk_header):
            _log('Write chunk header fail')
            return False
        single_index = self.index.indexes.add()
        single_index.type = SECTION_CHUNK_HEADER
        single_index.position = pos
        cache = single_index.chunk_header_cache
        cache.begin_time = chunk_header.begin_time
        cache.end_time = chunk_header.end_time
        cache.message_number = chunk_header.message_number
        cache.raw_size = chunk_header.raw_size

        pos = self._file.tell()
        if not self._write_section(chunk_body):
            _log('Write chunk body fail')
            return False
        header = self.header
        header.chunk_number += 1
        if header.begin_time == 0:
            header.begin_time = chunk_header.begin_time
        header.end_time = chunk_header.end_time
        header.message_number += chunk_header.message_number

        single_index = self.index.indexes.add()
        single_index.type = SECTION_CHUNK_BODY
        single_index.position = pos
        single_index.chunk_body_cache.message_number = len(chunk_body.messages)
        return True

    def write_message(self, message):
        """Appends a cyber.proto.SingleMessage to the active chunk."""
        chunk = self._chunk
        chunk.add(message)
        name = message.channel_name
        self._channel_message_number[name] = \
            self._channel_message_number.get(name, 0) + 1
        need_flush = False
        if (self.header.chunk_interval > 0 and
                ((message.time - chunk.header.begin_time) & _UINT64_MAX) >
                self.header.chunk_interval):
            need_flush = True
        if (self.header.chunk_raw_size > 0 and
                chunk.header.raw_size > self.header.chunk_raw_size):
            need_flush = True
        if need_flush:
            ok = self._write_chunk(chunk.header, chunk.body)
            chunk.clear()
            return ok
        return True

    def get_message_number(self, channel_name):
        return self._channel_message_number.get(channel_name, 0)


# ---------------------------------------------------------------------------
# Record level (record_reader.cc / record_writer.cc) and the cyber_py3 API
# ---------------------------------------------------------------------------


class RecordReader(object):

    """
    Class for cyber RecordReader wrapper.
    """

    ##
    # @brief the constructor function.
    #
    # @param file_name the record file name.
    def __init__(self, file_name):
        self._lock = threading.Lock()
        self._file_reader = RecordFileReader()
        self._header = _pb.Header()
        self._index = _pb.Index()
        self._channel_info = collections.OrderedDict()
        self._chunk = _pb.ChunkBody()
        self._message_index = 0
        self._reach_end = False
        self._is_valid = False
        if not self._file_reader.open(file_name):
            _log('Failed to open record file: %s' % file_name)
            return
        self._is_valid = True
        self._header = self._file_reader.header
        if self._file_reader.read_index():
            self._index = self._file_reader.index
            for single_index in self._index.indexes:
                if single_index.type != SECTION_CHANNEL:
                    continue
                if not single_index.HasField('channel_cache'):
                    _log('Single channel index does not have channel_cache.')
                    continue
                cache = single_index.channel_cache
                self._channel_info[cache.name] = cache
        else:
            # Incomplete record (writer crashed): the C++ reader knows no
            # channels at all. Scan the channel sections instead so that
            # message types/descriptors are still available.
            self._scan_channels()
        self._file_reader.reset()

    def __del__(self):
        reader = getattr(self, '_file_reader', None)
        if reader is not None:
            reader.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        """Closes the underlying file."""
        self._file_reader.close()
        self._is_valid = False

    def is_valid(self):
        """True when the file could be opened as a record."""
        return self._is_valid

    def _scan_channels(self):
        reader = self._file_reader
        reader.reset()
        counts = collections.Counter()
        while True:
            section = reader.read_section_header()
            if section is None:
                break
            section_type, size = section
            if section_type == SECTION_CHANNEL:
                channel = reader.read_section(size, _pb.Channel)
                if channel is None:
                    break
                cache = _pb.ChannelCache()
                cache.name = channel.name
                cache.message_number = 0
                cache.message_type = channel.message_type
                cache.proto_desc = channel.proto_desc
                self._channel_info.setdefault(channel.name, cache)
            elif section_type == SECTION_CHUNK_BODY:
                body = reader.read_section(size, _pb.ChunkBody,
                                           self._header.compress)
                if body is None:
                    break
                for message in body.messages:
                    counts[message.channel_name] += 1
            elif section_type in (SECTION_CHUNK_HEADER, SECTION_INDEX):
                reader.skip_section(size)
            else:
                break
        for name, cache in self._channel_info.items():
            cache.message_number = counts.get(name, 0)

    def _read_next_chunk(self, begin_time, end_time):
        reader = self._file_reader
        skip_next_chunk_body = False
        while not self._reach_end:
            section = reader.read_section_header()
            if section is None:
                if not reader.end_of_file:
                    _log('Failed to read section, file: %s' % reader.path)
                return False
            section_type, size = section
            if section_type == SECTION_INDEX:
                reader.skip_section(size)
                self._reach_end = True
            elif section_type == SECTION_CHANNEL:
                if reader.read_section(size, _pb.Channel) is None:
                    _log('Failed to read channel section.')
                    return False
            elif section_type == SECTION_CHUNK_HEADER:
                header = reader.read_section(size, _pb.ChunkHeader)
                if header is None:
                    _log('Failed to read chunk header section.')
                    return False
                if header.end_time < begin_time:
                    skip_next_chunk_body = True
                if header.begin_time > end_time:
                    return False
            elif section_type == SECTION_CHUNK_BODY:
                if skip_next_chunk_body:
                    reader.skip_section(size)
                    skip_next_chunk_body = False
                    continue
                chunk = reader.read_section(size, _pb.ChunkBody,
                                            self._header.compress)
                if chunk is None:
                    _log('Failed to read chunk body section.')
                    return False
                self._chunk = chunk
                return True
            else:
                _log('Invalid section, type: %d, size: %d' %
                     (section_type, size))
                return False
        return False

    def _read_message(self, begin_time, end_time):
        """RecordReader::ReadMessage(): returns SingleMessage or None."""
        if not self._is_valid:
            return None
        if begin_time > self._header.end_time or \
                end_time < self._header.begin_time:
            return None
        while True:
            messages = self._chunk.messages
            while self._message_index < len(messages):
                message = messages[self._message_index]
                if message.time > end_time:
                    return None
                self._message_index += 1
                if message.time < begin_time:
                    continue
                return message
            if not self._read_next_chunk(begin_time, end_time):
                return None
            self._message_index = 0

    ##
    # @brief Read message from bag file.
    #
    # @param start_time the start time to read.
    # @param end_time the end time to read.
    #
    # @return return (channnel, data, data_type, timestamp)
    def read_messages(self, start_time=0, end_time=18446744073709551615):
        while True:
            with self._lock:
                message = self._read_message(start_time, end_time)
            if message is None:
                break
            channel_name = message.channel_name
            yield PyBagMessage(channel_name, message.content,
                               self.get_messagetype(channel_name),
                               message.time)

    ##
    # @brief Return message count of the channel in current record file.
    #
    # @param channel_name the channel name.
    #
    # @return return the message count.
    def get_messagenumber(self, channel_name):
        cache = self._channel_info.get(_to_str(channel_name))
        return cache.message_number if cache is not None else 0

    ##
    # @brief Get the corresponding message type of channel.
    #
    # @param channel_name channel name.
    #
    # @return return the name of ther string type.
    def get_messagetype(self, channel_name):
        cache = self._channel_info.get(_to_str(channel_name))
        return cache.message_type if cache is not None else ''

    def get_protodesc(self, channel_name):
        """
        Return message protodesc.
        """
        cache = self._channel_info.get(_to_str(channel_name))
        return cache.proto_desc if cache is not None else b''

    def get_headerstring(self):
        """
        Return message header string.
        """
        return self._header.SerializeToString()

    def get_header(self):
        """Returns a copy of the record header (cyber.proto.Header)."""
        header = _pb.Header()
        header.CopyFrom(self._header)
        return header

    def reset(self):
        """
        Return reset.
        """
        with self._lock:
            if self._is_valid:
                self._file_reader.reset()
            self._reach_end = False
            self._message_index = 0
            self._chunk = _pb.ChunkBody()

    def get_channellist(self):
        """
        Return current channel names list.
        """
        return sorted(self._channel_info.keys())


class RecordWriter(object):

    """
    Class for cyber RecordWriter wrapper.
    """

    ##
    # @brief the constructor function.
    #
    # @param file_segmentation_size_kb size to segment the file, 0 is no segmentation.
    # @param file_segmentation_interval_sec size to segment the file, 0 is no segmentation.
    def __init__(self, file_segmentation_size_kb=0,
                 file_segmentation_interval_sec=0):
        self._lock = threading.RLock()
        self._header = default_header()
        self._file = ''
        self._path = ''
        self._file_index = 0
        self._is_opened = False
        self._segment_raw_size = 0
        self._segment_begin_time = 0
        self._channel_message_number = collections.OrderedDict()
        self._channel_message_type = {}
        self._channel_proto_desc = {}
        self._file_writer = None
        self.set_size_fileseg(file_segmentation_size_kb)
        self.set_intervaltime_fileseg(file_segmentation_interval_sec)

    def __del__(self):
        try:
            self.close()
        except Exception:  # pylint: disable=broad-except
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _segment_path(self):
        suffix = '.%05d.' % self._file_index
        self._file_index += 1
        return self._file + suffix + _time.strftime('%Y%m%d%H%M%S',
                                                    _time.localtime())

    ##
    # @brief Open record file for write.
    #
    # @param path the file path.
    #
    # @return Success is True, other False.
    def open(self, path):
        with self._lock:
            self._file = path
            self._file_index = 0
            if self._header.segment_interval > 0 or \
                    self._header.segment_raw_size > 0:
                self._path = self._segment_path()
            else:
                self._path = path
            self._file_writer = RecordFileWriter()
            if not self._file_writer.open(self._path):
                _log('Failed to open output record file: %s' % self._path)
                return False
            if not self._file_writer.write_header(self._header):
                _log('Failed to write header: %s' % self._path)
                self._file_writer.close()
                return False
            self._is_opened = True
            return True

    ##
    # @brief Close record file.
    def close(self):
        """
        Close record file.
        """
        with self._lock:
            if self._is_opened:
                self._file_writer.close()
                self._is_opened = False

    def get_path(self):
        """Path of the file currently written (differs when segmenting)."""
        return self._path

    def _split_outfile(self):
        self._file_writer = RecordFileWriter()
        if self._file_index > 99999:
            _log('More than 99999 record files had been recored, will '
                 'restart counting from 0.')
            self._file_index = 0
        self._path = self._segment_path()
        self._segment_raw_size = 0
        self._segment_begin_time = 0
        if not self._file_writer.open(self._path):
            _log('Failed to open record file: %s' % self._path)
            return False
        if not self._file_writer.write_header(self._header):
            _log('Failed to write header for record file: %s' % self._path)
            return False
        for name in self._channel_message_number:
            channel = _pb.Channel()
            channel.name = name
            channel.message_type = self._channel_message_type[name]
            channel.proto_desc = self._channel_proto_desc[name]
            if not self._file_writer.write_channel(channel):
                _log('Failed to write channel for record file: %s' %
                     self._path)
                return False
        return True

    ##
    # @brief Writer channel by channelname, typename, protodesc.
    #
    # @param channel_name the channel name to write
    # @param type_name a string of message type name.
    # @param proto_desc the message descriptor.
    #
    # @return Success is True, other False.
    def write_channel(self, channel_name, type_name, proto_desc):
        """
        Writer channel by channelname,typename,protodesc
        """
        channel_name = _to_str(channel_name)
        type_name = _to_str(type_name)
        proto_desc = _to_bytes(proto_desc)
        with self._lock:
            if not self._is_opened:
                _log('Record file is not opened.')
                return False
            if channel_name not in self._channel_message_number:
                self._channel_message_number[channel_name] = 0
                self._channel_message_type[channel_name] = type_name
                self._channel_proto_desc[channel_name] = proto_desc
                channel = _pb.Channel()
                channel.name = channel_name
                channel.message_type = type_name
                channel.proto_desc = proto_desc
                if not self._file_writer.write_channel(channel):
                    _log('Failed to write channel: %s' % channel_name)
                    return False
            else:
                _log('Intercept write channel request, duplicate channel: %s'
                     % channel_name)
            return True

    ##
    # @brief Writer msg: channelname, data, writer time.
    #
    # @param channel_name channel name to write.
    # @param data when raw is True, data processed as a rawdata, other it needs to SerializeToString
    # @param time message time.
    # @param raw the flag implies data whether or not a rawdata.
    #
    # @return Success is True, other False.
    def write_message(self, channel_name, data, time, raw=True):
        """
        Writer msg:channelname,rawmsg,writer time
        """
        channel_name = _to_str(channel_name)
        if raw:
            content = _to_bytes(data)
        else:
            # Like the templated C++ RecordWriter::WriteMessage<MessageT>():
            # a new channel is registered with the message's descriptor.
            with self._lock:
                if self._is_opened and \
                        not self.get_messagetype(channel_name):
                    from . import proto_registry
                    self.write_channel(channel_name,
                                       data.DESCRIPTOR.full_name,
                                       proto_registry.get_proto_desc(data))
            content = data.SerializeToString()
        message = _pb.SingleMessage()
        message.channel_name = channel_name
        message.content = content
        message.time = int(time)
        return self._write_single_message(message)

    def _write_single_message(self, message):
        with self._lock:
            if not self._is_opened:
                _log('Record file is not opened.')
                return False
            name = message.channel_name
            if name in self._channel_message_number:
                self._channel_message_number[name] += 1
            if not self._file_writer.write_message(message):
                _log('Write message is failed.')
                return False
            self._segment_raw_size += len(message.content)
            if self._segment_begin_time == 0:
                self._segment_begin_time = message.time
            if self._segment_begin_time > message.time:
                self._segment_begin_time = message.time
            header = self._header
            if ((header.segment_interval > 0 and
                 message.time - self._segment_begin_time >
                 header.segment_interval) or
                    (header.segment_raw_size > 0 and
                     self._segment_raw_size > header.segment_raw_size)):
                self._file_writer.close()
                if not self._split_outfile():
                    _log('Split out file is failed.')
                    return False
            return True

    def set_size_fileseg(self, size_kilobytes):
        """
        Return filesegment size.
        """
        if self._is_opened:
            _log('Please call this interface before opening file.')
            return False
        self._header.segment_raw_size = int(size_kilobytes) * 1024
        return True

    def set_intervaltime_fileseg(self, time_sec):
        """
        Return file interval time.
        """
        if self._is_opened:
            _log('Please call this interface before opening file.')
            return False
        self._header.segment_interval = int(time_sec) * 1000000000
        return True

    def get_messagenumber(self, channel_name):
        """
        Return message count.
        """
        return self._channel_message_number.get(_to_str(channel_name), 0)

    def get_messagetype(self, channel_name):
        """
        Return message type.
        """
        return self._channel_message_type.get(_to_str(channel_name), '')

    def get_protodesc(self, channel_name):
        """
        Return message protodesc.
        """
        return self._channel_proto_desc.get(_to_str(channel_name), b'')

    def get_channellist(self):
        """Return the channels written so far (sorted)."""
        return sorted(self._channel_message_number.keys())
