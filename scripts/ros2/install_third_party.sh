#!/usr/bin/env bash
###############################################################################
# Installs Apollo third-party libraries that Ubuntu 24.04 does not package as
# needed, for the ROS 2 (colcon) build.
#
#   sudo scripts/ros2/install_third_party.sh [PREFIX]   (default /usr/local)
#
#   * ADOL-C   : apt (libadolc-dev) on Ubuntu; tools/ros2/install_adolc.sh
#                builds it elsewhere (e.g. RoboStack/conda environments)
#   * OSQP 1.x : built from source. Apollo's OSQP 0.6 C API is provided on top
#                of it by modules/ros2_compat/osqp (an installed OSQP 0.6 also
#                works; the build then uses its native API).
#   * civetweb : apt (libcivetweb-dev) -- Dreamview, monitor, task_manager
###############################################################################
set -euo pipefail

PREFIX="${1:-/usr/local}"
JOBS="${JOBS:-$(nproc)}"
OSQP_VERSION="${OSQP_VERSION:-v1.0.0}"

if command -v apt-get >/dev/null 2>&1; then
  apt-get update
  apt-get install -y --no-install-recommends libadolc-dev libcivetweb-dev \
    git cmake build-essential
  rm -rf /var/lib/apt/lists/*
fi

if [[ ! -f "${PREFIX}/include/osqp/osqp.h" && ! -f "${PREFIX}/include/osqp.h" ]]; then
  work="$(mktemp -d)"
  trap 'rm -rf "${work}"' EXIT
  git clone --depth 1 --branch "${OSQP_VERSION}" --recursive \
    https://github.com/osqp/osqp.git "${work}/osqp"
  cmake -S "${work}/osqp" -B "${work}/osqp/build" \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DOSQP_BUILD_SHARED_LIB=ON
  cmake --build "${work}/osqp/build" -j "${JOBS}"
  cmake --install "${work}/osqp/build"
fi
ldconfig 2>/dev/null || true
echo "third-party libraries installed into ${PREFIX}"
