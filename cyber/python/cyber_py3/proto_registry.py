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
"""Protobuf type registry: fully-qualified type name -> message class.

Introspection tools (cyber_channel echo, record converters, ...) need to turn
the protobuf type name of a channel into a class. Lookup order:

1. classes registered explicitly (``register_message_class``/``register_file``,
   also done implicitly by ``Node.create_reader``/``create_writer``);
2. the default descriptor pool, i.e. any ``*_pb2`` module already imported;
3. installed ``*_pb2`` modules under ``<sys.path entry>/modules/**`` and
   ``<sys.path entry>/cyber/**``. They are indexed *without importing
   them* (the serialized descriptor is read out of each file) and only the
   module defining the requested type is imported, so unrelated modules with
   conflicting symbols are never loaded;
4. descriptors registered at runtime from a Cyber ``proto_desc`` blob (e.g. the
   per-channel descriptor stored in a .record file), built into a private pool;
5. the descriptor set installed with the native ROS 2 interfaces
   (``native_types``), which covers every Apollo channel type.
"""

import ast
import importlib
import importlib.util
import os
import re
import sys
import threading

from google.protobuf import descriptor_pb2
from google.protobuf import descriptor_pool
from google.protobuf import text_format
from google.protobuf.message import DecodeError

from . import _cyber_protos

try:  # protobuf >= 4.21
    from google.protobuf.message_factory import GetMessageClass as _get_class
except ImportError:  # pragma: no cover - protobuf 3.x
    from google.protobuf import message_factory as _mf
    _get_class = _mf.MessageFactory().GetPrototype

# Directories (relative to every sys.path entry) searched for *_pb2.py.
SEARCH_ROOTS = ('modules', 'cyber')

_lock = threading.RLock()
_classes = {}
_dynamic_pool = descriptor_pool.DescriptorPool()
_pb2_index = None
_proto_desc_cache = {}

# protoc >= 3.20: AddSerializedFile(b'...'); older: serialized_pb=b'...' or
# serialized_pb=_b('...').
_SERIALIZED_RE = re.compile(
    r"(?:AddSerializedFile\(|serialized_pb=)(_b\()?([bB]?'(?:[^'\\]|\\.)*')",
    re.S)


def _from_pool(pool, type_name):
    try:
        desc = pool.FindMessageTypeByName(type_name)
    except KeyError:
        return None
    return _get_class(desc)


def _walk_messages(prefix, message_protos):
    for msg in message_protos:
        name = prefix + msg.name if prefix else msg.name
        yield name
        for nested in _walk_messages(name + '.', msg.nested_type):
            yield nested


def _descriptor_of(obj):
    """Returns the message Descriptor of a message, class, Descriptor or name."""
    if isinstance(obj, str):
        cls = get_message_class(obj)
        if cls is None:
            raise KeyError('unknown protobuf type: %s' % obj)
        return cls.DESCRIPTOR
    desc = getattr(obj, 'DESCRIPTOR', obj)
    if not hasattr(desc, 'full_name') or not hasattr(desc, 'file'):
        raise TypeError('not a protobuf message: %r' % (obj,))
    return desc


def _register_descriptor(desc):
    _classes[desc.full_name] = _get_class(desc)
    for nested in desc.nested_types:
        _register_descriptor(nested)


def register_message_class(cls):
    """Registers a generated message class (and its nested types)."""
    with _lock:
        _register_descriptor(cls.DESCRIPTOR)


def register_file(file_desc):
    """Registers all message types of a FileDescriptor and its dependencies."""
    with _lock:
        for dep in file_desc.dependencies:
            register_file(dep)
        for desc in file_desc.message_types_by_name.values():
            _register_descriptor(desc)


def _index_pb2_file(path):
    """Returns the message type names defined by a generated *_pb2.py file."""
    try:
        with open(path, 'r', encoding='latin-1') as f:
            text = f.read()
    except OSError:
        return []
    match = _SERIALIZED_RE.search(text)
    if not match:
        return []
    try:
        literal = ast.literal_eval(match.group(2))
        if isinstance(literal, str):
            literal = literal.encode('latin-1')
        fdp = descriptor_pb2.FileDescriptorProto.FromString(literal)
    except (ValueError, SyntaxError, DecodeError):
        return []
    prefix = fdp.package + '.' if fdp.package else ''
    return list(_walk_messages(prefix, fdp.message_type))


def _build_pb2_index():
    index = {}
    seen = set()
    for entry in list(sys.path):
        base = os.path.abspath(entry or os.getcwd())
        for sub in SEARCH_ROOTS:
            root = os.path.join(base, sub)
            real = os.path.realpath(root)
            if real in seen or not os.path.isdir(root):
                continue
            seen.add(real)
            for dirpath, dirnames, filenames in os.walk(root):
                dirnames[:] = [d for d in dirnames
                               if not d.startswith('.') and d != '__pycache__'
                               and d.isidentifier()]
                for filename in filenames:
                    if not filename.endswith('_pb2.py'):
                        continue
                    path = os.path.join(dirpath, filename)
                    module = os.path.relpath(path, base)[:-3].replace(
                        os.sep, '.')
                    for type_name in _index_pb2_file(path):
                        index.setdefault(type_name, (module, path))
    return index


def _import_pb2(module, path):
    try:
        importlib.import_module(module)
        return
    except ImportError:
        pass
    # The file is not importable under its dotted name, e.g. because a
    # regular `cyber` package elsewhere on sys.path shadows this directory:
    # load it from its path (its own imports resolve normally).
    if module in sys.modules:
        return
    spec = importlib.util.spec_from_file_location(module, path)
    loaded = importlib.util.module_from_spec(spec)
    sys.modules[module] = loaded
    try:
        spec.loader.exec_module(loaded)
    except BaseException:
        sys.modules.pop(module, None)
        raise


