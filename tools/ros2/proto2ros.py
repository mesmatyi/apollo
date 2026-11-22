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
"""Generates native ROS 2 interfaces (.msg / .srv) from Apollo protobuf files.

Every Apollo channel carries a protobuf message. This tool derives one ROS 2
message type per protobuf message, so Apollo topics and services are ordinary
typed ROS 2 interfaces (`ros2 topic echo`, `ros2 interface show`, rosbag2,
Foxglove, PlotJuggler, other ROS 2 nodes) while the mapping stays lossless
for Apollo itself.

Mapping (docs/ros2/native_types.md has the full description):

  proto message  a.b.Outer.Inner      -> <pkg>/msg/BOuterInner ("apollo." dropped)
  double/float/int*/uint*/bool/string -> float64/float32/int*/uint*/bool/string
  bytes                               -> uint8[]
  enum                                -> int32, constants <FIELD>_<VALUE>
  message                             -> nested message
  repeated T                          -> T[]
  map<K, V>                           -> <Entry>[] (key, value)
  oneof members                       -> plain fields (presence bits tell which)
  optional presence (proto2, message
  fields, oneof, proto3 optional)     -> bit in `uint64 proto_has_bits`,
                                         constants HAS_<FIELD>
  recursive message reference         -> apollo_msgs/ProtoMessage (serialized)

Besides the interface files the tool writes
  registry.txt     proto name <-> ROS type, member order, presence bits and a
                   structure hash (read by the C++ and Python runtimes)
  protos.desc      FileDescriptorSet of the generated types
  interfaces.cmake list of generated files for rosidl_generate_interfaces()

Usage:
  proto2ros.py --apollo_root . --proto_list ros2/apollo_msgs/proto_files.txt \
      --services ros2/apollo_msgs/services.txt --package apollo_msgs \
      --output_dir <dir> [--protoc protoc]
"""

import argparse
import collections
import glob
import os
import re
import subprocess
import sys
import tempfile

from google.protobuf import descriptor_pb2

F = descriptor_pb2.FieldDescriptorProto

REGISTRY_VERSION = 1

SCALAR_TYPES = {
    F.TYPE_DOUBLE: 'float64', F.TYPE_FLOAT: 'float32',
    F.TYPE_INT64: 'int64', F.TYPE_SINT64: 'int64', F.TYPE_SFIXED64: 'int64',
    F.TYPE_UINT64: 'uint64', F.TYPE_FIXED64: 'uint64',
    F.TYPE_INT32: 'int32', F.TYPE_SINT32: 'int32', F.TYPE_SFIXED32: 'int32',
    F.TYPE_UINT32: 'uint32', F.TYPE_FIXED32: 'uint32',
    F.TYPE_BOOL: 'bool', F.TYPE_STRING: 'string', F.TYPE_ENUM: 'int32',
}

# Identifiers that cannot be used as field names in generated C, C++, Python
# or IDL code.
RESERVED = set("""
abstract alignas alignof and and_eq any as asm assert async attribute auto
await bitand bitfield bitmask bitor bitset bool boolean break case catch char
char16_t char32_t char8_t class co_await co_return co_yield compl component
concept connector const const_cast consteval constexpr constinit consumes
context continue custom decltype def default del delete do double
dynamic_cast elif else emits enum eventtype except exception explicit export
extern factory false finally finder fixed float for friend from getraises
global goto home if import in inline inout int int16 int32 int64 int8
interface is lambda local long manages map mirrorport module multiple mutable
namespace native new noexcept none nonlocal not not_eq nullptr object octet
oneway operator or or_eq out pass port porttype primarykey private protected
provides public publishes raise raises readonly register reinterpret_cast
requires restrict return sequence setraises short signed sizeof static
static_assert static_cast string struct supports switch template this
thread_local throw true truncatable try typedef typeid typename typeprefix
uint16 uint32 uint64 uint8 union unsigned uses using valuebase valuetype
virtual void volatile wchar wchar_t while with wstring xor xor_eq yield
""".split())

HAS_BITS_FIELD = 'proto_has_bits'
RECURSIVE_WRAPPER = 'ProtoMessage'
STATIC_INTERFACES = {'ProtoMessage', 'ProtoService'}


