#!/usr/bin/env bash
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
# Builds civetweb (C + C++ API, websockets, no SSL -- the configuration of
# Apollo's Bazel repo @civetweb) from the Ubuntu source tarball and installs
# libcivetweb.so + civetweb.h/CivetServer.h into PREFIX (default: the active
# conda env, else /usr/local). Needed by dreamview, monitor and task_manager.
#
# On a plain Ubuntu 24.04 machine `apt install libcivetweb-dev` provides it;
# civetweb is not on conda-forge.
#
#   tools/ros2/install_civetweb.sh [PREFIX]
set -euo pipefail

PREFIX="${1:-${CONDA_PREFIX:-/usr/local}}"
MIRROR="${UBUNTU_MIRROR:-http://archive.ubuntu.com/ubuntu}"
VER="1.16+dfsg"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/apollo-civetweb.XXXXXX")"
trap 'rm -rf "${WORK}"' EXIT
CC="${CC:-cc}"
CXX="${CXX:-c++}"
FLAGS="-O2 -fPIC -DUSE_WEBSOCKET -DNO_SSL"

cd "${WORK}"
curl -fsSL --retry 3 -o civetweb.tar.xz \
  "${MIRROR}/pool/universe/c/civetweb/civetweb_${VER}.orig.tar.xz"
mkdir src && tar -xJf civetweb.tar.xz -C src --strip-components=1
cd src
"${CC}" ${FLAGS} -Iinclude -c src/civetweb.c -o civetweb.o
"${CXX}" ${FLAGS} -Iinclude -c src/CivetServer.cpp -o CivetServer.o
"${CXX}" -shared -o libcivetweb.so -Wl,-soname,libcivetweb.so \
  civetweb.o CivetServer.o -lpthread -ldl
install -d "${PREFIX}/lib" "${PREFIX}/include"
install -m 0755 libcivetweb.so "${PREFIX}/lib/"
install -m 0644 include/civetweb.h include/CivetServer.h "${PREFIX}/include/"
echo "civetweb ${VER} installed into ${PREFIX}"
