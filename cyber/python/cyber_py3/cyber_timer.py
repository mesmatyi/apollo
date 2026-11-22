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
"""Cyber Timer in pure Python.

Each started timer runs its callback on its own daemon thread. Like a Cyber
timer the first call happens one period after start(), periods are kept on a
monotonic schedule (no drift; ticks missed by a slow callback are skipped,
not queued), and cyber.shutdown() stops every timer.
"""

import threading
import time
import traceback
import weakref

_timers = weakref.WeakSet()
_timers_lock = threading.Lock()


def _stop_all():
    """Stops every live timer (called by cyber.shutdown())."""
    with _timers_lock:
        timers = list(_timers)
    for timer in timers:
        timer.stop()


class Timer(object):

    """
    Class for cyber timer wrapper.
    """

    ##
    # @brief Used to perform oneshot or periodic timing tasks
    #
    # @param period The period of the timer, unit is ms.
    # @param callback The tasks that the timer needs to perform.
    # @param oneshot 1:perform the callback only after the first timing cycle
    # 0:perform the callback every timed period
    def __init__(self, period=None, callback=None, oneshot=None):
        self._lock = threading.Lock()
        self._thread = None
        self._stop_event = None
        self._period_ms = 0
        self._callback = None
        self._oneshot = False
        if not (period is None and callback is None and oneshot is None):
            self.set_option(period, callback, oneshot or 0)
        with _timers_lock:
            _timers.add(self)

    def __del__(self):
        event = getattr(self, '_stop_event', None)
        if event is not None:
            event.set()

    ##
    # @brief set the option of timer.
    #
    # @param period The period of the timer, unit is ms.
    # @param callback The tasks that the timer needs to perform.
    # @param oneshot 1:perform the callback only after the first timing cycle
    # 0:perform the callback every timed period
    def set_option(self, period, callback, oneshot=0):
        with self._lock:
            self._period_ms = int(period or 0)
            self._callback = callback
            self._oneshot = bool(oneshot)

    ##
    # @brief start the timer
    def start(self):
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return
            if self._period_ms <= 0 or self._callback is None:
                return
            self._stop_event = threading.Event()
            self._thread = threading.Thread(
                target=self._run,
                args=(self._stop_event, self._period_ms / 1000.0,
                      self._callback, self._oneshot),
                name='cyber_timer', daemon=True)
            self._thread.start()

    ##
    # @brief stop the timer
    def stop(self):
        with self._lock:
            thread, event = self._thread, self._stop_event
            self._thread = None
        if event is not None:
            event.set()
        if thread is not None and thread is not threading.current_thread():
            thread.join(timeout=1.0)

    @staticmethod
    def _run(stop_event, period, callback, oneshot):
        deadline = time.monotonic() + period
        while not stop_event.wait(max(0.0, deadline - time.monotonic())):
            try:
                callback()
            except Exception:  # pylint: disable=broad-except
                traceback.print_exc()
            if oneshot:
                return
            deadline += period
            now = time.monotonic()
            if deadline < now:
                deadline = now
