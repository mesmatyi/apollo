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
"""Cyber Time / Duration / Rate in pure Python (cyber/time/*.cc semantics).

Time is nanoseconds since the Unix epoch (system clock, like
cyber::Time::Now()); Duration is a signed nanosecond count.
"""

import time as _time

_NSEC_PER_SEC = 1000000000


def _sleep_ns(nanoseconds):
    if nanoseconds > 0:
        _time.sleep(nanoseconds / float(_NSEC_PER_SEC))


class Duration(object):

    """
    Class for cyber Duration wrapper.
    """

    ##
    # @brief Constructor, creates a Duration.
    #
    # @param other float means seconds unit, int means nanoseconds,
    # Duration copies.
    def __init__(self, other):
        if isinstance(other, Duration):
            self.nanoseconds_ = other.nanoseconds_
        elif isinstance(other, float):
            self.nanoseconds_ = int(other * _NSEC_PER_SEC)
        elif isinstance(other, int):
            self.nanoseconds_ = int(other)
        else:
            raise TypeError('Duration expects int (ns), float (s) or Duration,'
                            ' got %s' % type(other).__name__)

    def sleep(self):
        """
        sleep for the amount of time specified by the duration.
        """
        _sleep_ns(self.nanoseconds_)

    def __str__(self):
        return str(self.nanoseconds_)

    def __repr__(self):
        return 'Duration(%d)' % self.nanoseconds_

    def to_sec(self):
        """
        convert to second.
        """
        return float(self.nanoseconds_) / _NSEC_PER_SEC

    def to_nsec(self):
        """
        convert to nanosecond.
        """
        return self.nanoseconds_

    def iszero(self):
        return self.nanoseconds_ == 0

    def __add__(self, other):
        return Duration(self.nanoseconds_ + other.nanoseconds_)

    def __radd__(self, other):
        return Duration(self.nanoseconds_ + other.nanoseconds_)

    def __sub__(self, other):
        return Duration(self.nanoseconds_ - other.nanoseconds_)

    def __lt__(self, other):
        return self.nanoseconds_ < other.nanoseconds_

    def __gt__(self, other):
        return self.nanoseconds_ > other.nanoseconds_

    def __le__(self, other):
        return self.nanoseconds_ <= other.nanoseconds_

    def __ge__(self, other):
        return self.nanoseconds_ >= other.nanoseconds_

    def __eq__(self, other):
        return isinstance(other, Duration) and \
            self.nanoseconds_ == other.nanoseconds_

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(self.nanoseconds_)


class Time(object):

    """
    Class for cyber time wrapper.
    """

    ##
    # @brief Constructor, creates a Time.
    #
    # @param other float means seconds unit.
    # int means nanoseconds.
    def __init__(self, other):
        nanoseconds = 0
        if isinstance(other, Time):
            nanoseconds = other.to_nsec()
        elif isinstance(other, float):
            nanoseconds = other * _NSEC_PER_SEC
        elif isinstance(other, int):
            nanoseconds = other
        # cyber::Time stores an uint64_t
        self.time = int(nanoseconds) & 0xFFFFFFFFFFFFFFFF

    def __str__(self):
        return str(self.to_nsec())

    def __repr__(self):
        return 'Time(%d)' % self.time

    def iszero(self):
        return self.to_nsec() == 0

    @staticmethod
    def now():
        """
        return current time.
        """
        return Time(_time.time_ns())

    @staticmethod
    def mono_time():
        return Time(_time.monotonic_ns())

    def to_sec(self):
        """
        convert to second.
        """
        return float(self.time) / _NSEC_PER_SEC

    def to_nsec(self):
        """
        convert to nanosecond.
        """
        return self.time

    def sleep_until(self, cyber_time):
        """
        sleep until time.
        """
        if isinstance(cyber_time, Time):
            _sleep_ns(cyber_time.to_nsec() - _time.time_ns())
            return None
        return NotImplemented

    def __sub__(self, other):
        if isinstance(other, Time):
            return Duration(self.to_nsec() - other.to_nsec())
        return Time(self.to_nsec() - other.to_nsec())

    def __add__(self, other):
        return Time(self.to_nsec() + other.to_nsec())

    def __radd__(self, other):
        return Time(self.to_nsec() + other.to_nsec())

    def __lt__(self, other):
        return self.to_nsec() < other.to_nsec()

    def __gt__(self, other):
        return self.to_nsec() > other.to_nsec()

    def __le__(self, other):
        return self.to_nsec() <= other.to_nsec()

    def __ge__(self, other):
        return self.to_nsec() >= other.to_nsec()

    def __eq__(self, other):
        return isinstance(other, Time) and self.to_nsec() == other.to_nsec()

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(self.time)


class Rate(object):

    """
    Class for cyber Rate wrapper. Help run loops at a desired frequency.
    """

    ##
    # @brief Constructor, creates a Rate.
    #
    # @param other float means frequency the desired rate to run at in Hz.
    # int means the expected_cycle_time in nanoseconds.
    # Duration means the expected_cycle_time.
    def __init__(self, other):
        if isinstance(other, Duration):
            expected = other.to_nsec()
        elif isinstance(other, float):
            # cyber::Rate(double frequency): expected cycle = 1 / frequency s
            expected = int(_NSEC_PER_SEC / other)
        elif isinstance(other, int):
            expected = other
        else:
            raise TypeError('Rate expects float (Hz), int (ns) or Duration')
        self._start = Time.now()
        self._expected_cycle_time = Duration(int(expected))
        self._actual_cycle_time = Duration(0)

    def __str__(self):
        return "cycle_time = %s, exp_cycle_time = %s" % (
            str(self.get_cycle_time()), str(self.get_expected_cycle_time()))

    def sleep(self):
        """
        Sleeps for any leftover time in a cycle.
        """
        expected_end = self._start + self._expected_cycle_time
        actual_end = Time.now()
        # detect backward jumps in time
        if actual_end < self._start:
            expected_end = actual_end + self._expected_cycle_time
        sleep_time = expected_end.to_nsec() - actual_end.to_nsec()
        self._actual_cycle_time = actual_end - self._start
        self._start = expected_end
        if sleep_time < 0:
            # forward jump or the loop took more than a full extra cycle
            if actual_end > expected_end + self._expected_cycle_time:
                self._start = actual_end
            return
        _sleep_ns(sleep_time)

    def reset(self):
        """
        Sets the start time for the rate to now.
        """
        self._start = Time.now()

    def get_cycle_time(self):
        """
        Get the actual run time of a cycle from start to sleep.
        """
        return Duration(self._actual_cycle_time)

    def get_expected_cycle_time(self):
        """
        Get the expected cycle time.
        """
        return Duration(self._expected_cycle_time)
