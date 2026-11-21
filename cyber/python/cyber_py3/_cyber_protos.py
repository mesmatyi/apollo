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
"""The few Cyber protos that cyber_py3 itself needs, embedded.

cyber_py3 must work without generated ``cyber.proto.*_pb2`` modules being
importable (e.g. a pure-Python record tool on a machine without the Apollo
install), so the descriptors of record.proto, proto_desc.proto,
qos_profile.proto, role_attributes.proto and parameter.proto are embedded here
and built into a *private* descriptor pool. Using a private pool means these
classes never collide with ``cyber.proto.*_pb2`` if a script imports those too;
the wire format is of course identical.

The blobs are ``FileDescriptor.serialized_pb`` of the protoc-generated modules
(i.e. ``FileDescriptor::CopyTo()`` output, no json_name / source info). To
refresh them after editing those protos run::

    python3 -m cyber.python.cyber_py3._cyber_protos <apollo_root>

and paste the printed tuple over ``_SERIALIZED_FILES``. The unit test
``cyber_protos_test.py`` fails when they drift from cyber/proto/*.proto.
"""

import base64

from google.protobuf import descriptor_pb2
from google.protobuf import descriptor_pool

try:  # protobuf >= 4.21
    from google.protobuf.message_factory import GetMessageClass as _get_class
except ImportError:  # pragma: no cover - protobuf 3.x
    from google.protobuf import message_factory as _mf
    _FACTORY = _mf.MessageFactory()
    _get_class = _FACTORY.GetPrototype

