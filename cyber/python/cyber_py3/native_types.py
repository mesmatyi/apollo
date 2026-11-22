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
"""Native ROS 2 interfaces for Apollo protobuf messages (Python side).

``tools/ros2/proto2ros.py`` generates one ROS 2 message type per Apollo
protobuf message (package ``apollo_msgs``) and a registry that describes the
mapping. This module reads the registries found through the ament index and
converts between protobuf messages and the generated ROS 2 messages, with the
same rules as the C++ runtime (``cyber/ros2/native_types.cc``), so Python and
C++ nodes -- and plain ROS 2 nodes -- exchange ordinary typed messages.

Field presence: a ROS 2 message carries ``proto_has_bits``. Going from ROS 2
to protobuf a singular field is present when its bit is set, or when its
value differs from the protobuf default; a oneof member must in addition
differ from what a newly constructed ROS 2 message holds. The value a ROS 2
node sees is always the value Apollo reads: an absent field shows its
protobuf default.

Environment (same as C++): ``APOLLO_ROS_WIRE_FORMAT=proto`` switches native
types off; ``APOLLO_ROS_PROTO_CHANNELS=<regex>`` keeps the matching channels
and services on the serialized-protobuf wire format
(``apollo_msgs/msg/ProtoMessage``).
"""

import array
import os
import re
import struct
import sys
import threading

from google.protobuf import descriptor_pb2
from google.protobuf import descriptor_pool
from google.protobuf import message_factory
from google.protobuf.descriptor import FieldDescriptor

RESOURCE_TYPE = 'apollo_proto_types'
PROTO_MESSAGE_TYPE = 'apollo_msgs/msg/ProtoMessage'
PROTO_SERVICE_TYPE = 'apollo_msgs/srv/ProtoService'

_SCALAR, _STRING, _BYTES, _MESSAGE, _RECURSIVE = range(5)

# array.array type codes of the numeric ROS 2 sequence types.
_ARRAY_CODES = {
    'double': 'd', 'float': 'f', 'int8': 'b', 'uint8': 'B', 'octet': 'B',
    'int16': 'h', 'uint16': 'H', 'int32': 'i', 'uint32': 'I', 'int64': 'q',
    'uint64': 'Q',
}


class TypeInfo(object):
    """Registry entry of a protobuf message type."""
    __slots__ = ('proto_name', 'ros_type', 'hash', 'members')

    def __init__(self, proto_name, ros_type, type_hash):
        self.proto_name = proto_name      # apollo.planning.ADCTrajectory
        self.ros_type = ros_type          # apollo_msgs/msg/PlanningADCTrajectory
        self.hash = type_hash
        self.members = []                 # [(kind, ros_name, number, bit)]


class ServiceInfo(object):
    """Registry entry of an Apollo service."""
    __slots__ = ('request', 'response', 'ros_type')

    def __init__(self, request, response, ros_type):
        self.request = request
        self.response = response
        self.ros_type = ros_type


def structure_hash(descriptor):
    """Structure hash of a protobuf message definition (FNV-1a 64), as
    written to the registry by proto2ros.py and computed by the C++ runtime."""
    canonical = descriptor.full_name
    for field in descriptor.fields:
        type_name = ''
        if field.message_type is not None:
            type_name = field.message_type.full_name
        elif field.enum_type is not None:
            type_name = field.enum_type.full_name
        canonical += '|%d,%d,%d,%s' % (field.number, field.type, field.label,
                                       type_name)
    h = 0xcbf29ce484222325
    for byte in canonical.encode('utf-8'):
        h ^= byte
        h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % h


