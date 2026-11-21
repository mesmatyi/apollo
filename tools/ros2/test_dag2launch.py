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
"""Tests for tools/ros2/dag2launch.py.

    python3 -m unittest tools/ros2/test_dag2launch.py      # or pytest

The converter tests only need the standard library.  The tests of the
generated files need a sourced ROS 2 environment (launch, launch_ros) and are
skipped without one; the protobuf cross-check also needs protoc and the
google.protobuf Python package.
"""

import contextlib
import glob
import io
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import dag2launch  # noqa: E402
from dag2launch import Converter, TextFormatError, output_path_for, parse_text_proto  # noqa: E402

REPO_ROOT = dag2launch.DEFAULT_ROOT


def _write(root, relpath, text):
    path = os.path.join(root, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(textwrap.dedent(text).lstrip('\n'))
    return path


def _read(path):
    with open(path) as f:
        return f.read()


def _run_main(argv):
    err = io.StringIO()
    with contextlib.redirect_stderr(err):
        code = dag2launch.main(argv)
    return code, err.getvalue()


class TextFormatTest(unittest.TestCase):

    def test_parse_and_serialize(self):
        msg = parse_text_proto('''
            # comment
            name : "a" 'b'   # adjacent strings are concatenated
            interval: 10,
            readers: [ { channel: "/x" }, { channel: "/y" pending_queue_size: 5 } ];
            readers < channel: "/z\\t\\"q\\"\\101" qos_profile { depth: 2 history: HISTORY_KEEP_ALL } >
        ''')
        self.assertEqual(msg.scalar('name'), 'ab')
        self.assertEqual(msg.scalar('interval'), '10')
        self.assertEqual([r.scalar('channel') for r in msg.get_all('readers')],
                         ['/x', '/y', '/z\t"q"A'])
        text = msg.to_text()
        self.assertEqual(text.split('\n')[0], 'name: "ab"')
        self.assertIn('readers { channel: "/z\\t\\"q\\"A" qos_profile { depth: 2 '
                      'history: HISTORY_KEEP_ALL } }', text)
        # serializing is stable
        self.assertEqual(parse_text_proto(text).to_text(), text)

    def test_escapes_round_trip(self):
        value = 'tab\t nl\n quote" back\\ \x01 \xe9té 中'
        msg = parse_text_proto('name: %s' % dag2launch._escape(value))
        self.assertEqual(msg.scalar('name'), value)

    def test_errors(self):
        for text in ('name "x"', 'name: "x', 'a { b: 1', '{', 'name: "\\q"'):
            with self.assertRaises(TextFormatError, msg=text):
                parse_text_proto(text)

    def test_schema(self):
        good = parse_text_proto('module_config { module_library: "x.so" components { '
                                'class_name: "A" config { name: "a" readers { channel: "/c" '
                                'qos_profile { reliability: RELIABILITY_BEST_EFFORT } } } } }')
        dag2launch._validate(good, 'DagConfig')
        for bad in ('module_config { foo: 1 }',
                    'module_config { module_library: "a" module_library: "b" }',
                    'module_config { timer_components { config { interval: "10" } } }',
                    'module_config { timer_components { config { interval: -1 } } }',
                    'module_config { components { config { readers { qos_profile { '
                    'history: NOPE } } } } }'):
            with self.assertRaises(TextFormatError, msg=bad):
                dag2launch._validate(parse_text_proto(bad), 'DagConfig')


class OutputPathTest(unittest.TestCase):

    def test_paths(self):
        self.assertEqual(output_path_for('modules/a/dag/foo.dag'), 'modules/a/launch/foo.dag.launch.py')
        self.assertEqual(output_path_for('modules/a/launch/bar.launch'), 'modules/a/launch/bar.launch.py')
        self.assertEqual(output_path_for('modules/t/x/x.dag'), 'modules/t/x/launch/x.dag.launch.py')
        self.assertEqual(output_path_for('modules/t/x/x.launch'), 'modules/t/x/launch/x.launch.py')


class ConverterTest(unittest.TestCase):

    def setUp(self):
        self.root = tempfile.mkdtemp(prefix='dag2launch_test_')
        self.addCleanup(shutil.rmtree, self.root)
        _write(self.root, 'modules/foo/foo_component.h', '''
            namespace apollo {
            namespace foo {
            class FooComponent {};
            class FooTimer {};
            CYBER_REGISTER_COMPONENT(FooComponent)
            CYBER_REGISTER_COMPONENT(FooTimer)
            }  // namespace foo
            }  // namespace apollo
        ''')
        _write(self.root, 'modules/bar/impl/bar_component.h', '''
            namespace apollo { namespace bar {
            CYBER_REGISTER_COMPONENT(BarComponent)
            } }
        ''')
        _write(self.root, 'modules/foo/dag/foo.dag', '''
            module_config {
              module_library : "/apollo/bazel-bin/modules/foo/libfoo.so"
              timer_components {
                class_name : "FooTimer"
                config { name: "foo-timer" interval: 10 }
              }
              components {
                class_name : "FooComponent"
                config {
                  name : "foo"
                  config_file_path : "/apollo/modules/foo/conf/foo.pb.txt"
                  readers { channel: "/apollo/foo" pending_queue_size: 5 }
                }
              }
            }
        ''')
        _write(self.root, 'modules/bar/dag/bar.dag', '''
            module_config {
              module_library : "modules/bar/libbar.so"
              components { class_name : "BarComponent" config { name : "bar" } }
              components { class_name : "Missing" config { name : "missing" } }
            }
        ''')
        _write(self.root, 'modules/foo/launch/foo.launch', '''
            <cyber>
              <desc>test</desc>
              <!-- foo and bar share a process -->
              <module>
                <name>foo</name>
                <dag_conf>/apollo/modules/foo/dag/foo.dag</dag_conf>
                <process_name>shared</process_name>
                <exception_handler>respawn</exception_handler>
              </module>
              <module>
                <name>bar</name>
                <!-- bar comment -->
                <dag_conf>/apollo/modules/bar/dag/bar.dag</dag_conf>
                <process_name>shared</process_name>
              </module>
              <module>
                <name>moved</name>
                <dag_conf>/apollo/modules/foo/old/foo.dag</dag_conf>
                <process_name>moved</process_name>
                <exception_handler>exit</exception_handler>
              </module>
              <module>
                <name>tool</name>
                <type>binary</type>
                <dag_conf></dag_conf>
                <process_name>/apollo/bazel-bin/modules/foo/tool --flagfile=/apollo/modules/foo/conf/x.conf</process_name>
              </module>
            </cyber>
        ''')
        _write(self.root, 'modules/ros1/start.launch', '<launch><node pkg="x"/></launch>')

    def test_class_resolution(self):
        conv = Converter(self.root)
        foo = conv.dag('modules/foo/dag/foo.dag')
        self.assertIsNone(foo.error)
        # components are loaded before timer components, like mainboard
        self.assertEqual([(c.plugin, c.node_name, c.resolution.status) for c in foo.components], [
            ('apollo::foo::FooComponent', 'foo', 'exact'),
            ('apollo::foo::FooTimer', 'foo_timer', 'exact')])
        bar = conv.dag('modules/bar/dag/bar.dag')
        self.assertEqual([(c.plugin, c.resolution.status) for c in bar.components], [
            ('apollo::bar::BarComponent', 'fallback'), ('Missing', 'unresolved')])

    def test_dag_conversion(self):
        result = Converter(self.root).convert(os.path.join(self.root, 'modules/foo/dag/foo.dag'))
        self.assertEqual(result.output, 'modules/foo/launch/foo.dag.launch.py')
        content = result.content
        self.assertTrue(content.startswith(dag2launch.MARKER))
        compile(content, result.output, 'exec')
        self.assertIn("plugin='apollo::foo::FooComponent'", content)
        self.assertIn("name='foo_timer'", content)
        self.assertIn("'name: \"foo\"\\n'", content)
        self.assertIn("'readers { channel: \"/apollo/foo\" pending_queue_size: 5 }')}]),", content)
        self.assertIn("apollo_file('modules/foo/dag/foo.dag')", content)
        self.assertNotIn(self.root, content)  # no machine specific paths

    def test_launch_conversion(self):
        result = Converter(self.root).convert(os.path.join(self.root, 'modules/foo/launch/foo.launch'))
        content = result.content
        compile(content, result.output, 'exec')
        # one container for the shared process, with the components of both dags
        self.assertEqual(content.count('ComposableNodeContainer('), 2)
        self.assertIn("name='shared_container'", content)
        self.assertIn('# foo and bar share a process', content)
        self.assertIn('# bar comment', content)
        # respawn: components are reloaded from an OnProcessStart handler
        self.assertIn('respawn=True', content)
        self.assertIn('respawn_max_retries=3', content)
        self.assertIn('LoadComposableNodes(', content)
        # exit: shut the launch down
        self.assertIn("on_exit=[Shutdown(reason='process moved exited", content)
        # moved dag is found by its file name
        self.assertIn('NOTE: modules/foo/old/foo.dag does not exist; using modules/foo/dag/foo.dag',
                      content)
        # mainboard arguments as cyber_launch passed them
        self.assertIn("'-p', 'shared'", content)
        self.assertIn("'-s', 'CYBER_DEFAULT'", content)
        # binary
        self.assertIn("apollo_executable('/apollo/bazel-bin/modules/foo/tool')", content)
        self.assertIn("apollo_arg('--flagfile=/apollo/modules/foo/conf/x.conf')", content)
        self.assertTrue(any('unresolved class' in e for e in result.errors))

    def test_non_cyber_launch_is_skipped(self):
        result = Converter(self.root).convert(os.path.join(self.root, 'modules/ros1/start.launch'))
        self.assertIsNone(result.content)
        self.assertIn('not a cyber_launch file', result.skipped)

    def test_main_check_and_determinism(self):
        root = ['--root', self.root]
        code, err = _run_main(root + ['--check'])
        self.assertEqual(code, 1)
        self.assertIn('stale: modules/foo/launch/foo.dag.launch.py', err)
        self.assertEqual(_run_main(root)[0], 0)
        outputs = sorted(glob.glob(os.path.join(self.root, 'modules/*/launch/*.launch.py')))
        self.assertEqual([os.path.relpath(p, self.root) for p in outputs], [
            'modules/bar/launch/bar.dag.launch.py',
            'modules/foo/launch/foo.dag.launch.py',
            'modules/foo/launch/foo.launch.py'])
        before = {p: _read(p) for p in outputs}
        self.assertEqual(_run_main(root)[0], 0)  # re-run: nothing changes
        self.assertEqual(before, {p: _read(p) for p in outputs})
        self.assertEqual(_run_main(root + ['--check'])[0], 0)
        # --strict fails on the unresolved class
        self.assertEqual(_run_main(root + ['--check', '--strict'])[0], 1)
        # editing a source makes the output stale
        with open(os.path.join(self.root, 'modules/bar/dag/bar.dag'), 'a') as f:
            f.write('module_config { module_library: "modules/foo/libfoo.so" '
                    'components { class_name: "FooComponent" config { name: "foo2" } } }\n')
        code, err = _run_main(root + ['--check'])
        self.assertEqual(code, 1)
        self.assertIn('stale: modules/bar/launch/bar.dag.launch.py', err)
        # removing a source makes its output an orphan, which a run deletes
        os.remove(os.path.join(self.root, 'modules/bar/dag/bar.dag'))
        _write(self.root, 'modules/bar/launch/hand_written.launch.py', '# not generated\n')
        code, err = _run_main(root + ['--check'])
        self.assertEqual(code, 1)
        self.assertIn('orphan (source removed): modules/bar/launch/bar.dag.launch.py', err)
        self.assertEqual(_run_main(root)[0], 0)
        self.assertFalse(os.path.exists(os.path.join(self.root, 'modules/bar/launch/bar.dag.launch.py')))
        self.assertTrue(os.path.exists(os.path.join(self.root, 'modules/bar/launch/hand_written.launch.py')))

    def test_single_file_to_stdout(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code, _ = _run_main(['--root', self.root, '-o', '-',
                                 os.path.join(self.root, 'modules/foo/dag/foo.dag')])
        self.assertEqual(code, 0)
        self.assertTrue(out.getvalue().startswith(dag2launch.MARKER))


class RepositoryTest(unittest.TestCase):
    """The generated files in the repository are up to date (the CI check)."""

    def test_generated_files_are_fresh(self):
        code, err = _run_main(['--root', REPO_ROOT, '--check'])
        self.assertEqual(code, 0, err)


def _generated_files():
    return sorted(glob.glob(os.path.join(REPO_ROOT, 'modules/**/launch/*.launch.py'), recursive=True))


def _source_of(path):
    with open(path) as f:
        first = f.readline()
    assert first.startswith(dag2launch.MARKER), path
    return first[len(dag2launch.MARKER) + len(' from '):].strip().rstrip('.')


def _compile_protos(out_dir):
    protoc = shutil.which('protoc')
    if not protoc:
        return False
    protos = ['cyber/proto/dag_conf.proto', 'cyber/proto/component_conf.proto',
              'cyber/proto/qos_profile.proto']
    if not all(os.path.exists(os.path.join(REPO_ROOT, p)) for p in protos):
        return False
    subprocess.check_call([protoc, '-I' + REPO_ROOT, '--python_out=' + out_dir] + protos,
                          cwd=REPO_ROOT)
    return True


class GeneratedLaunchFilesTest(unittest.TestCase):
    """Loads every generated launch file with ROS 2 launch and evaluates both
    runners without starting processes."""

    @classmethod
    def setUpClass(cls):
        try:
            import launch  # noqa: F401
            import launch_ros  # noqa: F401
        except ImportError:
            raise unittest.SkipTest('ROS 2 launch / launch_ros not available')
        cls.pb = None
        try:
            from google.protobuf import text_format  # noqa: F401
            tmp = tempfile.mkdtemp(prefix='dag2launch_pb_')
            if _compile_protos(tmp):
                sys.path.insert(0, tmp)
                from cyber.proto import component_conf_pb2, dag_conf_pb2
                cls.pb = (text_format, dag_conf_pb2, component_conf_pb2)
        except (ImportError, subprocess.CalledProcessError):
            cls.pb = None

    def _expected_from_dag(self, relpath):
        """[(class_name, kind, config message)] parsed by protobuf itself."""
        text_format, dag_conf_pb2, _ = self.pb
        dag = dag_conf_pb2.DagConfig()
        with open(os.path.join(REPO_ROOT, relpath)) as f:
            text_format.Parse(f.read(), dag)
        out = []
        for module in dag.module_config:
            out += [(c.class_name, 'components', c.config) for c in module.components]
            out += [(c.class_name, 'timer_components', c.config) for c in module.timer_components]
        return out

    def _evaluate(self, path, runner):
        from launch import LaunchContext
        from launch.actions import DeclareLaunchArgument, ExecuteProcess, RegisterEventHandler
        from launch.actions import SetEnvironmentVariable
        from launch.events.process import ProcessStarted
        from launch.launch_description_sources import get_launch_description_from_python_launch_file
        from launch_ros.actions import ComposableNodeContainer, Node
        from launch_ros.actions.load_composable_nodes import get_composable_node_load_request

        ld = get_launch_description_from_python_launch_file(path)
        context = LaunchContext()
        context.launch_configurations['runner'] = runner
        containers, mainboards, binaries = [], [], []

        def requests(descriptions):
            return [get_composable_node_load_request(d, context) for d in descriptions]

        for entity in ld.entities:
            if isinstance(entity, DeclareLaunchArgument):
                entity.visit(context)  # validates `runner` against the choices
                continue
            if isinstance(entity, SetEnvironmentVariable):
                continue
            if isinstance(entity, RegisterEventHandler):
                handler = entity.event_handler
                target = handler._OnActionEventBase__target_action
                if target.condition and not target.condition.evaluate(context):
                    continue
                event = ProcessStarted(action=target, name='x', cmd=[], cwd=None, env=None, pid=1)
                load = handler.handle(event, context)
                containers[-1][1].extend(requests(
                    load._LoadComposableNodes__composable_node_descriptions))
                continue
            if entity.condition and not entity.condition.evaluate(context):
                continue
            if isinstance(entity, ComposableNodeContainer):
                containers.append((entity, requests(
                    entity._ComposableNodeContainer__composable_node_descriptions or [])))
            elif isinstance(entity, Node):
                mainboards.append(entity)
            elif isinstance(entity, ExecuteProcess):
                binaries.append(entity)
            else:
                self.fail('%s: unexpected action %r' % (path, entity))
        return containers, mainboards, binaries

    def test_all_generated_files(self):
        from rcl_interfaces.msg import ParameterType
        files = _generated_files()
        self.assertGreater(len(files), 150)
        conv = Converter(REPO_ROOT)
        total = 0
        for path in files:
            with self.subTest(path=os.path.relpath(path, REPO_ROOT)):
                source = _source_of(path)
                containers, mainboards, binaries = self._evaluate(path, 'container')
                self.assertFalse(mainboards)
                m_containers, m_mainboards, m_binaries = self._evaluate(path, 'mainboard')
                self.assertFalse(m_containers)
                self.assertEqual(len(m_mainboards), len(containers))
                self.assertEqual(len(binaries), len(m_binaries))
                # the dags of each process, as the converter grouped them
                if source.endswith('.dag'):
                    dag_lists = [[source]]
                else:
                    result = conv.convert(os.path.join(REPO_ROOT, source))
                    self.assertIsNotNone(result.content)
                    with open(os.path.join(REPO_ROOT, source), 'rb') as f:
                        lf = dag2launch.parse_launch(f.read())
                    procs = conv._group(lf, source, dag2launch.ConversionResult(source, 'launch'))
                    dag_lists = [[r.relpath for r in p.dags if not r.error]
                                 for p in procs if p.kind == 'library']
                self.assertEqual(len(dag_lists), len(containers))
                for (container, reqs), dags, mainboard in zip(containers, dag_lists, m_mainboards):
                    args = [a for sub in mainboard.cmd for a in sub
                            if type(a).__name__ == 'TextSubstitution']
                    self.assertGreaterEqual(sum(1 for a in args if a.text == '-d'), len(dags))
                    names = []
                    for req in reqs:
                        self.assertIn(req.package_name, ('apollo', 'cyber'))
                        self.assertEqual(len(req.parameters), 1)
                        param = req.parameters[0]
                        self.assertEqual(param.name, 'component_config')
                        self.assertEqual(param.value.type, ParameterType.PARAMETER_STRING)
                        names.append(req.node_name)
                    total += len(reqs)
                    if self.pb is None:
                        continue
                    text_format, _, component_conf_pb2 = self.pb
                    expected = [e for d in dags for e in self._expected_from_dag(d)]
                    self.assertEqual(len(expected), len(reqs))
                    for (cls, kind, config), req in zip(expected, reqs):
                        msg_type = (component_conf_pb2.ComponentConfig if kind == 'components'
                                    else component_conf_pb2.TimerComponentConfig)
                        parsed = text_format.Parse(req.parameters[0].value.string_value, msg_type())
                        self.assertEqual(parsed, config)
                        self.assertTrue(req.plugin_name == cls
                                        or req.plugin_name.endswith('::' + cls), req.plugin_name)
                        self.assertEqual(req.node_name, dag2launch.to_node_name(config.name))
        self.assertGreater(total, 300)
        if self.pb is None:
            print('note: protobuf cross-check skipped (no protoc / google.protobuf)',
                  file=sys.stderr)

    def test_plain_string_parameter_stays_a_string(self):
        """The generated files pass component_config as a plain str, which
        launch_ros must not YAML-evaluate (it only does that for substitutions)."""
        from launch import LaunchContext
        from launch_ros.descriptions import ComposableNode
        from launch_ros.actions.load_composable_nodes import get_composable_node_load_request
        from rcl_interfaces.msg import ParameterType
        for value in ('name: "control"\ninterval: 10', 'interval: 10', '10', 'true'):
            node = ComposableNode(package='apollo', plugin='X', name='x',
                                  parameters=[{'component_config': value}])
            param = get_composable_node_load_request(node, LaunchContext()).parameters[0]
            self.assertEqual(param.value.type, ParameterType.PARAMETER_STRING)
            self.assertEqual(param.value.string_value, value)


if __name__ == '__main__':
    unittest.main()
