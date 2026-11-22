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
"""pytest setup for the cyber_py3 tests.

* The source tree is put first on sys.path (cyber.python.cyber_py3 is tested
  from source; set CYBER_PY3_TEST_INSTALLED=1 to test the installed copy).
* If ``cyber.proto.*_pb2`` is not importable, cyber/proto/*.proto are compiled
  with protoc into a temporary directory added to sys.path / PYTHONPATH.
* Every session uses a random ROS_DOMAIN_ID (unless CYBER_PY3_TEST_DOMAIN_ID
  is set) so concurrent test runs on one machine do not see each other.

Run with:  python3 -m pytest cyber/python/cyber_py3/test
"""

import atexit
import glob
import os
import random
import shutil
import subprocess
import sys
import tempfile

import pytest

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
APOLLO_ROOT = os.path.abspath(os.path.join(TEST_DIR, '..', '..', '..', '..'))
TOOLS_DIR = os.path.join(APOLLO_ROOT, 'cyber', 'tools')

_paths = []
if not os.environ.get('CYBER_PY3_TEST_INSTALLED'):
    # An installed `cyber` *regular* package (cyber/__init__.py) would shadow
    # the source tree even if APOLLO_ROOT comes first on sys.path. A tiny
    # overlay package merging every `cyber` directory on the path (source
    # tree first) avoids that.
    _overlay = tempfile.mkdtemp(prefix='cyber_py3_test_overlay_')
    atexit.register(shutil.rmtree, _overlay, True)
    os.mkdir(os.path.join(_overlay, 'cyber'))
    with open(os.path.join(_overlay, 'cyber', '__init__.py'), 'w') as _f:
        _f.write('from pkgutil import extend_path\n'
                 '__path__ = extend_path(__path__, __name__)\n')
    _paths += [_overlay, APOLLO_ROOT]


def _generate_cyber_protos():
    try:
        import cyber.proto.unit_test_pb2  # noqa: F401 pylint: disable=unused-import
        return None
    except ImportError:
        pass
    protoc = shutil.which('protoc')
    if protoc is None:
        return None
    out = tempfile.mkdtemp(prefix='cyber_py3_test_protos_')
    atexit.register(shutil.rmtree, out, True)
    protos = sorted(glob.glob(os.path.join(APOLLO_ROOT, 'cyber', 'proto',
                                           '*.proto')))
    subprocess.check_call(
        [protoc, '-I', APOLLO_ROOT, '--python_out=' + out] +
        [os.path.relpath(p, APOLLO_ROOT) for p in protos], cwd=APOLLO_ROOT)
    return out


for _p in reversed(_paths):
    while _p in sys.path:
        sys.path.remove(_p)
    sys.path.insert(0, _p)
_proto_dir = _generate_cyber_protos()
if _proto_dir:
    sys.path.insert(len(_paths), _proto_dir)
    _paths.append(_proto_dir)
    # `cyber` may already be imported with a fixed __path__ (overlay)
    _cyber_pkg = sys.modules.get('cyber')
    if _cyber_pkg is not None and isinstance(
            getattr(_cyber_pkg, '__path__', None), list):
        _cyber_pkg.__path__.append(os.path.join(_proto_dir, 'cyber'))

os.environ['PYTHONPATH'] = os.pathsep.join(
    _paths + [p for p in os.environ.get('PYTHONPATH', '').split(os.pathsep)
              if p])
os.environ['ROS_DOMAIN_ID'] = os.environ.get(
    'CYBER_PY3_TEST_DOMAIN_ID', str(random.randint(60, 100)))


def have_cyber_protos():
    try:
        import cyber.proto.unit_test_pb2  # noqa: F401 pylint: disable=unused-import
        return True
    except ImportError:
        return False


def have_ros():
    try:
        import rclpy  # noqa: F401 pylint: disable=unused-import
        import apollo_msgs.msg  # noqa: F401 pylint: disable=unused-import
        return True
    except ImportError:
        return False


requires_ros = pytest.mark.skipif(
    not have_ros(), reason='rclpy / apollo_msgs not available')
requires_cyber_protos = pytest.mark.skipif(
    not have_cyber_protos(), reason='cyber.proto python modules unavailable')


@pytest.fixture
def cyber_env():
    """cyber.init() for the test, cyber.shutdown() afterwards."""
    from cyber.python.cyber_py3 import cyber
    assert cyber.init('cyber_py3_test')
    yield cyber
    cyber.shutdown()


@pytest.fixture
def unique():
    """A per-test unique suffix for channel/node names."""
    return '%d_%d' % (os.getpid(), random.randint(0, 1 << 30))


def wait_until(predicate, timeout=10.0, interval=0.02):
    import time
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(interval)
    return predicate()


def run_python(code, *args, timeout=60, **kwargs):
    """Runs python code in a subprocess with the test environment."""
    return subprocess.run([sys.executable, '-c', code] + list(args),
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          universal_newlines=True, timeout=timeout,
                          env=dict(os.environ), **kwargs)


def spawn_python(code, *args, **kwargs):
    return subprocess.Popen([sys.executable, '-c', code] + list(args),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, env=dict(os.environ),
                            **kwargs)