class Registry(object):
    """The registries of all installed interface packages plus their
    protobuf descriptors."""

    def __init__(self):
        self._lock = threading.RLock()
        self._loaded = False
        self._by_proto = {}
        self._by_ros = {}
        self._services = []
        self._descriptor_files = []
        self._pool = None
        self.enabled = os.environ.get('APOLLO_ROS_WIRE_FORMAT') != 'proto'
        self._proto_channels = None
        pattern = os.environ.get('APOLLO_ROS_PROTO_CHANNELS')
        if pattern:
            try:
                self._proto_channels = re.compile(pattern)
            except re.error as e:
                sys.stderr.write('cyber: APOLLO_ROS_PROTO_CHANNELS is not a '
                                 'valid regex: %s\n' % e)

    # -- registry -----------------------------------------------------------

    def _parse(self, path):
        current = None
        try:
            with open(path, 'r', encoding='utf-8') as f:
                lines = f.read().splitlines()
        except OSError:
            sys.stderr.write('cyber: cannot read %s\n' % path)
            return
        for line in lines:
            if not line or line[0] == '#':
                continue
            parts = line.split()
            tag = parts[0]
            if tag == 'V':
                if len(parts) < 2 or parts[1] != '1':
                    sys.stderr.write('cyber: %s: unsupported registry '
                                     'version\n' % path)
                    return
            elif tag == 'M' and len(parts) >= 4:
                current = None
                if parts[1] in self._by_proto:
                    continue             # first registration wins
                current = TypeInfo(parts[1], parts[2], parts[3])
                self._by_proto[current.proto_name] = current
                self._by_ros[current.ros_type] = current
            elif tag in ('f', 'r', 'h') and len(parts) >= 4:
                if current is not None:
                    current.members.append(
                        (tag, parts[1], int(parts[2]), int(parts[3])))
            elif tag == 'S' and len(parts) >= 4:
                self._services.append(ServiceInfo(parts[1], parts[2], parts[3]))

    def _load(self):
        with self._lock:
            if self._loaded:
                return
            self._loaded = True
            try:
                import ament_index_python
                resources = ament_index_python.get_resources(RESOURCE_TYPE)
            except Exception:  # pylint: disable=broad-except
                resources = {}
            for package in sorted(resources):
                try:
                    content, prefix = ament_index_python.get_resource(
                        RESOURCE_TYPE, package)
                except Exception:  # pylint: disable=broad-except
                    continue
                directory = os.path.join(prefix, content.strip())
                self._parse(os.path.join(directory, 'registry.txt'))
                self._descriptor_files.append(
                    os.path.join(directory, 'protos.desc'))

    def find_by_proto_name(self, proto_name):
        self._load()
        return self._by_proto.get(proto_name)

    def find_by_ros_type(self, ros_type):
        self._load()
        return self._by_ros.get(ros_type)

    def find_service(self, request, response):
        self._load()
        for service in self._services:
            if service.request == request and service.response == response:
                return service
        return None

    def find_service_by_ros_type(self, ros_type):
        self._load()
        for service in self._services:
            if service.ros_type == ros_type:
                return service
        return None

    def proto_names(self):
        self._load()
        return sorted(self._by_proto)

    def services(self):
        self._load()
        return list(self._services)

    def __len__(self):
        self._load()
        return len(self._by_proto)

    def use_proto_wire(self, channel):
        """True when `channel` must keep the protobuf wire format."""
        if not self.enabled:
            return True
        return (self._proto_channels is not None and
                self._proto_channels.search(channel) is not None)

    def is_apollo_topic_type(self, ros_type):
        return (ros_type == PROTO_MESSAGE_TYPE or
                self.find_by_ros_type(ros_type) is not None)

    def is_apollo_service_type(self, ros_type):
        return (ros_type == PROTO_SERVICE_TYPE or
                self.find_service_by_ros_type(ros_type) is not None)

    def proto_type_of(self, ros_types):
        """Protobuf type carried by a topic with these ROS 2 types ('' when
        none of them is a native Apollo type)."""
        for ros_type in ros_types:
            info = self.find_by_ros_type(ros_type)
            if info is not None:
                return info.proto_name
        return ''

    # -- descriptors --------------------------------------------------------

    def _installed_pool(self):
        """A private pool with the descriptors shipped by the interface
        packages (never mixed into the default pool, whose files may come
        from differently built ``*_pb2`` modules)."""
        with self._lock:
            if self._pool is not None:
                return self._pool
            self._load()
            pool = descriptor_pool.DescriptorPool()
            files = {}
            for path in self._descriptor_files:
                try:
                    with open(path, 'rb') as f:
                        file_set = descriptor_pb2.FileDescriptorSet.FromString(
                            f.read())
                except Exception:  # pylint: disable=broad-except
                    sys.stderr.write('cyber: cannot read %s\n' % path)
                    continue
                for file_proto in file_set.file:
                    files.setdefault(file_proto.name, file_proto)
            added = set()

            def add(name):
                if name in added or name not in files:
                    return
                added.add(name)
                for dependency in files[name].dependency:
                    add(dependency)
                try:
                    pool.Add(files[name])
                except Exception as e:  # pylint: disable=broad-except
                    sys.stderr.write('cyber: cannot build descriptors of '
                                     '%s: %s\n' % (name, e))

            for name in list(files):
                add(name)
            self._pool = pool
            return pool

    def find_descriptor(self, proto_name):
        """Descriptor of a protobuf type: the ``*_pb2`` modules imported in
        this process first, then the installed descriptor set (so tools work
        with types they never imported)."""
        try:
            return descriptor_pool.Default().FindMessageTypeByName(proto_name)
        except KeyError:
            pass
        try:
            return self._installed_pool().FindMessageTypeByName(proto_name)
        except KeyError:
            return None

    def find_installed_descriptor(self, proto_name):
        """Descriptor from the installed descriptor set only."""
        try:
            return self._installed_pool().FindMessageTypeByName(proto_name)
        except KeyError:
            return None

    def message_class(self, proto_name):
        """Message class of a protobuf type, or None."""
        descriptor = self.find_descriptor(proto_name)
        if descriptor is None:
            return None
        return message_factory.GetMessageClass(descriptor)