def log(msg):
    sys.stderr.write('proto2ros: %s\n' % msg)


def fnv1a64(text):
    h = 0xcbf29ce484222325
    for b in text.encode('utf-8'):
        h ^= b
        h = (h * 0x100000001b3) & 0xffffffffffffffff
    return '%016x' % h


def camel_to_snake(value):
    """rosidl's convert_camel_case_to_lower_case_underscore()."""
    value = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', value)
    value = re.sub('([a-z0-9])([A-Z])', r'\1_\2', value)
    return value.lower()


def camel_token(token):
    parts = [p for p in re.split(r'[^A-Za-z0-9]+', token) if p]
    return ''.join(p[0].upper() + p[1:] for p in parts)


def ros_message_name(package, relative_name):
    """apollo.planning + ADCTrajectory -> PlanningADCTrajectory."""
    tokens = [t for t in package.split('.') if t]
    if tokens and tokens[0] == 'apollo':
        tokens = tokens[1:]
    name = ''.join(camel_token(t) for t in tokens + relative_name.split('.'))
    if not name or not name[0].isalpha():
        name = 'N' + name
    return name[0].upper() + name[1:]


def sanitize_field_name(name):
    s = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name)
    s = re.sub(r'[^A-Za-z0-9]+', '_', s).lower()
    s = re.sub(r'_+', '_', s).strip('_')
    if not s or not s[0].isalpha():
        s = 'f_' + s
    if s in RESERVED or s.startswith(HAS_BITS_FIELD):
        s += '_field'
    return s


def sanitize_constant_name(name):
    s = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name)
    s = re.sub(r'[^A-Za-z0-9]+', '_', s).upper()
    s = re.sub(r'_+', '_', s).strip('_')
    if not s or not s[0].isalpha():
        s = 'C_' + s
    return s


def unique(name, used, sep='_'):
    if name not in used:
        used.add(name)
        return name
    i = 2
    while '%s%s%d' % (name, sep, i) in used:
        i += 1
    name = '%s%s%d' % (name, sep, i)
    used.add(name)
    return name


class MessageDef(object):
    """One protobuf message definition (as seen from its own file)."""

    def __init__(self, full_name, proto, file_proto, path):
        self.full_name = full_name
        self.proto = proto
        self.file = file_proto
        self.path = path  # source-info path of the message
        self.ros_name = None
        self.members = []   # filled by layout()
        self.recursive_fields = set()

    @property
    def relative_name(self):
        pkg = self.file.package
        return self.full_name[len(pkg) + 1:] if pkg else self.full_name

    def canonical(self):
        parts = [self.full_name]
        for f in self.proto.field:
            parts.append('%d,%d,%d,%s' % (f.number, f.type, f.label,
                                         f.type_name.lstrip('.')))
        return '|'.join(parts)

    def structure_hash(self):
        return fnv1a64(self.canonical())


class Schema(object):
    """All protobuf files given to the tool, merged by file name."""

    def __init__(self):
        self.files = collections.OrderedDict()   # name -> FileDescriptorProto
        self.defs = {}        # file name -> {full_name: MessageDef}
        self.enums = {}       # file name -> {full_name: EnumDescriptorProto}
        self._closure = {}

    def add_file(self, fproto):
        if fproto.name in self.files:
            return
        self.files[fproto.name] = fproto
        defs, enums = {}, {}
        prefix = fproto.package + '.' if fproto.package else ''

        def walk(m, scope, path):
            full = scope + m.name
            defs[full] = MessageDef(full, m, fproto, path)
            for i, e in enumerate(m.enum_type):
                enums[full + '.' + e.name] = e
            for i, n in enumerate(m.nested_type):
                walk(n, full + '.', path + [3, i])

        for i, m in enumerate(fproto.message_type):
            walk(m, prefix, [4, i])
        for e in fproto.enum_type:
            enums[prefix + e.name] = e
        self.defs[fproto.name] = defs
        self.enums[fproto.name] = enums

    def closure(self, file_name):
        """File names visible from `file_name` (itself + transitive imports)."""
        if file_name not in self._closure:
            seen = []
            stack = [file_name]
            visited = set()
            while stack:
                f = stack.pop()
                if f in visited or f not in self.files:
                    continue
                visited.add(f)
                seen.append(f)
                stack.extend(self.files[f].dependency)
            self._closure[file_name] = seen
        return self._closure[file_name]

    def resolve_message(self, type_name, from_file):
        type_name = type_name.lstrip('.')
        for f in self.closure(from_file):
            d = self.defs[f].get(type_name)
            if d is not None:
                return d
        return None

    def resolve_enum(self, type_name, from_file):
        type_name = type_name.lstrip('.')
        for f in self.closure(from_file):
            e = self.enums[f].get(type_name)
            if e is not None:
                return e
        return None


