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
"""Module for test cyber timer."""

import time

from conftest import requires_ros

from cyber.python.cyber_py3 import cyber_timer


class Counter(object):

    def __init__(self):
        self.count = 0

    def func(self):
        self.count += 1


def test_timer():
    counter = Counter()
    ct = cyber_timer.Timer(100, counter.func, 0)  # 100ms
    ct.start()
    time.sleep(1.05)
    ct.stop()
    assert 8 <= counter.count <= 11
    stopped_at = counter.count
    time.sleep(0.25)
    assert counter.count == stopped_at


def test_set_option():
    counter = Counter()
    ct2 = cyber_timer.Timer()
    ct2.set_option(100, counter.func, 0)
    ct2.start()
    time.sleep(0.55)
    ct2.stop()
    assert 4 <= counter.count <= 6


def test_oneshot():
    counter = Counter()
    ct = cyber_timer.Timer(50, counter.func, 1)
    ct.start()
    time.sleep(0.3)
    assert counter.count == 1
    ct.stop()


def test_callback_exception_keeps_timer_alive():
    calls = []

    def bad():
        calls.append(1)
        raise RuntimeError('boom')

    ct = cyber_timer.Timer(30, bad, 0)
    ct.start()
    time.sleep(0.2)
    ct.stop()
    assert len(calls) >= 3


@requires_ros
def test_shutdown_stops_timers():
    from cyber.python.cyber_py3 import cyber
    assert cyber.init()
    counter = Counter()
    ct = cyber_timer.Timer(20, counter.func, 0)
    ct.start()
    time.sleep(0.1)
    cyber.shutdown()
    stopped_at = counter.count
    time.sleep(0.15)
    assert counter.count == stopped_at > 0
