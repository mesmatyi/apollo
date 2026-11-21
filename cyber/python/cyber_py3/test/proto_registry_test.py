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
"""Type name -> protobuf class lookup."""

from google.protobuf import descriptor_pb2

from conftest import requires_cyber_protos
from conftest import run_python

from cyber.python.cyber_py3 import _cyber_protos
from cyber.python.cyber_py3 import proto_registry


def _dynamic_file(package):
    fdp = descriptor_pb2.FileDescriptorProto()
    fdp.name = '%s/dyn.proto' % package.replace('.', '/')
    fdp.package = package
    fdp.dependency.append('cyber/proto/proto_desc.proto')
    msg = fdp.message_type.add()
    msg.name = 'Dyn'
    field = msg.field.add()
    field.name, field.number = 'text', 1
    field.type = descriptor_pb2.FieldDescriptorProto.TYPE_STRING
    field.label = descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
    field = msg.field.add()
    field.name, field.number = 'desc', 2
    field.type = descriptor_pb2.FieldDescriptorProto.TYPE_MESSAGE
    field.label = descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
    field.type_name = '.apollo.cyber.proto.ProtoDesc'
    return fdp


def test_unknown_type():
    assert proto_registry.get_message_class('no.such.Type') is None
    assert proto_registry.get_message_class('') is None
    assert proto_registry.to_debug_string('no.such.Type', b'\x08\x01') == ''


def test_registered_class():
    proto_registry.register_message_class(_cyber_protos.ChannelCache)
    assert proto_registry.get_message_class(
        'apollo.cyber.proto.ChannelCache') is _cyber_protos.ChannelCache
    assert proto_registry.get_message_class(
        b'apollo.cyber.proto.ChannelCache') is _cyber_protos.ChannelCache


def test_proto_desc_round_trip():
    """A type known only through a Cyber proto_desc blob (as stored in
    records) becomes usable, dependencies included."""
    dep = _cyber_protos.ProtoDesc()
    dep.desc = _cyber_protos.serialized_file('cyber/proto/proto_desc.proto')
    blob = _cyber_protos.ProtoDesc()
    blob.desc = _dynamic_file('test.registry.a').SerializeToString()
    blob.dependencies.add().CopyFrom(dep)

    cls = proto_registry.get_message_class('test.registry.a.Dyn',
                                           blob.SerializeToString())
    assert cls is not None
    msg = cls(text='hello')
    msg.desc.desc = b'x'
    text = proto_registry.to_debug_string('test.registry.a.Dyn',
                                          msg.SerializeToString())
    assert 'hello' in text
    # get_proto_desc() rebuilds the same structure: file + dependencies
    rebuilt = _cyber_protos.ProtoDesc.FromString(
        proto_registry.get_proto_desc(cls))
    assert rebuilt.desc == blob.desc
    assert [d.desc for d in rebuilt.dependencies] == [dep.desc]


def test_bare_file_descriptor_blob():
    """Old cyber_py3 examples stored a bare FileDescriptorProto."""
    fdp = _dynamic_file('test.registry.b')
    fdp.ClearField('dependency')
    del fdp.message_type[0].field[1]
    assert proto_registry.register_proto_desc(fdp.SerializeToString())
    assert proto_registry.get_message_class('test.registry.b.Dyn') is not None


@requires_cyber_protos
def test_generated_module_found_without_import():
    """Installed *_pb2 modules are indexed without importing them, and only
    the module defining the type is imported."""
    code = '''
import sys
from cyber.python.cyber_py3 import proto_registry
assert 'cyber.proto.clock_pb2' not in sys.modules
cls = proto_registry.get_message_class('apollo.cyber.proto.Clock')
assert cls is not None and cls.__module__ == 'cyber.proto.clock_pb2', cls
assert 'cyber.proto.unit_test_pb2' not in sys.modules
nested = proto_registry.get_message_class(
    'apollo.cyber.proto.SimpleRepeatedMessage')
assert nested is not None
print('OK')
'''
    result = run_python(code)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == 'OK'


@requires_cyber_protos
def test_proto_desc_matches_cpp_layout():
    from cyber.proto import role_attributes_pb2
    blob = _cyber_protos.ProtoDesc.FromString(
        proto_registry.get_proto_desc(role_attributes_pb2.RoleAttributes))
    names = [descriptor_pb2.FileDescriptorProto.FromString(blob.desc).name] + \
        [descriptor_pb2.FileDescriptorProto.FromString(d.desc).name
         for d in blob.dependencies]
    assert names == ['cyber/proto/role_attributes.proto',
                     'cyber/proto/qos_profile.proto']
    assert proto_registry.get_proto_desc('apollo.cyber.proto.RoleAttributes') \
        == blob.SerializeToString()