_registry = None
_registry_lock = threading.Lock()


def registry():
    """The process-wide Registry."""
    global _registry  # pylint: disable=global-statement
    with _registry_lock:
        if _registry is None:
            _registry = Registry()
        return _registry


def _reset_for_tests():
    """Drops the registry and every converter (tests change the
    environment variables that are read once)."""
    global _registry  # pylint: disable=global-statement
    with _registry_lock:
        _registry = None
    with _build_lock:
        _converters.clear()
        _top_level.clear()


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

def _float32(value):
    try:
        return struct.unpack('f', struct.pack('f', value))[0]
    except (OverflowError, struct.error):
        return value


def _same(value, default, is_float32=False):
    """value == default, NaN equal to NaN, float32 fields compared as such."""
    if value == default:
        return True
    if value != value and default != default:   # pylint: disable=comparison-with-itself
        return True
    if is_float32:
        a, b = _float32(value), _float32(default)
        return a == b or (a != a and b != b)   # pylint: disable=comparison-with-itself
    return False


class _Op(object):
    __slots__ = ('kind', 'name', 'slot', 'bit', 'repeated', 'is_map',
                 'in_oneof', 'required', 'default', 'is_float32',
                 'array_code', 'nested', 'has_presence', 'is_enum')


class Converter(object):
    """Converts between one protobuf type and its native ROS 2 type."""

    def __init__(self, descriptor, ros_class, ros_type):
        self.descriptor = descriptor
        self.ros_class = ros_class
        self.ros_type = ros_type
        self.message_class = message_factory.GetMessageClass(descriptor)
        self._ops = []
        self._has_slots = []
        self._prototype = None
        #: True when a newly constructed ROS 2 message holds the protobuf
        #: default of every field (not for the NaN defaults of Apollo's
        #: geometry types: a .msg file cannot express them).
        self.defaults_match = False

    # -- protobuf -> ROS 2 ---------------------------------------------------

    def to_ros(self, proto, ros=None):
        """Fills (and returns) a ROS 2 message from a protobuf message."""
        if ros is None:
            ros = self.ros_class()
        words = [0] * len(self._has_slots)
        for op in self._ops:
            value = getattr(proto, op.name)
            kind = op.kind
            if op.repeated:
                if op.is_map:
                    setattr(ros, op.slot,
                            [op.nested._entry_to_ros(k, value[k])  # pylint: disable=protected-access
                             for k in value])
                elif kind == _MESSAGE:
                    nested = op.nested
                    setattr(ros, op.slot, [nested.to_ros(m) for m in value])
                elif kind == _RECURSIVE:
                    setattr(ros, op.slot, [_wrap(m) for m in value])
                elif op.array_code is not None:
                    setattr(ros, op.slot, array.array(op.array_code, value))
                else:
                    setattr(ros, op.slot, list(value))
                continue
            has = proto.HasField(op.name) if op.has_presence else False
            if has and op.bit >= 0:
                words[op.bit >> 6] |= 1 << (op.bit & 63)
            if kind == _SCALAR or kind == _STRING:
                setattr(ros, op.slot, value)
            elif kind == _BYTES:
                setattr(ros, op.slot, array.array('B', value))
            elif kind == _MESSAGE:
                # An absent sub-message reads as its protobuf defaults, which
                # a newly constructed ROS 2 message normally holds already.
                if has or not op.nested.defaults_match:
                    op.nested.to_ros(value, getattr(ros, op.slot))
            elif has:
                _wrap(value, getattr(ros, op.slot))
        for index, slot in enumerate(self._has_slots):
            if slot is not None:
                setattr(ros, slot, words[index])
        return ros

    def _entry_to_ros(self, key, value):
        """ROS 2 map entry message of one key/value pair."""
        ros = self.ros_class()
        word = 0
        for op in self._ops:
            item = key if op.name == 'key' else value
            if op.kind == _MESSAGE:
                op.nested.to_ros(item, getattr(ros, op.slot))
            elif op.kind == _BYTES:
                setattr(ros, op.slot, array.array('B', item))
            else:
                setattr(ros, op.slot, item)
            if op.bit >= 0:
                word |= 1 << (op.bit & 63)
        if self._has_slots and self._has_slots[0] is not None:
            setattr(ros, self._has_slots[0], word)
        return ros

    # -- ROS 2 -> protobuf ---------------------------------------------------

    def from_ros(self, ros, proto=None):
        """Fills (and returns) a protobuf message from a ROS 2 message.
        `proto` must be empty."""
        if proto is None:
            proto = self.message_class()
        words = [0 if slot is None else getattr(ros, slot)
                 for slot in self._has_slots]
        for op in self._ops:
            value = getattr(ros, op.slot)
            kind = op.kind
            if op.repeated:
                if not value:
                    continue
                target = getattr(proto, op.name)
                if op.is_map:
                    for entry in value:
                        op.nested._entry_from_ros(entry, target)  # pylint: disable=protected-access
                elif kind == _MESSAGE:
                    nested = op.nested
                    for item in value:
                        nested.from_ros(item, target.add())
                elif kind == _RECURSIVE:
                    for item in value:
                        target.add().MergeFromString(_data_of(item))
                elif op.is_enum:
                    for item in value:
                        try:
                            target.append(item)
                        except ValueError:
                            pass       # not a value of a closed enum
                else:
                    target.extend(value)
                continue
            # Present when the bit is set, when presence is not tracked
            # (required fields), or when the value differs from the protobuf
            # default. A oneof member must also differ from what a newly
            # constructed ROS 2 message holds.
            has = op.bit < 0 or (words[op.bit >> 6] >> (op.bit & 63)) & 1
            if not has and op.in_oneof and self._untouched(op, value):
                continue
            if kind == _MESSAGE:
                if has or not op.nested.is_pristine(value):
                    sub = getattr(proto, op.name)
                    sub.SetInParent()
                    op.nested.from_ros(value, sub)
            elif kind == _RECURSIVE:
                data = _data_of(value)
                present = (has or bool(data)) if op.bit >= 0 else \
                    (op.required or bool(data))
                if present:
                    sub = getattr(proto, op.name)
                    sub.SetInParent()
                    sub.MergeFromString(data)
            elif kind == _BYTES:
                data = _bytes_of(value)
                if has or data != op.default:
                    setattr(proto, op.name, data)
            elif has or not _same(value, op.default, op.is_float32):
                try:
                    setattr(proto, op.name, value)
                except ValueError:
                    pass               # not a value of a closed enum
        return proto

    def _entry_from_ros(self, ros, target):
        """Adds a ROS 2 map entry message to a protobuf map field."""
        key = None
        value_op = None
        for op in self._ops:
            if op.name == 'key':
                key = getattr(ros, op.slot)
            else:
                value_op = op
        if value_op is None:
            return
        value = getattr(ros, value_op.slot)
        if value_op.kind == _MESSAGE:
            value_op.nested.from_ros(value, target[key])
            target[key].SetInParent()
        elif value_op.kind == _BYTES:
            target[key] = _bytes_of(value)
        else:
            try:
                target[key] = value
            except ValueError:
                pass

    def is_pristine(self, ros, reference=None):
        """True when the ROS 2 message carries no information: no presence
        bit is set, every sequence is empty and every singular field equals
        its protobuf default or, with `reference` (a newly constructed ROS 2
        message), the value it has there."""
        for slot in self._has_slots:
            if slot is not None and getattr(ros, slot):
                return False
        for op in self._ops:
            value = getattr(ros, op.slot)
            if op.repeated:
                if len(value):
                    return False
                continue
            kind = op.kind
            if kind == _MESSAGE:
                if not op.nested.is_pristine(
                        value, None if reference is None
                        else getattr(reference, op.slot)):
                    return False
            elif kind == _RECURSIVE:
                if len(value.data):
                    return False
            elif kind == _BYTES:
                default = op.default if reference is None else \
                    _bytes_of(getattr(reference, op.slot))
                if _bytes_of(value) != default:
                    return False
            else:
                default = op.default if reference is None else \
                    getattr(reference, op.slot)
                if not _same(value, default, op.is_float32):
                    return False
        return True

    def _untouched(self, op, value):
        """True when a member still has the value of a newly constructed
        ROS 2 message."""
        reference = getattr(self._prototype, op.slot)
        if op.kind == _MESSAGE:
            return op.nested.is_pristine(value, reference)
        if op.kind == _RECURSIVE:
            return not len(value.data)
        if op.kind == _BYTES:
            return _bytes_of(value) == _bytes_of(reference)
        return _same(value, reference, op.is_float32)

    # -- serialized forms ----------------------------------------------------

    def parse(self, data):
        """Protobuf message of this type from its wire bytes."""
        proto = self.message_class()
        proto.MergeFromString(bytes(data))
        return proto

    def as_message(self, data):
        """`data` (a message object or wire bytes) as an instance of this
        converter's message class."""
        if isinstance(data, self.message_class):
            return data
        if isinstance(data, (bytes, bytearray, memoryview)):
            return self.parse(data)
        if isinstance(data, str):
            return self.parse(data.encode('utf-8'))
        # Same type built from another descriptor pool (or a foreign class).
        return self.parse(data.SerializePartialToString())