def pb2_index(refresh=False):
    """Returns {type name: (module name, file)} of the installed *_pb2."""
    global _pb2_index
    with _lock:
        if _pb2_index is None or refresh:
            _pb2_index = _build_pb2_index()
        return _pb2_index


def get_message_class(type_name, proto_desc=None):
    """Returns the protobuf class for `type_name`, or None if unknown.

    Args:
      type_name: fully-qualified protobuf name, e.g. 'apollo.cyber.proto.Header'.
      proto_desc: optional Cyber proto_desc blob (cyber.proto.ProtoDesc or a
        bare FileDescriptorProto) used when no generated module defines it.
    """
    if not type_name:
        return None
    if isinstance(type_name, bytes):
        type_name = type_name.decode('utf-8')
    with _lock:
        cls = _classes.get(type_name)
        if cls is not None:
            return cls
        cls = _from_pool(descriptor_pool.Default(), type_name)
        if cls is None:
            entry = pb2_index().get(type_name)
            if entry is not None:
                try:
                    _import_pb2(*entry)
                except Exception as e:  # pylint: disable=broad-except
                    sys.stderr.write('cyber_py3: failed to import %s: %s\n' %
                                     (entry[1], e))
                cls = _from_pool(descriptor_pool.Default(), type_name)
        if cls is None:
            cls = _from_pool(_dynamic_pool, type_name)
        if cls is None and proto_desc:
            register_proto_desc(proto_desc)
            cls = _from_pool(_dynamic_pool, type_name)
        if cls is None:
            # The descriptor set installed with the native ROS 2 interfaces
            # knows every Apollo channel type, imported or not.
            try:
                from . import native_types
                cls = native_types.registry().message_class(type_name)
            except Exception:  # pylint: disable=broad-except
                cls = None
        if cls is not None:
            _classes[type_name] = cls
        return cls


def _file_protos(proto_desc):
    """Decodes a proto_desc blob into FileDescriptorProtos, deps first."""
    if not proto_desc:
        return []
    pd = _cyber_protos.ProtoDesc()
    try:
        pd.ParseFromString(proto_desc)
        fdp = descriptor_pb2.FileDescriptorProto.FromString(pd.desc)
        if pd.desc and fdp.name.endswith('.proto'):
            result = []
            for dep in pd.dependencies:
                result.extend(_file_protos(dep.SerializeToString()))
            result.append(fdp)
            return result
    except DecodeError:
        pass
    # Some writers (old cyber_py3 examples) store a bare FileDescriptorProto.
    try:
        fdp = descriptor_pb2.FileDescriptorProto.FromString(proto_desc)
    except DecodeError:
        return []
    return [fdp] if fdp.name.endswith('.proto') else []


def _add_to_dynamic_pool(fdp):
    try:
        _dynamic_pool.FindFileByName(fdp.name)
        return True
    except KeyError:
        pass
    for dep in fdp.dependency:
        try:
            _dynamic_pool.FindFileByName(dep)
        except KeyError:
            # Not shipped with the blob: borrow it from the default pool.
            try:
                dep_file = descriptor_pool.Default().FindFileByName(dep)
            except KeyError:
                return False
            if not _add_to_dynamic_pool(
                    descriptor_pb2.FileDescriptorProto.FromString(
                        dep_file.serialized_pb)):
                return False
    try:
        _dynamic_pool.Add(fdp)
    except (TypeError, ValueError) as e:
        sys.stderr.write('cyber_py3: cannot register %s: %s\n' % (fdp.name, e))
        return False
    return True


def register_proto_desc(proto_desc):
    """Registers the types of a Cyber proto_desc blob. Returns success."""
    if isinstance(proto_desc, str):
        proto_desc = proto_desc.encode('latin-1')
    with _lock:
        files = _file_protos(proto_desc)
        ok = bool(files)
        for fdp in files:
            ok = _add_to_dynamic_pool(fdp) and ok
        return ok


def _proto_desc_of_file(file_desc):
    cached = _proto_desc_cache.get(file_desc.name)
    if cached is not None and cached[0] is file_desc:
        return cached[1]
    pd = _cyber_protos.ProtoDesc()
    pd.desc = file_desc.serialized_pb
    for dep in file_desc.dependencies:
        pd.dependencies.add().CopyFrom(_proto_desc_of_file(dep))
    _proto_desc_cache[file_desc.name] = (file_desc, pd)
    return pd


def get_proto_desc(obj):
    """Returns the serialized cyber.proto.ProtoDesc of a message type.

    Same bytes as C++ ProtobufFactory::GetDescriptorString(): the file
    descriptor plus, recursively, all of its dependencies.

    Args:
      obj: message instance, message class, Descriptor or type name.
    """
    desc = _descriptor_of(obj)
    with _lock:
        return _proto_desc_of_file(desc.file).SerializeToString()


def parse_message(type_name, data, proto_desc=None):
    """Parses wire bytes into a message of `type_name`; None if impossible."""
    cls = get_message_class(type_name, proto_desc)
    if cls is None:
        return None
    msg = cls()
    try:
        msg.ParseFromString(bytes(data))
    except DecodeError:
        return None
    return msg


def to_debug_string(type_name, data, proto_desc=None):
    """Human readable text of a serialized message ('' when unknown/broken)."""
    msg = parse_message(type_name, data, proto_desc)
    if msg is None:
        return ''
    return text_format.MessageToString(msg, as_utf8=True)
