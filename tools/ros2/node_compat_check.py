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
"""ROS 2 compatibility matrix of the Apollo nodes.

Starts every dag file of the source tree with `mainboard`, each in its own
ROS domain, and looks at the result the way any ROS 2 tool would: which nodes
appear, the types of their topics and services (native Apollo types generated
from the protobuf messages, the serialized-protobuf fallback, or other ROS 2
types), and whether the standard parameter services answer.

  node_compat_check.py --apollo_root . --json matrix.json --markdown matrix.md
  node_compat_check.py --apollo_root . --dag modules/planning/planning_component/dag/planning.dag

The ROS 2 environment with the cyber, apollo_msgs and apollo packages must be
sourced. A dag whose library was not built (CUDA, vendor SDKs, ...) is
reported as "not built"; a component that needs hardware or data that is not
there is reported with the error it logged.
"""

import argparse
import concurrent.futures
import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

PROBE = r'''
import json, sys, time
import rclpy
rclpy.init()
node = rclpy.create_node('_compat_probe')
deadline = time.monotonic() + float(sys.argv[1])
names = []
while time.monotonic() < deadline:
    rclpy.spin_once(node, timeout_sec=0.2)
result = {'nodes': {}}
for name, namespace in node.get_node_names_and_namespaces():
    if name.startswith('_'):
        continue
    full = (namespace.rstrip('/') + '/' + name)
    def entries(query):
        try:
            return {n: list(t) for n, t in query(name, namespace)}
        except Exception:
            return {}
    result['nodes'][full] = {
        'publishers': entries(node.get_publisher_names_and_types_by_node),
        'subscribers': entries(node.get_subscriber_names_and_types_by_node),
        'services': entries(node.get_service_names_and_types_by_node),
    }
print(json.dumps(result))
node.destroy_node()
rclpy.shutdown()
'''

# Topics and services every ROS 2 node has; not part of Apollo's interface.
STANDARD_TOPICS = ('/rosout', '/parameter_events', '/clock')
STANDARD_SERVICE_SUFFIXES = (
    '/describe_parameters', '/get_parameter_types', '/get_parameters',
    '/list_parameters', '/set_parameters', '/set_parameters_atomically',
    '/get_type_description')
PARAMETER_SERVICES = ('/list_parameters', '/get_parameters',
                      '/set_parameters', '/describe_parameters')


def find_mainboard():
    path = shutil.which('mainboard')
    if path:
        return path
    try:
        import ament_index_python
        prefix = ament_index_python.get_package_prefix('cyber')
        candidate = os.path.join(prefix, 'lib', 'cyber', 'mainboard')
        if os.access(candidate, os.X_OK):
            return candidate
    except Exception:  # pylint: disable=broad-except
        pass
    return None


def dag_summary(path):
    """Component classes and node names a dag file declares."""
    try:
        with open(path, errors='replace') as f:
            text = f.read()
    except OSError:
        return [], []
    classes = re.findall(r'class_name\s*:\s*"([^"]+)"', text)
    names = re.findall(r'\bname\s*:\s*"([^"]+)"', text)
    return classes, names


def classify(types, registry):
    """'native', 'proto' (serialized protobuf fallback) or 'ros' (any other
    ROS 2 type) for the types of one topic or service."""
    kinds = set()
    for ros_type in types:
        if ros_type in ('apollo_msgs/msg/ProtoMessage',
                        'apollo_msgs/srv/ProtoService'):
            kinds.add('proto')
        elif registry.find_by_ros_type(ros_type) is not None or \
                registry.find_service_by_ros_type(ros_type) is not None:
            kinds.add('native')
        else:
            kinds.add('ros')
    for kind in ('proto', 'native', 'ros'):
        if kind in kinds:
            return kind
    return 'ros'


def error_lines(log_path, limit=3):
    """The first error lines of a glog/stderr log (file:line stripped)."""
    lines = []
    try:
        with open(log_path, errors='replace') as f:
            for line in f:
                if re.match(r'^[EF]\d{8} ', line) or 'error' in line.lower():
                    text = re.sub(r'^[EF]\d{8} [\d:.]+ +\d+ ', '', line.strip())
                    if text not in lines:
                        lines.append(text[:240])
                    if len(lines) >= limit:
                        break
    except OSError:
        pass
    return lines