def _bytes_of(value):
    try:
        return value.tobytes()
    except AttributeError:
        return bytes(value)


def _data_of(wrapper):
    return _bytes_of(wrapper.data)


def _wrap(proto, wrapper=None):
    """A recursive protobuf field travels as apollo_msgs/msg/ProtoMessage."""
    if wrapper is None:
        from apollo_msgs.msg import ProtoMessage
        wrapper = ProtoMessage()
    wrapper.type = proto.DESCRIPTOR.full_name
    wrapper.data = array.array('B', proto.SerializePartialToString())
    return wrapper


_build_lock = threading.RLock()
_converters = {}      # (pool id, proto name, ros class) -> Converter | None
_top_level = {}       # (pool id, proto name) -> Converter | None


def _pool_id(descriptor):
    return id(descriptor.file.pool)


def _ros_class_of(type_string):
    """ROS 2 message class named by an entry of
    get_fields_and_field_types(): 'pkg/Name' or 'sequence<pkg/Name>'."""
    from rosidl_runtime_py.utilities import get_message
    name = type_string
    if name.startswith('sequence<') and name.endswith('>'):
        name = name[len('sequence<'):-1]
    if '/' not in name:
        return None
    package, message = name.split('/', 1)
    return get_message('%s/msg/%s' % (package, message))


