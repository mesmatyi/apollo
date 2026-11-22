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
"""The descriptors embedded in _cyber_protos match cyber/proto/*.proto."""

import importlib

import pytest

from conftest import requires_cyber_protos

from cyber.python.cyber_py3 import _cyber_protos


@requires_cyber_protos
@pytest.mark.parametrize('file_name', _cyber_protos.EMBEDDED_FILES)
def test_embedded_descriptor_is_up_to_date(file_name):
    module = importlib.import_module(
        file_name[:-len('.proto')].replace('/', '.') + '_pb2')
    assert _cyber_protos.serialized_file(file_name) == \
        module.DESCRIPTOR.serialized_pb, (
            '%s changed: regenerate _cyber_protos._SERIALIZED_FILES with '
            '"python3 -m cyber.python.cyber_py3._cyber_protos <apollo>"' %
            file_name)


@requires_cyber_protos
def test_wire_compatible_with_generated_classes():
    from cyber.proto import record_pb2
    header = _cyber_protos.Header(major_version=1, chunk_number=3,
                                  is_complete=True)
    parsed = record_pb2.Header.FromString(header.SerializeToString())
    assert parsed.chunk_number == 3 and parsed.is_complete
    assert _cyber_protos.SectionType['SECTION_CHANNEL'] == \
        record_pb2.SECTION_CHANNEL