def run_protoc(protoc, apollo_root, proto_files):
    """Compiles the files directory by directory (Apollo has protobuf files
    that define the same symbols and cannot share one descriptor pool) and
    returns the FileDescriptorProtos in a deterministic order."""
    by_dir = collections.OrderedDict()
    for p in proto_files:
        by_dir.setdefault(os.path.dirname(p), []).append(p)
    schema = Schema()
    with tempfile.TemporaryDirectory() as tmp:
        for i, (_, files) in enumerate(by_dir.items()):
            out = os.path.join(tmp, '%d.desc' % i)
            cmd = [protoc, '-I' + apollo_root, '--include_imports',
                   '--include_source_info', '--descriptor_set_out=' + out]
            cmd += [os.path.join(apollo_root, f) for f in files]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode != 0:
                raise RuntimeError('protoc failed:\n%s' % res.stderr)
            fds = descriptor_pb2.FileDescriptorSet()
            with open(out, 'rb') as f:
                fds.ParseFromString(f.read())
            for fproto in fds.file:
                schema.add_file(fproto)
    return schema


def expand_proto_list(apollo_root, list_file):
    files = []
    seen = set()
    with open(list_file) as f:
        for raw in f:
            line = raw.split('#', 1)[0].strip()
            if not line:
                continue
            matches = sorted(glob.glob(os.path.join(apollo_root, line),
                                       recursive=True))
            if not matches:
                raise RuntimeError('%s: no file matches "%s"' % (list_file, line))
            for m in matches:
                rel = os.path.relpath(m, apollo_root)
                if rel not in seen:
                    seen.add(rel)
                    files.append(rel)
    return files


def read_services(path):
    pairs = []
    if not path:
        return pairs
    with open(path) as f:
        for raw in f:
            line = raw.split('#', 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) != 2:
                raise RuntimeError('%s: expected "<request> <response>": %s' %
                                   (path, raw.strip()))
            pairs.append((parts[0], parts[1]))
    return pairs


def select_messages(schema, root_files, service_pairs):
    """Chooses one definition per message name (first root file wins) and
    keeps only definitions whose whole structure is consistent with the
    chosen ones. Returns {full_name: MessageDef} of the transitive closure of
    the messages defined in the root files and used by the services."""
    accepted = {}       # full_name -> MessageDef
    order = list(root_files)
    for f in schema.files:   # imports after the roots, deterministic
        if f not in order:
            order.append(f)
    for fname in order:
        for full, d in schema.defs.get(fname, {}).items():
            accepted.setdefault(full, d)

    compatible_memo = {}

    def compatible(d):
        """True when `d` and everything it references (as seen from its own
        file) is structurally identical to the accepted definitions."""
        key = (d.full_name, d.file.name)
        if key in compatible_memo:
            return compatible_memo[key]
        compatible_memo[key] = True   # recursion guard (cycles are fine)
        ok = accepted[d.full_name].canonical() == d.canonical()
        if ok:
            for f in d.proto.field:
                if f.type == F.TYPE_MESSAGE or f.type == F.TYPE_GROUP:
                    ref = schema.resolve_message(f.type_name, d.file.name)
                    if ref is None or not compatible(ref):
                        ok = False
                        break
        compatible_memo[key] = ok
        return ok

    selected = collections.OrderedDict()
    rejected = []

    def add(d):
        if d.full_name in selected:
            return True
        if not compatible(d) or accepted[d.full_name] is not d and \
                not compatible(accepted[d.full_name]):
            return False
        d = accepted[d.full_name]
        if unsupported_reason(d) is not None:
            return False
        selected[d.full_name] = d
        for f in d.proto.field:
            if f.type == F.TYPE_MESSAGE:
                ref = schema.resolve_message(f.type_name, d.file.name)
                if not add(ref):
                    del selected[d.full_name]
                    return False
        return True

    for fname in root_files:
        for full, d in sorted(schema.defs.get(fname, {}).items()):
            if d.proto.options.map_entry:
                continue   # generated with the message that owns the map
            if not add(d) and full not in rejected:
                rejected.append(full)
    for req, resp in service_pairs:
        for name in (req, resp):
            if name not in accepted:
                raise RuntimeError('service type %s is not defined in any '
                                   'listed protobuf file' % name)
            if not add(accepted[name]):
                raise RuntimeError('service type %s cannot be generated' % name)
    return selected, rejected


