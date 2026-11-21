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
"""Module for test cyber_time."""

import time

from cyber.python.cyber_py3 import cyber_time


def test_time():
    ct = cyber_time.Time(123)
    assert ct.to_nsec() == 123
    assert abs(ct.now().to_sec() - time.time()) < 1.0
    assert cyber_time.Time.mono_time().to_nsec() > 0

    td1 = cyber_time.Duration(111)
    tm1 = ct - td1
    assert isinstance(tm1, cyber_time.Time)
    assert tm1.to_nsec() == 12
    assert isinstance(ct - tm1, cyber_time.Duration)
    assert (ct + td1).to_nsec() == 234

    tm5 = cyber_time.Time(1.8)
    assert not tm5.iszero()
    assert tm5.to_nsec() == 1800000000
    tm7 = cyber_time.Time(tm5)
    assert tm7.to_nsec() == 1800000000
    assert tm7 == tm5 and tm1 < tm5 and tm5 >= tm7
    assert str(tm7) == '1800000000'


def test_sleep_until():
    target = cyber_time.Time.now() + cyber_time.Duration(0.05)
    cyber_time.Time(0).sleep_until(target)
    assert cyber_time.Time.now() >= target


def test_duration():
    td1 = cyber_time.Duration(111)
    td2 = cyber_time.Duration(601000000000)
    td3 = td2 - td1
    assert td1.to_nsec() == 111
    assert td2.to_nsec() == 601000000000
    assert td3.to_nsec() == 601000000000 - 111
    assert td2.to_sec() == 601.0
    assert not td2.iszero()
    assert str(td2) == '601000000000'
    td5 = cyber_time.Duration(1.8)
    assert td5.to_nsec() == 1800000000
    assert isinstance(cyber_time.Duration(td5), cyber_time.Duration)
    start = time.monotonic()
    cyber_time.Duration(0.05).sleep()
    assert time.monotonic() - start >= 0.045


def test_rate():
    rt1 = cyber_time.Rate(111)
    assert rt1.get_cycle_time().to_nsec() == 0
    assert rt1.get_expected_cycle_time().to_nsec() == 111
    # float = frequency in Hz (cyber::Rate(double)); 0.2 Hz -> 5 s
    rt2 = cyber_time.Rate(0.2)
    assert rt2.get_cycle_time().to_nsec() == 0
    assert rt2.get_expected_cycle_time().to_nsec() == 5000000000
    rt3 = cyber_time.Rate(cyber_time.Duration(666))
    assert rt3.get_cycle_time().to_nsec() == 0
    assert rt3.get_expected_cycle_time().to_nsec() == 666


def test_rate_sleep():
    rate = cyber_time.Rate(50.0)   # 20 ms
    start = time.monotonic()
    for _ in range(10):
        rate.sleep()
    elapsed = time.monotonic() - start
    assert 0.17 < elapsed < 0.4
    assert rate.get_cycle_time().to_nsec() < 20000000