# (file name, base64 of serialized FileDescriptorProto), dependency order.
_SERIALIZED_FILES = (
    ('cyber/proto/qos_profile.proto',
     'Ch1jeWJlci9wcm90by9xb3NfcHJvZmlsZS5wcm90bxISYXBvbGxvLmN5YmVyLnByb3Rv'
     'Ip8CCgpRb3NQcm9maWxlEkgKB2hpc3RvcnkYASABKA4yJC5hcG9sbG8uY3liZXIucHJv'
     'dG8uUW9zSGlzdG9yeVBvbGljeToRSElTVE9SWV9LRUVQX0xBU1QSEAoFZGVwdGgYAiAB'
     'KA06ATESDgoDbXBzGAMgASgNOgEwElMKC3JlbGlhYmlsaXR5GAQgASgOMiguYXBvbGxv'
     'LmN5YmVyLnByb3RvLlFvc1JlbGlhYmlsaXR5UG9saWN5OhRSRUxJQUJJTElUWV9SRUxJ'
     'QUJMRRJQCgpkdXJhYmlsaXR5GAUgASgOMicuYXBvbGxvLmN5YmVyLnByb3RvLlFvc0R1'
     'cmFiaWxpdHlQb2xpY3k6E0RVUkFCSUxJVFlfVk9MQVRJTEUqWwoQUW9zSGlzdG9yeVBv'
     'bGljeRIaChZISVNUT1JZX1NZU1RFTV9ERUZBVUxUEAASFQoRSElTVE9SWV9LRUVQX0xB'
     'U1QQARIUChBISVNUT1JZX0tFRVBfQUxMEAIqbQoUUW9zUmVsaWFiaWxpdHlQb2xpY3kS'
     'HgoaUkVMSUFCSUxJVFlfU1lTVEVNX0RFRkFVTFQQABIYChRSRUxJQUJJTElUWV9SRUxJ'
     'QUJMRRABEhsKF1JFTElBQklMSVRZX0JFU1RfRUZGT1JUEAIqbQoTUW9zRHVyYWJpbGl0'
     'eVBvbGljeRIdChlEVVJBQklMSVRZX1NZU1RFTV9ERUZBVUxUEAASHgoaRFVSQUJJTElU'
     'WV9UUkFOU0lFTlRfTE9DQUwQARIXChNEVVJBQklMSVRZX1ZPTEFUSUxFEAI='),
    ('cyber/proto/role_attributes.proto',
     'CiFjeWJlci9wcm90by9yb2xlX2F0dHJpYnV0ZXMucHJvdG8SEmFwb2xsby5jeWJlci5w'
     'cm90bxodY3liZXIvcHJvdG8vcW9zX3Byb2ZpbGUucHJvdG8iJgoKU29ja2V0QWRkchIK'
     'CgJpcBgBIAEoCRIMCgRwb3J0GAIgASgNIuACCg5Sb2xlQXR0cmlidXRlcxIRCglob3N0'
     'X25hbWUYASABKAkSDwoHaG9zdF9pcBgCIAEoCRISCgpwcm9jZXNzX2lkGAMgASgFEhEK'
     'CW5vZGVfbmFtZRgEIAEoCRIPCgdub2RlX2lkGAUgASgEEhQKDGNoYW5uZWxfbmFtZRgG'
     'IAEoCRISCgpjaGFubmVsX2lkGAcgASgEEhQKDG1lc3NhZ2VfdHlwZRgIIAEoCRISCgpw'
     'cm90b19kZXNjGAkgASgMEgoKAmlkGAogASgEEjMKC3Fvc19wcm9maWxlGAsgASgLMh4u'
     'YXBvbGxvLmN5YmVyLnByb3RvLlFvc1Byb2ZpbGUSMwoLc29ja2V0X2FkZHIYDCABKAsy'
     'Hi5hcG9sbG8uY3liZXIucHJvdG8uU29ja2V0QWRkchIUCgxzZXJ2aWNlX25hbWUYDSAB'
     'KAkSEgoKc2VydmljZV9pZBgOIAEoBA=='),
    ('cyber/proto/record.proto',
     'ChhjeWJlci9wcm90by9yZWNvcmQucHJvdG8SEmFwb2xsby5jeWJlci5wcm90byKWAgoL'
     'U2luZ2xlSW5kZXgSLQoEdHlwZRgBIAEoDjIfLmFwb2xsby5jeWJlci5wcm90by5TZWN0'
     'aW9uVHlwZRIQCghwb3NpdGlvbhgCIAEoBBI5Cg1jaGFubmVsX2NhY2hlGGUgASgLMiAu'
     'YXBvbGxvLmN5YmVyLnByb3RvLkNoYW5uZWxDYWNoZUgAEkIKEmNodW5rX2hlYWRlcl9j'
     'YWNoZRhmIAEoCzIkLmFwb2xsby5jeWJlci5wcm90by5DaHVua0hlYWRlckNhY2hlSAAS'
     'PgoQY2h1bmtfYm9keV9jYWNoZRhnIAEoCzIiLmFwb2xsby5jeWJlci5wcm90by5DaHVu'
     'a0JvZHlDYWNoZUgAQgcKBWNhY2hlImIKEENodW5rSGVhZGVyQ2FjaGUSFgoObWVzc2Fn'
     'ZV9udW1iZXIYASABKAQSEgoKYmVnaW5fdGltZRgCIAEoBBIQCghlbmRfdGltZRgDIAEo'
     'BBIQCghyYXdfc2l6ZRgEIAEoBCIoCg5DaHVua0JvZHlDYWNoZRIWCg5tZXNzYWdlX251'
     'bWJlchgBIAEoBCJeCgxDaGFubmVsQ2FjaGUSFgoObWVzc2FnZV9udW1iZXIYASABKAQS'
     'DAoEbmFtZRgCIAEoCRIUCgxtZXNzYWdlX3R5cGUYAyABKAkSEgoKcHJvdG9fZGVzYxgE'
     'IAEoDCJECg1TaW5nbGVNZXNzYWdlEhQKDGNoYW5uZWxfbmFtZRgBIAEoCRIMCgR0aW1l'
     'GAIgASgEEg8KB2NvbnRlbnQYAyABKAwiKAoHTWFwSW5mbxIMCgRuYW1lGAEgASgJEg8K'
     'B3ZlcnNpb24YAiABKAkiGwoLVmVoaWNsZUluZm8SDAoEbmFtZRgBIAEoCSL3AwoGSGVh'
     'ZGVyEhUKDW1ham9yX3ZlcnNpb24YASABKA0SFQoNbWlub3JfdmVyc2lvbhgCIAEoDRIy'
     'Cghjb21wcmVzcxgDIAEoDjIgLmFwb2xsby5jeWJlci5wcm90by5Db21wcmVzc1R5cGUS'
     'FgoOY2h1bmtfaW50ZXJ2YWwYBCABKAQSGAoQc2VnbWVudF9pbnRlcnZhbBgFIAEoBBIZ'
     'Cg5pbmRleF9wb3NpdGlvbhgGIAEoBDoBMBIXCgxjaHVua19udW1iZXIYByABKAQ6ATAS'
     'GQoOY2hhbm5lbF9udW1iZXIYCCABKAQ6ATASFQoKYmVnaW5fdGltZRgJIAEoBDoBMBIT'
     'CghlbmRfdGltZRgKIAEoBDoBMBIZCg5tZXNzYWdlX251bWJlchgLIAEoBDoBMBIPCgRz'
     'aXplGAwgASgEOgEwEhoKC2lzX2NvbXBsZXRlGA0gASgIOgVmYWxzZRIWCg5jaHVua19y'
     'YXdfc2l6ZRgOIAEoBBIYChBzZWdtZW50X3Jhd19zaXplGA8gASgEEi0KCG1hcF9pbmZv'
     'GBAgASgLMhsuYXBvbGxvLmN5YmVyLnByb3RvLk1hcEluZm8SNQoMdmVoaWNsZV9pbmZv'
     'GBEgASgLMh8uYXBvbGxvLmN5YmVyLnByb3RvLlZlaGljbGVJbmZvIkEKB0NoYW5uZWwS'
     'DAoEbmFtZRgBIAEoCRIUCgxtZXNzYWdlX3R5cGUYAiABKAkSEgoKcHJvdG9fZGVzYxgD'
     'IAEoDCJdCgtDaHVua0hlYWRlchISCgpiZWdpbl90aW1lGAEgASgEEhAKCGVuZF90aW1l'
     'GAIgASgEEhYKDm1lc3NhZ2VfbnVtYmVyGAMgASgEEhAKCHJhd19zaXplGAQgASgEIkAK'
     'CUNodW5rQm9keRIzCghtZXNzYWdlcxgBIAMoCzIhLmFwb2xsby5jeWJlci5wcm90by5T'
     'aW5nbGVNZXNzYWdlIjkKBUluZGV4EjAKB2luZGV4ZXMYASADKAsyHy5hcG9sbG8uY3li'
     'ZXIucHJvdG8uU2luZ2xlSW5kZXgiZgoKUmVjb3JkSW5mbxIVCgtyZWNvcmRfbmFtZRgB'
     'IAEoCToAEhQKDHRvdGFsX3RpbWVfcxgCIAEoARIWCgtjdXJyX3RpbWVfcxgDIAEoAToB'
     'MBITCghwcm9ncmVzcxgEIAEoAToBMCp7CgtTZWN0aW9uVHlwZRISCg5TRUNUSU9OX0hF'
     'QURFUhAAEhgKFFNFQ1RJT05fQ0hVTktfSEVBREVSEAESFgoSU0VDVElPTl9DSFVOS19C'
     'T0RZEAISEQoNU0VDVElPTl9JTkRFWBADEhMKD1NFQ1RJT05fQ0hBTk5FTBAEKkUKDENv'
     'bXByZXNzVHlwZRIRCg1DT01QUkVTU19OT05FEAASEAoMQ09NUFJFU1NfQloyEAESEAoM'
     'Q09NUFJFU1NfTFo0EAI='),
    ('cyber/proto/proto_desc.proto',
     'ChxjeWJlci9wcm90by9wcm90b19kZXNjLnByb3RvEhJhcG9sbG8uY3liZXIucHJvdG8i'
     'TgoJUHJvdG9EZXNjEgwKBGRlc2MYASABKAwSMwoMZGVwZW5kZW5jaWVzGAIgAygLMh0u'
     'YXBvbGxvLmN5YmVyLnByb3RvLlByb3RvRGVzYw=='),
    ('cyber/proto/parameter.proto',
     'ChtjeWJlci9wcm90by9wYXJhbWV0ZXIucHJvdG8SEmFwb2xsby5jeWJlci5wcm90byLT'
     'AQoFUGFyYW0SDAoEbmFtZRgBIAEoCRIrCgR0eXBlGAIgASgOMh0uYXBvbGxvLmN5YmVy'
     'LnByb3RvLlBhcmFtVHlwZRIRCgl0eXBlX25hbWUYAyABKAkSFAoKYm9vbF92YWx1ZRgE'
     'IAEoCEgAEhMKCWludF92YWx1ZRgFIAEoA0gAEhYKDGRvdWJsZV92YWx1ZRgGIAEoAUgA'
     'EhYKDHN0cmluZ192YWx1ZRgHIAEoCUgAEhIKCnByb3RvX2Rlc2MYCCABKAxCDQoLb25l'
     'b2ZfdmFsdWUiGQoITm9kZU5hbWUSDQoFdmFsdWUYASABKAkiGgoJUGFyYW1OYW1lEg0K'
     'BXZhbHVlGAEgASgJIhsKCkJvb2xSZXN1bHQSDQoFdmFsdWUYASABKAgiMgoGUGFyYW1z'
     'EigKBXBhcmFtGAEgAygLMhkuYXBvbGxvLmN5YmVyLnByb3RvLlBhcmFtKlEKCVBhcmFt'
     'VHlwZRILCgdOT1RfU0VUEAASCAoEQk9PTBABEgcKA0lOVBACEgoKBkRPVUJMRRADEgoK'
     'BlNUUklORxAEEgwKCFBST1RPQlVGEAU='),
)

