#! /usr/bin/env bash
###############################################################################
# Environment for Apollo Cyber on ROS 2.
#
#   source cyber/setup.bash
#
# Sources ROS 2 and the colcon install of this repository (if present), points
# APOLLO_ROOT at this checkout and sets Cyber's logging defaults. Binaries such
# as mainboard, cyber_channel, cyber_node, cyber_service, cyber_launch and the
# record converters come from the install and are on PATH afterwards.
###############################################################################
TOP_DIR="$(cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P)"
: "${ROS_DISTRO:=jazzy}"

if [[ -z "${AMENT_PREFIX_PATH:-}" && -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]]; then
  source "/opt/ros/${ROS_DISTRO}/setup.bash"
fi
_apollo_install="${APOLLO_INSTALL_BASE:-${TOP_DIR}/install}"
if [[ -f "${_apollo_install}/setup.bash" ]]; then
  source "${_apollo_install}/setup.bash"
fi
unset _apollo_install

export APOLLO_ROOT="${APOLLO_ROOT:-${TOP_DIR}}"

# Cyber's CYBER_DOMAIN_ID maps onto the ROS 2 domain.
if [[ -z "${ROS_DOMAIN_ID:-}" && -n "${CYBER_DOMAIN_ID:-}" ]]; then
  export ROS_DOMAIN_ID="${CYBER_DOMAIN_ID}"
fi

export GLOG_log_dir="${GLOG_log_dir:-${APOLLO_ROOT}/data/log}"
mkdir -p "${GLOG_log_dir}" 2>/dev/null || true
export GLOG_alsologtostderr="${GLOG_alsologtostderr:-0}"
export GLOG_colorlogtostderr="${GLOG_colorlogtostderr:-1}"
export GLOG_minloglevel="${GLOG_minloglevel:-0}"

if [[ -f "${TOP_DIR}/cyber/tools/cyber_tools_auto_complete.bash" ]]; then
  source "${TOP_DIR}/cyber/tools/cyber_tools_auto_complete.bash"
fi