def drop_clashing_files(selected, schema, priority):
    """All generated types must fit into one descriptor pool. When two files
    needed by the selection define the same symbol, the selection homed in
    the lower-priority file (and everything referencing it) is dropped."""
    rank = {f: i for i, f in enumerate(priority)}
    dropped = []
    while True:
        needed = []
        seen = set()

        def visit(fname):
            if fname in seen or fname not in schema.files:
                return
            seen.add(fname)
            for dep in schema.files[fname].dependency:
                visit(dep)
            needed.append(fname)

        for full in sorted(selected):
            visit(selected[full].file.name)
        needed.sort(key=lambda f: rank.get(f, len(rank)))
        defined = {}
        clash = None
        for fname in needed:
            for n in list(schema.defs[fname]) + list(schema.enums[fname]):
                if n in defined:
                    clash = fname
                    break
                defined[n] = fname
            if clash:
                break
        if clash is None:
            return needed, dropped
        gone = set(n for n, d in selected.items() if d.file.name == clash)
        if not gone:
            # the clashing file is only imported: drop what imports it
            gone = set(n for n, d in selected.items()
                       if clash in schema.closure(d.file.name))
        changed = True
        while changed:
            changed = False
            for n, d in selected.items():
                if n in gone:
                    continue
                for f in d.proto.field:
                    if f.type == F.TYPE_MESSAGE and schema.resolve_message(
                            f.type_name, d.file.name).full_name in gone:
                        gone.add(n)
                        changed = True
                        break
        for n in sorted(gone):
            del selected[n]
            dropped.append(n)


def unsupported_reason(d):
    for f in d.proto.field:
        if f.type == F.TYPE_GROUP:
            return 'group field %s' % f.name
        if f.type == F.TYPE_BYTES and f.label == F.LABEL_REPEATED:
            return 'repeated bytes field %s' % f.name
    if d.proto.extension_range:
        return None   # extensions are dropped, the message itself is fine
    return None


def find_recursive_fields(selected, schema):
    """Marks fields that close a cycle in the message graph (ROS 2 interfaces
    cannot be recursive); they are carried as serialized protobuf."""
    graph = {}
    for full, d in selected.items():
        graph[full] = [schema.resolve_message(f.type_name, d.file.name).full_name
                       for f in d.proto.field if f.type == F.TYPE_MESSAGE]
    index, low, comp = {}, {}, {}
    stack, on_stack = [], set()
    counter = [0]

    def strongconnect(v):
        # iterative Tarjan
        work = [(v, 0)]
        while work:
            node, i = work.pop()
            if i == 0:
                index[node] = low[node] = counter[0]
                counter[0] += 1
                stack.append(node)
                on_stack.add(node)
            recurse = False
            succ = graph[node]
            while i < len(succ):
                w = succ[i]
                i += 1
                if w not in index:
                    work.append((node, i))
                    work.append((w, 0))
                    recurse = True
                    break
                if w in on_stack:
                    low[node] = min(low[node], index[w])
            if recurse:
                continue
            if low[node] == index[node]:
                members = []
                while True:
                    w = stack.pop()
                    on_stack.discard(w)
                    members.append(w)
                    if w == node:
                        break
                for w in members:
                    comp[w] = node
            if work:
                parent = work[-1][0]
                low[parent] = min(low[parent], low[node])

    for v in sorted(graph):
        if v not in index:
            strongconnect(v)
    sizes = collections.Counter(comp.values())
    for full, d in selected.items():
        for f in d.proto.field:
            if f.type != F.TYPE_MESSAGE:
                continue
            target = schema.resolve_message(f.type_name, d.file.name).full_name
            if comp[target] == comp[full] and (sizes[comp[full]] > 1 or
                                               target == full):
                d.recursive_fields.add(f.number)