def _build(descriptor, ros_class, ros_type):
    """Converter of `descriptor` for `ros_class`; raises ValueError when the
    two do not belong together. Caller holds _build_lock."""
    key = (_pool_id(descriptor), descriptor.full_name, ros_class)
    if key in _converters:
        converter = _converters[key]
        if converter is None:
            raise ValueError('%s has no usable native type' %
                             descriptor.full_name)
        return converter
    _converters[key] = None
    info = registry().find_by_proto_name(descriptor.full_name)
    if info is None:
        raise ValueError('%s is not in the native type registry' %
                         descriptor.full_name)
    if structure_hash(descriptor) != info.hash:
        raise ValueError(
            'the definition of %s in this process differs from the one %s '
            'was generated from' % (descriptor.full_name, info.ros_type))
    fields = list(ros_class.get_fields_and_field_types().items())
    members = info.members
    if not (len(fields) == len(members) or
            (not members and len(fields) == 1)):
        raise ValueError('%s has %d members, registry %d' %
                         (info.ros_type, len(fields), len(members)))
    converter = Converter(descriptor, ros_class, ros_type)
    for index, (kind, ros_name, number, bit) in enumerate(members):
        field_name, type_string = fields[index]
        if field_name != ros_name:
            raise ValueError('%s: member %s != %s' %
                             (info.ros_type, field_name, ros_name))
        slot = '_' + ros_name
        if kind == 'h':
            while len(converter._has_slots) <= number:  # pylint: disable=protected-access
                converter._has_slots.append(None)       # pylint: disable=protected-access
            converter._has_slots[number] = slot         # pylint: disable=protected-access
            continue
        field = descriptor.fields_by_number.get(number)
        if field is None:
            raise ValueError('%s has no field %d' %
                             (descriptor.full_name, number))
        op = _Op()
        op.name = field.name
        op.slot = slot
        op.bit = bit
        op.repeated = field.label == FieldDescriptor.LABEL_REPEATED
        op.required = field.label == FieldDescriptor.LABEL_REQUIRED
        op.in_oneof = (field.containing_oneof is not None and
                       not _is_synthetic_oneof(field))
        op.has_presence = bool(getattr(field, 'has_presence', False)) and \
            not op.repeated
        op.is_map = False
        op.nested = None
        op.array_code = None
        op.is_float32 = field.type == FieldDescriptor.TYPE_FLOAT
        op.is_enum = field.enum_type is not None
        op.default = None
        sequence = type_string.startswith('sequence<')
        if sequence != op.repeated and not (
                field.type == FieldDescriptor.TYPE_BYTES and sequence):
            raise ValueError('%s: member %s does not match field %s' %
                             (info.ros_type, ros_name, field.full_name))
        if kind == 'r':
            op.kind = _RECURSIVE
            if 'apollo_msgs/ProtoMessage' not in type_string:
                raise ValueError('%s: member %s is not a ProtoMessage' %
                                 (info.ros_type, ros_name))
        elif field.message_type is not None:
            op.kind = _MESSAGE
            nested_class = _ros_class_of(type_string)
            if nested_class is None:
                raise ValueError('%s: member %s is not a message' %
                                 (info.ros_type, ros_name))
            nested_info = registry().find_by_proto_name(
                field.message_type.full_name)
            op.nested = _build(field.message_type, nested_class,
                               nested_info.ros_type if nested_info else '')
            op.is_map = bool(field.message_type.GetOptions().map_entry)
        elif field.type == FieldDescriptor.TYPE_BYTES:
            op.kind = _BYTES
            op.default = bytes(field.default_value) \
                if not op.repeated else b''
        elif field.cpp_type == FieldDescriptor.CPPTYPE_STRING:
            op.kind = _STRING
            op.default = field.default_value if not op.repeated else ''
        else:
            op.kind = _SCALAR
            if not op.repeated:
                op.default = field.default_value
            else:
                element = type_string[len('sequence<'):-1]
                op.array_code = _ARRAY_CODES.get(element)
        converter._ops.append(op)  # pylint: disable=protected-access
    converter._prototype = ros_class()  # pylint: disable=protected-access
    converter.defaults_match = converter.is_pristine(converter._prototype)  # pylint: disable=protected-access
    _converters[key] = converter
    return converter


