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
"""Locates the official Lanelet2 example map (mapping_example.osm of the
lanelet2_maps package, BSD-3-Clause, not part of this repository).

  fetch_example_map.py [--cache_dir DIR]

Search order: $LANELET2_EXAMPLE_MAP, an installed lanelet2_maps package
(share/lanelet2_maps/res), the cache directory, and finally the RoboStack
conda package, which is downloaded and unpacked into the cache directory.
Prints the path, or exits with 1 if the map cannot be obtained (offline).
"""

import argparse
import io
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

NAME = 'mapping_example.osm'
PACKAGE_URL = ('https://conda.anaconda.org/robostack-jazzy/linux-64/'
               'ros-jazzy-lanelet2-maps-1.2.3-np2py312h2ed9cc7_21.conda')
MEMBER = 'share/lanelet2_maps/res/' + NAME


def default_cache_dir():
    base = os.environ.get('XDG_CACHE_HOME',
                          os.path.join(os.path.expanduser('~'), '.cache'))
    return os.path.join(base, 'apollo_lanelet2')


def _unzstd(data):
    try:
        import zstandard  # pylint: disable=import-outside-toplevel
        return zstandard.ZstdDecompressor().stream_reader(
            io.BytesIO(data)).read()
    except ImportError:
        tool = shutil.which('zstd')
        if not tool:
            raise RuntimeError('need the zstandard module or the zstd tool')
        return subprocess.run([tool, '-dc'], input=data, capture_output=True,
                              check=True).stdout


def find_example_map(cache_dir=None, download=True):
    """Path of mapping_example.osm, or None."""
    candidate = os.environ.get('LANELET2_EXAMPLE_MAP', '')
    if candidate and os.path.isfile(candidate):
        return candidate
    for prefix in os.environ.get('AMENT_PREFIX_PATH', '').split(os.pathsep) + \
            os.environ.get('CONDA_PREFIX', '').split(os.pathsep):
        candidate = os.path.join(prefix, MEMBER)
        if prefix and os.path.isfile(candidate):
            return candidate
    cache_dir = cache_dir or default_cache_dir()
    candidate = os.path.join(cache_dir, NAME)
    if os.path.isfile(candidate):
        return candidate
    if not download:
        return None
    try:
        with urllib.request.urlopen(PACKAGE_URL, timeout=60) as response:
            package = response.read()
        # a .conda file is a zip with the payload as pkg-*.tar.zst
        with zipfile.ZipFile(io.BytesIO(package)) as archive:
            payload = [n for n in archive.namelist()
                       if n.startswith('pkg-') and n.endswith('.tar.zst')][0]
            tar_bytes = _unzstd(archive.read(payload))
        with tarfile.open(fileobj=io.BytesIO(tar_bytes)) as tar:
            data = tar.extractfile(MEMBER).read()
        os.makedirs(cache_dir, exist_ok=True)
        with open(candidate, 'wb') as f:
            f.write(data)
        return candidate
    except Exception as e:  # pylint: disable=broad-except
        print('cannot fetch the Lanelet2 example map: %s' % e,
              file=sys.stderr)
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--cache_dir', default='')
    args = parser.parse_args()
    path = find_example_map(args.cache_dir or None)
    if not path:
        return 1
    print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