def has_presence(f, file_proto):
    if f.label == F.LABEL_REPEATED or f.label == F.LABEL_REQUIRED:
        return False
    if f.type == F.TYPE_MESSAGE or f.HasField('oneof_index'):
        return True
    if (file_proto.syntax or 'proto2') == 'proto2':
        return True
    return f.proto3_optional


def assign_names(selected):
    used = set(STATIC_INTERFACES)
    used_snake = set(camel_to_snake(n) for n in used)
    for full in sorted(selected):
        d = selected[full]
        base = ros_message_name(d.file.package, d.relative_name)
        name = base
        i = 2
        while name in used or camel_to_snake(name) in used_snake:
            name = '%s%d' % (base, i)
            i += 1
        used.add(name)
        used_snake.add(camel_to_snake(name))
        d.ros_name = name
    return used, used_snake


def comment_lines(text, indent=''):
    lines = []
    for raw in text.strip('\n').split('\n'):
        raw = raw.rstrip()
        # keep the files ASCII: rosidl reads them with the default encoding
        raw = raw.encode('ascii', 'replace').decode('ascii')
        lines.append((indent + '# ' + raw.strip()).rstrip())
    return lines


def source_comments(file_proto):
    table = {}
    if not file_proto.HasField('source_code_info'):
        return table
    for loc in file_proto.source_code_info.location:
        text = loc.leading_comments or loc.trailing_comments
        if text.strip():
            table[tuple(loc.path)] = text
    return table


def default_literal(f, schema, d):
    """ROS default value for a singular field with a protobuf default."""
    if not f.HasField('default_value') or f.label == F.LABEL_REPEATED:
        return None
    v = f.default_value
    if f.type in (F.TYPE_DOUBLE, F.TYPE_FLOAT):
        try:
            x = float(v)
        except ValueError:
            return None
        if x != x or x in (float('inf'), float('-inf')) or x == 0.0:
            return None   # nan/inf cannot be written in .msg files
        return repr(x)
    if f.type == F.TYPE_BOOL:
        return 'true' if v == 'true' else None
    if f.type == F.TYPE_STRING:
        if not v or any(ord(c) < 32 or ord(c) > 126 for c in v) or '"' in v \
                or '\\' in v:
            return None
        return '"%s"' % v
    if f.type == F.TYPE_ENUM:
        enum = schema.resolve_enum(f.type_name, d.file.name)
        for value in enum.value:
            if value.name == v:
                return str(value.number) if value.number != 0 else None
        return None
    if f.type == F.TYPE_BYTES:
        return None
    try:
        n = int(v)
    except ValueError:
        return None
    return str(n) if n != 0 else None


