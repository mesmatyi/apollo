#!/usr/bin/env bash
# Sources ROS 2 and the Apollo workspace and points APOLLO_ROOT at this
# checkout, so configs that reference /apollo/... resolve here.
#   source scripts/ros2/setup.bash
_apollo_top="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
: "${ROS_DISTRO:=jazzy}"
if [[ -z "${AMENT_PREFIX_PATH:-}" && -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]]; then
  source "/opt/ros/${ROS_DISTRO}/setup.bash"
fi
if [[ -f "${APOLLO_INSTALL_BASE:-${_apollo_top}/install}/setup.bash" ]]; then
  source "${APOLLO_INSTALL_BASE:-${_apollo_top}/install}/setup.bash"
fi
export APOLLO_ROOT="${APOLLO_ROOT:-${_apollo_top}}"
# Apollo logs through glog; also print to the console under ros2 launch.
export GLOG_alsologtostderr="${GLOG_alsologtostderr:-1}"
unset _apollo_top