def _is_synthetic_oneof(field):
    """True for the one-member oneof protoc makes for a proto3 optional."""
    oneof = field.containing_oneof
    if oneof is None:
        return False
    if getattr(field, 'has_optional_keyword', False) and \
            len(oneof.fields) == 1 and oneof.name.startswith('_'):
        return True
    return False


def get_converter(descriptor, quiet=False):
    """Converter for `descriptor`, or None when the type has no native ROS 2
    interface matching this definition (the caller then uses
    apollo_msgs/msg/ProtoMessage)."""
    if descriptor is None:
        return None
    reg = registry()
    if not reg.enabled:
        return None
    key = (_pool_id(descriptor), descriptor.full_name)
    with _build_lock:
        if key in _top_level:
            return _top_level[key]
        converter = None
        info = reg.find_by_proto_name(descriptor.full_name)
        if info is not None:
            try:
                from rosidl_runtime_py.utilities import get_message
                converter = _build(descriptor, get_message(info.ros_type),
                                   info.ros_type)
            except Exception as e:  # pylint: disable=broad-except
                converter = None
                if not quiet:
                    sys.stderr.write(
                        'cyber: no native ROS 2 type for %s: %s; using %s\n' %
                        (descriptor.full_name, e, PROTO_MESSAGE_TYPE))
        _top_level[key] = converter
        return converter