def layout(d, schema, selected):
    """Computes the ROS members of a message. Returns (lines, members)."""
    comments = source_comments(d.file)
    used_fields = set()
    used_consts = set()
    lines = []
    members = []      # (kind, ros_name, number_or_word, bit)
    constants = []
    presence = []
    bit = 0
    for i, f in enumerate(d.proto.field):
        name = unique(sanitize_field_name(f.name), used_fields)
        comment = comments.get(tuple(d.path + [2, i]))
        if comment:
            lines.extend(comment_lines(comment))
        kind = 'f'
        if f.type == F.TYPE_MESSAGE:
            if f.number in d.recursive_fields:
                ros_type = RECURSIVE_WRAPPER
                kind = 'r'
                lines.append('# serialized %s (recursive type)' %
                             f.type_name.lstrip('.'))
            else:
                ref = schema.resolve_message(f.type_name, d.file.name)
                ros_type = selected[ref.full_name].ros_name
        elif f.type == F.TYPE_BYTES:
            ros_type = 'uint8[]'
        else:
            ros_type = SCALAR_TYPES[f.type]
        if f.label == F.LABEL_REPEATED:
            ros_type += '[]'
        default = default_literal(f, schema, d)
        lines.append('%s %s%s' % (ros_type, name,
                                  ' ' + default if default else ''))
        if f.type == F.TYPE_ENUM:
            enum = schema.resolve_enum(f.type_name, d.file.name)
            prefix = sanitize_constant_name(name)
            for value in enum.value:
                cname = unique('%s_%s' % (prefix,
                                          sanitize_constant_name(value.name)),
                               used_consts)
                constants.append('int32 %s=%d' % (cname, value.number))
        fbit = -1
        if has_presence(f, d.file):
            fbit = bit
            bit += 1
            presence.append((name, fbit))
        members.append((kind, name, f.number, fbit))
    words = (bit + 63) // 64
    has_names = []
    for w in range(words):
        has_names.append(unique(HAS_BITS_FIELD if w == 0 else
                                '%s_%d' % (HAS_BITS_FIELD, w), used_fields))
    if constants:
        lines.append('')
        lines.append('# Enum values')
        lines.extend(constants)
    if words:
        lines.append('')
        lines.append('# Presence of optional protobuf fields: bit set = field '
                     'present.')
        for name, fbit in presence:
            w = fbit // 64
            prefix = 'HAS' if w == 0 else 'HAS_%d' % w
            cname = unique('%s_%s' % (prefix, sanitize_constant_name(name)),
                           used_consts)
            lines.append('uint64 %s=%d' % (cname, 1 << (fbit % 64)))
        for w, hname in enumerate(has_names):
            lines.append('uint64 %s' % hname)
            members.append(('h', hname, w, -1))
    return lines, members


_CONSTANT_LINE = re.compile(r'^[a-z0-9]+ [A-Z][A-Z0-9_]*=')


def distinct_constants(response_lines, request_lines):
    """rosidl_parser attributes a constant of the response to the request
    when both halves of a .srv declare a textually identical constant (it
    locates declarations by structural equality). A comment makes the
    response declarations distinct."""
    request_constants = set(l for l in request_lines if _CONSTANT_LINE.match(l))
    out = []
    for line in response_lines:
        if line in request_constants:
            out.append('# (response)')
        out.append(line)
    return out


