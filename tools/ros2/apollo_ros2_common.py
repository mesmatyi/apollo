#!/usr/bin/env python3
###############################################################################
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
###############################################################################
"""Helpers shared by the Apollo -> ROS 2 conversion tools.

The naming rules here must stay identical to the C++ runtime
(cyber/ros2/naming.cc) and the Python runtime
(cyber/python/cyber_py3/naming.py).
"""

import os
import re

_TOPIC_INVALID = re.compile(r'[^A-Za-z0-9_/]')
_NODE_INVALID = re.compile(r'[^A-Za-z0-9_]')


# Topic names that ROS 2 reserves for standard message types. Apollo channels
# with these names carry protobuf payloads, so they are moved under /apollo
# (the apollo_ros2_bridge republishes standard tf2_msgs / rosgraph_msgs).
RESERVED_TOPICS = ('/tf', '/tf_static', '/clock', '/rosout', '/parameter_events')


def channel_to_topic(channel):
    """Maps a Cyber channel name onto a valid ROS 2 topic/service name."""
    if not channel:
        return channel
    name = _TOPIC_INVALID.sub('_', channel)
    if not name.startswith('/'):
        name = '/' + name
    tokens = [t for t in name.split('/') if t]
    tokens = ['_' + t if t[0].isdigit() else t for t in tokens]
    topic = '/' + '/'.join(tokens)
    if topic in RESERVED_TOPICS:
        topic = '/apollo' + topic
    return topic


def to_node_name(name):
    """Maps a Cyber node / component name onto a valid ROS 2 node name."""
    if not name:
        return 'apollo_node'
    name = _NODE_INVALID.sub('_', name)
    if name[0].isdigit():
        name = '_' + name
    return name


_REGISTER_RE = re.compile(
    r'\bCYBER_REGISTER_COMPONENT\s*\(\s*([A-Za-z_:][A-Za-z0-9_:]*(?:\s*<[A-Za-z0-9_:<>,\s]*>)?)\s*\)')
_NAMESPACE_RE = re.compile(r'\bnamespace\s+([A-Za-z_][A-Za-z0-9_:]*)?\s*\{')
_COMMENT_RE = re.compile(r'//[^\n]*|/\*.*?\*/', re.S)
_STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def _strip_comments_and_strings(text):
    def blank(m):
        # keep newlines so that offsets stay meaningful for debugging
        return re.sub(r'[^\n]', ' ', m.group(0))
    text = _STRING_RE.sub(blank, text)
    return _COMMENT_RE.sub(blank, text)


_WRAPPER_DEFINE_RE = re.compile(
    r'#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)'
    r'\s*(?:\\\s*\n\s*)?CYBER_REGISTER_COMPONENT\s*\(\s*([^()]*?)\s*\)')


def _expand_wrapper_macros(text):
    """Expands `#define WRAP(x) CYBER_REGISTER_COMPONENT(T<x>)` wrappers.

    Apollo registers templated components through such one-argument
    wrappers (see modules/bridge). Each `WRAP(arg)` invocation is rewritten
    to `CYBER_REGISTER_COMPONENT(T<arg>)` so the scanner sees it.
    """
    wrappers = {}
    for m in _WRAPPER_DEFINE_RE.finditer(text):
        wrappers[m.group(1)] = (m.group(2), m.group(3))
    if not wrappers:
        return text
    # Drop the #define lines themselves so they are not counted.
    text = _WRAPPER_DEFINE_RE.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)
    for name, (param, body) in wrappers.items():
        call_re = re.compile(r'\b%s\s*\(\s*([^()]*?)\s*\)' % re.escape(name))
        text = call_re.sub(
            lambda m: 'CYBER_REGISTER_COMPONENT(%s)' %
            re.sub(r'\b%s\b' % re.escape(param), m.group(1), body), text)
    return text


def registered_components_in_file(path):
    """Returns [(class_name, fully_qualified_name)] registered in a C++ file.

    The fully-qualified name is built from the namespaces that are open at the
    CYBER_REGISTER_COMPONENT(...) invocation, which matches the demangled
    typeid name used by the runtime registration.
    """
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            raw = f.read()
    except OSError:
        return []
    if 'CYBER_REGISTER_COMPONENT' not in raw:
        return []
    text = _expand_wrapper_macros(_strip_comments_and_strings(raw))
    # Restore the macro invocations: they are not inside strings/comments.
    results = []
    # Walk the text tracking a stack of brace scopes; namespace scopes carry
    # their name, other scopes carry None.
    stack = []
    pending_ns = None
    i = 0
    n = len(text)
    while i < n:
        if text.startswith('namespace', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            m = _NAMESPACE_RE.match(text, i)
            if m:
                pending_ns = m.group(1) or ''
                i = m.end() - 1  # position at '{'
                continue
        if text.startswith('CYBER_REGISTER_COMPONENT', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            m = _REGISTER_RE.match(text, i)
            if m:
                cls = re.sub(r'\s+', '', m.group(1))
                if cls.startswith('::'):
                    fq = cls[2:]
                else:
                    ns = [s for s in stack if s]
                    fq = '::'.join(ns + [cls])
                base = cls.split('<', 1)[0]
                short = base.split('::')[-1] + (cls[len(base):] if '<' in cls else '')
                results.append((short, fq))
                i = m.end()
                continue
        c = text[i]
        if c == '{':
            stack.append(pending_ns)
            pending_ns = None
        elif c == '}':
            if stack:
                stack.pop()
        elif c == ';':
            pending_ns = None
        i += 1
    return results


def find_registered_components(directory, recursive=False):
    """Scans the C++ sources of a directory for registered components."""
    found = []
    if recursive:
        walker = os.walk(directory)
    else:
        try:
            walker = [(directory, [], os.listdir(directory))]
        except OSError:
            return found
    for root, _, files in walker:
        for name in sorted(files):
            if name.endswith(('.h', '.hpp', '.cc', '.cpp')):
                path = os.path.join(root, name)
                for cls, fq in registered_components_in_file(path):
                    found.append((cls, fq, path))
    return found


if __name__ == '__main__':
    import sys
    for d in sys.argv[1:] or ['.']:
        for cls, fq, path in find_registered_components(d, recursive=True):
            print('%s\t%s\t%s' % (cls, fq, path))