def get_converter_by_name(proto_name, quiet=False):
    """Converter for a protobuf type name (descriptor from the imported
    ``*_pb2`` modules, else from the installed descriptor set)."""
    reg = registry()
    if not reg.enabled or reg.find_by_proto_name(proto_name) is None:
        return None
    descriptor = reg.find_descriptor(proto_name)
    converter = get_converter(descriptor, quiet=True)
    if converter is None and descriptor is not None:
        # The imported definition differs from the generated one; a tool that
        # only needs the bytes can still use the installed definition.
        installed = reg.find_installed_descriptor(proto_name)
        if installed is not None and installed is not descriptor:
            converter = get_converter(installed, quiet=quiet)
    return converter


def get_service_converters(request_descriptor, response_descriptor):
    """(service class, request converter, response converter) of the native
    service type for the two protobuf types, or None."""
    if request_descriptor is None or response_descriptor is None:
        return None
    reg = registry()
    if not reg.enabled:
        return None
    info = reg.find_service(request_descriptor.full_name,
                            response_descriptor.full_name)
    if info is None:
        return None
    with _build_lock:
        try:
            from rosidl_runtime_py.utilities import get_service
            service_class = get_service(info.ros_type)
            request = _build(request_descriptor, service_class.Request,
                             info.ros_type)
            response = _build(response_descriptor, service_class.Response,
                              info.ros_type)
            return service_class, request, response
        except Exception as e:  # pylint: disable=broad-except
            sys.stderr.write(
                'cyber: no native ROS 2 service type for %s: %s; using %s\n' %
                (request_descriptor.full_name, e, PROTO_SERVICE_TYPE))
            return None