def write_if_changed(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if os.path.exists(path):
        with open(path) as f:
            if f.read() == content:
                return False
    with open(path, 'w') as f:
        f.write(content)
    return True


def generate(args):
    proto_files = expand_proto_list(args.apollo_root, args.proto_list)
    service_pairs = read_services(args.services)
    schema = run_protoc(args.protoc, args.apollo_root, proto_files)
    selected, rejected = select_messages(schema, proto_files, service_pairs)
    priority = list(proto_files) + [f for f in schema.files
                                    if f not in set(proto_files)]
    needed, dropped = drop_clashing_files(selected, schema, priority)
    rejected = sorted(set(rejected) | set(dropped))
    for req, resp in service_pairs:
        for name in (req, resp):
            if name not in selected:
                raise RuntimeError('service type %s cannot be generated' % name)
    find_recursive_fields(selected, schema)
    used, used_snake = assign_names(selected)

    out = args.output_dir
    msg_dir = os.path.join(out, 'msg')
    srv_dir = os.path.join(out, 'srv')
    expected = set()
    registry = ['# Apollo protobuf <-> ROS 2 interface registry. Generated by '
                'tools/ros2/proto2ros.py; do not edit.',
                'V %d' % REGISTRY_VERSION]
    bodies = {}
    interfaces = []
    comments_cache = {}
    for full in sorted(selected):
        d = selected[full]
        body, members = layout(d, schema, selected)
        d.members = members
        bodies[full] = body
        header = ['# %s' % full,
                  '# Generated from %s by tools/ros2/proto2ros.py. Do not '
                  'edit.' % d.file.name]
        if d.file.name not in comments_cache:
            comments_cache[d.file.name] = source_comments(d.file)
        doc = comments_cache[d.file.name].get(tuple(d.path))
        if doc:
            header.append('#')
            header.extend(comment_lines(doc))
        content = '\n'.join(header + [''] + body) + '\n'
        rel = 'msg/%s.msg' % d.ros_name
        write_if_changed(os.path.join(out, rel), content)
        expected.add(os.path.join(out, rel))
        interfaces.append(rel)
        registry.append('M %s %s/msg/%s %s' % (full, args.package, d.ros_name,
                                               d.structure_hash()))
        for kind, name, number, fbit in members:
            registry.append('%s %s %d %d' % (kind, name, number, fbit))

    srv_names = set()
    for req, resp in service_pairs:
        base = selected[req].ros_name
        name = base
        if name in srv_names or name in STATIC_INTERFACES:
            name = unique(base + 'To' + selected[resp].ros_name, srv_names, '')
        srv_names.add(name)
        content = '\n'.join(
            ['# Request: %s' % req] + bodies[req] + ['---'] +
            ['# Response: %s' % resp] +
            distinct_constants(bodies[resp], bodies[req])) + '\n'
        rel = 'srv/%s.srv' % name
        write_if_changed(os.path.join(out, rel), content)
        expected.add(os.path.join(out, rel))
        interfaces.append(rel)
        registry.append('S %s %s %s/srv/%s' % (req, resp, args.package, name))

    # remove interface files of types that no longer exist
    for directory in (msg_dir, srv_dir):
        if os.path.isdir(directory):
            for name in os.listdir(directory):
                path = os.path.join(directory, name)
                if path not in expected:
                    os.remove(path)

    write_if_changed(os.path.join(out, 'registry.txt'),
                     '\n'.join(registry) + '\n')

    # Descriptor set of the files that define the generated messages, in
    # dependency order, without source info.
    ordered = []
    seen = set()

    def visit(fname):
        if fname in seen or fname not in schema.files:
            return
        seen.add(fname)
        for dep in schema.files[fname].dependency:
            visit(dep)
        ordered.append(fname)

    for fname in needed:
        visit(fname)
    fds = descriptor_pb2.FileDescriptorSet()
    for fname in ordered:
        copy = fds.file.add()
        copy.CopyFrom(schema.files[fname])
        copy.ClearField('source_code_info')
    desc_path = os.path.join(out, 'protos.desc')
    data = fds.SerializeToString(deterministic=True)
    if not os.path.exists(desc_path) or open(desc_path, 'rb').read() != data:
        with open(desc_path, 'wb') as f:
            f.write(data)

    cmake = ['# Generated by tools/ros2/proto2ros.py; do not edit.',
             'set(APOLLO_PROTO_INTERFACES']
    cmake += ['  "%s:%s"' % (out, rel) for rel in interfaces]
    cmake.append(')')
    cmake.append('set(APOLLO_PROTO_INTERFACE_INPUTS')
    cmake += ['  "%s"' % os.path.join(args.apollo_root, f) for f in proto_files]
    cmake.append(')')
    write_if_changed(os.path.join(out, 'interfaces.cmake'),
                     '\n'.join(cmake) + '\n')

    log('%d messages and %d services generated from %d protobuf files '
        '(%d recursive fields carried as serialized protobuf)' % (
            len(selected), len(service_pairs), len(proto_files),
            sum(len(d.recursive_fields) for d in selected.values())))
    if rejected:
        log('%d messages keep the protobuf wire format because their '
            'definition conflicts with another file or is unsupported: %s%s'
            % (len(rejected), ', '.join(rejected[:8]),
               ', ...' if len(rejected) > 8 else ''))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--apollo_root', required=True)
    parser.add_argument('--proto_list', required=True,
                        help='file with protobuf paths/globs relative to '
                             'apollo_root; earlier entries win conflicts')
    parser.add_argument('--services', default='',
                        help='file with "<request type> <response type>" lines')
    parser.add_argument('--package', default='apollo_msgs')
    parser.add_argument('--output_dir', required=True)
    parser.add_argument('--protoc', default='protoc')
    args = parser.parse_args()
    args.apollo_root = os.path.abspath(args.apollo_root)
    args.output_dir = os.path.abspath(args.output_dir)
    return generate(args)


if __name__ == '__main__':
    sys.exit(main())