def run_dag(dag, args, slot, registry):
    """Starts one dag and inspects the graph. Returns the matrix row."""
    domain = args.domain_id_base + slot
    relative = os.path.relpath(dag, args.apollo_root)
    classes, names = dag_summary(dag)
    row = {'dag': relative, 'components': classes, 'status': 'ok',
           'nodes': [], 'writers': {}, 'readers': {}, 'services': {},
           'parameter_services': None, 'errors': []}
    work = tempfile.mkdtemp(prefix='apollo_compat_')
    log_path = os.path.join(work, 'mainboard.log')
    env = dict(os.environ, ROS_DOMAIN_ID=str(domain),
               GLOG_alsologtostderr='1', APOLLO_ROOT=args.apollo_root)
    if args.rmw:
        env['RMW_IMPLEMENTATION'] = args.rmw
    process_node = 'compat_process_%d' % slot
    with open(log_path, 'w') as log:
        process = subprocess.Popen(
            [args.mainboard, '-d', dag, '--ros-args', '-r',
             '__node:=' + process_node],
            cwd=work, stdout=log, stderr=subprocess.STDOUT, env=env,
            start_new_session=True)
        try:
            deadline = time.monotonic() + args.warmup
            while time.monotonic() < deadline and process.poll() is None:
                time.sleep(0.2)
            if process.poll() is not None:
                with open(log_path, errors='replace') as f:
                    text = f.read()
                if 'no module library' in text or \
                        'cannot open shared object file' in text:
                    row['status'] = 'not built'
                else:
                    row['status'] = 'exited'
                row['errors'] = error_lines(log_path)
                return row
            probe = subprocess.run(
                [sys.executable, '-c', PROBE, str(args.discovery)], env=env,
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                universal_newlines=True, timeout=args.discovery + 60)
            try:
                graph = json.loads(probe.stdout.strip().splitlines()[-1])
            except (ValueError, IndexError):
                graph = {'nodes': {}}
        finally:
            if process.poll() is None:
                try:
                    os.killpg(process.pid, signal.SIGINT)
                    process.wait(timeout=15)
                except (subprocess.TimeoutExpired, ProcessLookupError):
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    row['errors'].append('did not stop on SIGINT')
            row['exit_code'] = process.poll()

    has_parameters = True
    for full, info in sorted(graph['nodes'].items()):
        short = full.rsplit('/', 1)[-1]
        if short == process_node:
            continue
        row['nodes'].append(full)
        for kind, key in (('publishers', 'writers'),
                          ('subscribers', 'readers')):
            for topic, types in info[kind].items():
                if topic in STANDARD_TOPICS:
                    continue
                row[key][topic] = {'types': types,
                                   'kind': classify(types, registry)}
        for service, types in info['services'].items():
            if service.endswith(STANDARD_SERVICE_SUFFIXES):
                continue
            row['services'][service] = {'types': types,
                                        'kind': classify(types, registry)}
        for suffix in PARAMETER_SERVICES:
            if full + suffix not in info['services']:
                has_parameters = False
    row['parameter_services'] = bool(row['nodes']) and has_parameters
    row['errors'] += error_lines(log_path)
    # The components of the dag become nodes: none means they did not start.
    if not row['nodes']:
        row['status'] = 'no node'
    elif names and not any(
            n.rsplit('/', 1)[-1] in [re.sub(r'\W', '_', x) for x in names]
            for n in row['nodes']):
        row['status'] = 'component failed'
    if not args.keep_logs:
        shutil.rmtree(work, ignore_errors=True)
    else:
        row['log'] = log_path
    return row