EMBEDDED_FILES = tuple(name for name, _ in _SERIALIZED_FILES)

_POOL = descriptor_pool.DescriptorPool()
for _name, _b64 in _SERIALIZED_FILES:
    _POOL.Add(descriptor_pb2.FileDescriptorProto.FromString(
        base64.b64decode(_b64)))


def serialized_file(name):
    """Returns the embedded serialized FileDescriptorProto of `name`."""
    for file_name, b64 in _SERIALIZED_FILES:
        if file_name == name:
            return base64.b64decode(b64)
    raise KeyError(name)


def _cls(full_name):
    return _get_class(_POOL.FindMessageTypeByName(full_name))


def _enum_values(full_name):
    return {v.name: v.number
            for v in _POOL.FindEnumTypeByName(full_name).values}


# cyber/proto/record.proto
SingleIndex = _cls('apollo.cyber.proto.SingleIndex')
ChunkHeaderCache = _cls('apollo.cyber.proto.ChunkHeaderCache')
ChunkBodyCache = _cls('apollo.cyber.proto.ChunkBodyCache')
ChannelCache = _cls('apollo.cyber.proto.ChannelCache')
SingleMessage = _cls('apollo.cyber.proto.SingleMessage')
Header = _cls('apollo.cyber.proto.Header')
Channel = _cls('apollo.cyber.proto.Channel')
ChunkHeader = _cls('apollo.cyber.proto.ChunkHeader')
ChunkBody = _cls('apollo.cyber.proto.ChunkBody')
Index = _cls('apollo.cyber.proto.Index')
SectionType = _enum_values('apollo.cyber.proto.SectionType')
CompressType = _enum_values('apollo.cyber.proto.CompressType')

