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
"""init / ok / shutdown / signals."""

import signal
import time

import pytest

from conftest import requires_ros
from conftest import run_python
from conftest import spawn_python

pytestmark = requires_ros


def test_init():
    from cyber.python.cyber_py3 import cyber
    assert cyber.init()
    assert cyber.init()          # idempotent
    assert cyber.ok()
    assert not cyber.is_shutdown()
    cyber.shutdown()
    assert cyber.is_shutdown()
    assert not cyber.ok()
    cyber.shutdown()             # idempotent


def test_reinit_after_shutdown():
    from cyber.python.cyber_py3 import cyber
    assert cyber.init()
    cyber.Node('first_life')
    cyber.shutdown()
    assert cyber.init()
    assert cyber.ok() and not cyber.is_shutdown()
    cyber.Node('second_life')
    cyber.shutdown()


def test_state_before_init():
    result = run_python(
        'from cyber.python.cyber_py3 import cyber\n'
        'print(cyber.ok(), cyber.is_shutdown())')
    assert result.returncode == 0, result.stderr
    assert result.stdout.split() == ['False', 'False']


SIGINT_SCRIPT = '''
import sys, time
from cyber.python.cyber_py3 import cyber
cyber.init()
node = cyber.Node("sigint_test")
print("ready", flush=True)
%s
print("exited loop", cyber.is_shutdown(), flush=True)
cyber.shutdown()
print("clean exit", flush=True)
'''


@pytest.mark.parametrize('loop', [
    'while not cyber.is_shutdown():\n    time.sleep(0.05)',
    'node.spin()',
    'cyber.waitforshutdown()',
])
def test_sigint_sets_shutdown(loop):
    """Like Cyber, Ctrl-C ends `while not cyber.is_shutdown()` loops,
    node.spin() and waitforshutdown() without a KeyboardInterrupt."""
    proc = spawn_python(SIGINT_SCRIPT % loop)
    try:
        assert proc.stdout.readline().strip() == 'ready'
        time.sleep(0.2)
        proc.send_signal(signal.SIGINT)
        out, err = proc.communicate(timeout=20)
    finally:
        proc.kill()
    assert proc.returncode == 0, err
    assert 'exited loop True' in out
    assert 'clean exit' in out
    assert 'KeyboardInterrupt' not in err


def test_second_sigint_raises_keyboard_interrupt():
    script = SIGINT_SCRIPT % (
        'try:\n'
        '    while True:\n'
        '        time.sleep(0.05)\n'
        'except KeyboardInterrupt:\n'
        '    print("interrupted", cyber.is_shutdown(), flush=True)')
    proc = spawn_python(script)
    try:
        assert proc.stdout.readline().strip() == 'ready'
        time.sleep(0.2)
        proc.send_signal(signal.SIGINT)
        time.sleep(0.3)
        proc.send_signal(signal.SIGINT)
        out, err = proc.communicate(timeout=20)
    finally:
        proc.kill()
    assert proc.returncode == 0, err
    assert 'interrupted True' in out
