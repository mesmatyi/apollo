#!/usr/bin/env bash
###############################################################################
# Builds the Apollo ROS 2 colcon workspace.
#
#   scripts/ros2/build.sh                 # everything
#   scripts/ros2/build.sh cyber           # selected packages (+ dependencies)
#   APOLLO_BUILD_TESTS=1 scripts/ros2/build.sh
#
# Environment:
#   ROS_DISTRO            ROS 2 distribution to source (default: jazzy)
#   APOLLO_BUILD_BASE     colcon build dir   (default: <repo>/build_ros2)
#   APOLLO_INSTALL_BASE   colcon install dir (default: <repo>/install)
#   APOLLO_JOBS           parallel compile jobs (default: nproc)
#   APOLLO_BUILD_TESTS    1 = build and run unit tests
###############################################################################
set -euo pipefail

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROS_DISTRO="${ROS_DISTRO:-jazzy}"
BUILD_BASE="${APOLLO_BUILD_BASE:-${TOP_DIR}/build_ros2}"
INSTALL_BASE="${APOLLO_INSTALL_BASE:-${TOP_DIR}/install}"
JOBS="${APOLLO_JOBS:-$(nproc)}"
TESTS="${APOLLO_BUILD_TESTS:-0}"

if [[ -z "${AMENT_PREFIX_PATH:-}" ]]; then
  if [[ -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]]; then
    # shellcheck disable=SC1090
    set +u; source "/opt/ros/${ROS_DISTRO}/setup.bash"; set -u
  else
    echo "ROS 2 ${ROS_DISTRO} not found: source its setup.bash first." >&2
    exit 1
  fi
fi

select_args=()
if [[ $# -gt 0 ]]; then
  select_args=(--packages-up-to "$@")
fi

test_flag=OFF
[[ "${TESTS}" == "1" ]] && test_flag=ON

export CMAKE_BUILD_PARALLEL_LEVEL="${JOBS}"
cd "${TOP_DIR}"
colcon build \
  --base-paths ros2 cyber modules \
  --build-base "${BUILD_BASE}" \
  --install-base "${INSTALL_BASE}" \
  --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=${test_flag} \
               -DAPOLLO_BUILD_TESTS=${test_flag} -G Ninja \
  --event-handlers console_cohesion+ \
  "${select_args[@]}"

if [[ "${TESTS}" == "1" ]]; then
  colcon test --base-paths ros2 cyber modules \
    --build-base "${BUILD_BASE}" --install-base "${INSTALL_BASE}" \
    --return-code-on-test-failure "${select_args[@]}"
  colcon test-result --build-base "${BUILD_BASE}" --verbose
fi

echo
echo "Done. Use it with:  source ${INSTALL_BASE}/setup.bash"
