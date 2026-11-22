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
# Builds ColPack + ADOL-C (needed by the IPOPT based smoothers of planning,
# Bazel repo @adolc) from the Ubuntu source tarballs and installs them into
# PREFIX (default: the active conda env, else /usr/local).
#
# On a plain Ubuntu 24.04 + ROS 2 Jazzy machine, `apt install libadolc-dev`
# provides the same thing; this script exists for environments without it
# (e.g. a RoboStack conda env: ADOL-C is not on conda-forge).
#
#   tools/ros2/install_adolc.sh [PREFIX]
#
# modules/CMakeLists.txt finds ADOL-C in CMAKE_PREFIX_PATH or ADOLC_ROOT.
set -euo pipefail

PREFIX="${1:-${CONDA_PREFIX:-/usr/local}}"
JOBS="${JOBS:-2}"
MIRROR="${UBUNTU_MIRROR:-http://archive.ubuntu.com/ubuntu}"
COLPACK_VER=1.0.10
ADOLC_VER=2.7.2
WORK="$(mktemp -d "${TMPDIR:-/tmp}/apollo-adolc.XXXXXX")"
trap 'rm -rf "${WORK}"' EXIT

fetch() {
  local url="$1" out="$2"
  echo "fetching ${url}"
  curl -fsSL --retry 3 -o "${out}" "${url}"
}

cd "${WORK}"
fetch "${MIRROR}/pool/universe/c/colpack/colpack_${COLPACK_VER}.orig.tar.gz" colpack.tar.gz
fetch "${MIRROR}/pool/universe/a/adolc/adolc_${ADOLC_VER}.orig.tar.xz" adolc.tar.xz

mkdir colpack adolc
tar -xzf colpack.tar.gz -C colpack --strip-components=1
tar -xJf adolc.tar.xz -C adolc --strip-components=1

# Both are built as static PIC archives (libtool's shared-library link step
# does not work with every toolchain, e.g. conda's GCC), then turned into
# shared libraries with a plain `c++ -shared` link.
CXX="${CXX:-c++}"

# ColPack (graph coloring for ADOL-C's sparse drivers)
(
  cd colpack
  if [ ! -x configure ]; then autoreconf -fi; fi
  ./configure --prefix="${PREFIX}" --libdir="${PREFIX}/lib" \
    --enable-static --disable-shared --with-pic --disable-openmp \
    CXXFLAGS="${CXXFLAGS:-} -O2 -fPIC -std=c++11"
  make -j"${JOBS}"
  make install
)
"${CXX}" -shared -o "${PREFIX}/lib/libColPack.so" -Wl,-soname,libColPack.so \
  -Wl,--whole-archive "${PREFIX}/lib/libColPack.a" -Wl,--no-whole-archive
rm -f "${PREFIX}/lib/libColPack.a" "${PREFIX}/lib/libColPack.la"

# ADOL-C with sparse drivers and OpenMP support headers
(
  cd adolc
  ./configure --prefix="${PREFIX}" --libdir="${PREFIX}/lib" \
    --enable-sparse --with-colpack="${PREFIX}" --with-openmp-flag=-fopenmp \
    --with-boost=no \
    --enable-static --disable-shared --with-pic \
    CXXFLAGS="${CXXFLAGS:-} -O2 -fPIC" CFLAGS="${CFLAGS:-} -O2 -fPIC"
  make -j"${JOBS}"
  make install
)
"${CXX}" -shared -o "${PREFIX}/lib/libadolc.so" -Wl,-soname,libadolc.so \
  -Wl,--whole-archive "${PREFIX}/lib/libadolc.a" -Wl,--no-whole-archive \
  -L"${PREFIX}/lib" -lColPack -fopenmp
rm -f "${PREFIX}/lib/libadolc.a" "${PREFIX}/lib/libadolc.la"

echo "ADOL-C ${ADOLC_VER} installed into ${PREFIX}"