# cyber/proto/proto_desc.proto
ProtoDesc = _cls('apollo.cyber.proto.ProtoDesc')

# cyber/proto/qos_profile.proto, cyber/proto/role_attributes.proto
QosProfile = _cls('apollo.cyber.proto.QosProfile')
RoleAttributes = _cls('apollo.cyber.proto.RoleAttributes')
QosHistoryPolicy = _enum_values('apollo.cyber.proto.QosHistoryPolicy')
QosReliabilityPolicy = _enum_values('apollo.cyber.proto.QosReliabilityPolicy')
QosDurabilityPolicy = _enum_values('apollo.cyber.proto.QosDurabilityPolicy')

# cyber/proto/parameter.proto
Param = _cls('apollo.cyber.proto.Param')
NodeName = _cls('apollo.cyber.proto.NodeName')
ParamName = _cls('apollo.cyber.proto.ParamName')
BoolResult = _cls('apollo.cyber.proto.BoolResult')
Params = _cls('apollo.cyber.proto.Params')
ParamType = _enum_values('apollo.cyber.proto.ParamType')


def _regenerate(apollo_root):
    """Prints a fresh _SERIALIZED_FILES body generated with protoc."""
    import os
    import subprocess
    import tempfile
    import textwrap
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, 'set.desc')
        subprocess.check_call(
            ['protoc', '-I', apollo_root, '--descriptor_set_out=' + out] +
            list(EMBEDDED_FILES), cwd=apollo_root)
        with open(out, 'rb') as f:
            fds = descriptor_pb2.FileDescriptorSet.FromString(f.read())
    for fdp in fds.file:
        # protoc adds json_name to descriptor sets; CopyTo() does not.
        for msg in fdp.message_type:
            _strip_json_name(msg)
        b64 = base64.b64encode(fdp.SerializeToString()).decode()
        lines = textwrap.wrap(b64, 68)
        print("    ('%s'," % fdp.name)
        for i, line in enumerate(lines):
            print("     '%s'%s" % (line, '),' if i == len(lines) - 1 else ''))


def _strip_json_name(msg):
    for field in msg.field:
        field.ClearField('json_name')
    for ext in msg.extension:
        ext.ClearField('json_name')
    for nested in msg.nested_type:
        _strip_json_name(nested)


if __name__ == '__main__':
    import sys
    _regenerate(sys.argv[1] if len(sys.argv) > 1 else '.')