def summarize(rows):
    counts = {}
    for row in rows:
        counts[row['status']] = counts.get(row['status'], 0) + 1
    endpoints = {'native': 0, 'proto': 0, 'ros': 0}
    proto_topics = {}
    for row in rows:
        for key in ('writers', 'readers', 'services'):
            for name, info in row[key].items():
                endpoints[info['kind']] += 1
                if info['kind'] == 'proto':
                    proto_topics.setdefault(name, set()).add(row['dag'])
    started = [r for r in rows if r['nodes']]
    return {
        'dags': len(rows),
        'status': counts,
        'started': len(started),
        'with_parameter_services': sum(
            1 for r in started if r['parameter_services']),
        'endpoints': endpoints,
        'proto_wire': {k: sorted(v) for k, v in sorted(proto_topics.items())},
    }


def markdown(rows, summary, args):
    out = []
    out.append('| dag | status | nodes | topics: native / protobuf / other | '
               'services: native / protobuf | parameter services |')
    out.append('|---|---|---|---|---|---|')
    for row in rows:
        topics = {'native': 0, 'proto': 0, 'ros': 0}
        for key in ('writers', 'readers'):
            for info in row[key].values():
                topics[info['kind']] += 1
        services = {'native': 0, 'proto': 0, 'ros': 0}
        for info in row['services'].values():
            services[info['kind']] += 1
        status = row['status']
        if status != 'ok' and row['errors']:
            status += ': ' + row['errors'][0].replace('|', '/')[:120]
        out.append('| `%s` | %s | %s | %d / %d / %d | %d / %d | %s |' % (
            row['dag'], status,
            ', '.join('`%s`' % n for n in row['nodes']) or '-',
            topics['native'], topics['proto'], topics['ros'],
            services['native'], services['proto'],
            {True: 'yes', False: 'no', None: '-'}[
                row['parameter_services'] if row['nodes'] else None]))
    return '\n'.join(out) + '\n'


def main():
    parser = argparse.ArgumentParser(
        description=__doc__.split('\n')[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--apollo_root', default=os.getcwd())
    parser.add_argument('--dag', action='append', default=[],
                        help='check only this dag (repeatable); default: '
                             'every modules/**/*.dag and cyber/examples dag')
    parser.add_argument('--jobs', type=int, default=3)
    parser.add_argument('--warmup', type=float, default=8.0,
                        help='seconds to let a dag start')
    parser.add_argument('--discovery', type=float, default=3.0,
                        help='seconds the probe listens to the graph')
    parser.add_argument('--domain_id_base', type=int, default=200)
    parser.add_argument('--rmw', help='RMW_IMPLEMENTATION for the run')
    parser.add_argument('--json')
    parser.add_argument('--markdown')
    parser.add_argument('--keep_logs', action='store_true')
    args = parser.parse_args()
    args.apollo_root = os.path.abspath(args.apollo_root)
    args.mainboard = find_mainboard()
    if args.mainboard is None:
        parser.error('mainboard not found: source the cyber install')
    from cyber.python.cyber_py3 import native_types
    registry = native_types.registry()

    dags = [os.path.abspath(d) for d in args.dag]
    if not dags:
        dags = sorted(
            glob.glob(os.path.join(args.apollo_root, 'modules', '**', '*.dag'),
                      recursive=True) +
            glob.glob(os.path.join(args.apollo_root, 'cyber', 'examples',
                                   '**', '*.dag'), recursive=True))
    rows = [None] * len(dags)
    slots = list(range(args.jobs))

    def work(index):
        slot = slots.pop()
        try:
            rows[index] = run_dag(dags[index], args, slot, registry)
        finally:
            slots.append(slot)
        row = rows[index]
        sys.stderr.write('[%d/%d] %-18s %s\n' % (
            sum(1 for r in rows if r is not None), len(dags), row['status'],
            row['dag']))

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        list(pool.map(work, range(len(dags))))

    summary = summarize(rows)
    summary['rmw'] = args.rmw or os.environ.get('RMW_IMPLEMENTATION',
                                                'default')
    if args.json:
        with open(args.json, 'w') as f:
            json.dump({'summary': summary, 'dags': rows}, f, indent=1)
    if args.markdown:
        with open(args.markdown, 'w') as f:
            f.write(markdown(rows, summary, args))
    print(json.dumps(summary, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
