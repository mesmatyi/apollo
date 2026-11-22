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
#
# Apollo (Cyber) executables as seen by the stock ROS 2 command line tools:
# typed topics and services, ROS arguments (remapping, node name, namespace,
# parameters, parameter files), gflags as parameters.
#
# Usage: cli_interop_test.sh [directory with the cyber executables
#                             [example dag [library directory]]]
# The ROS 2 environment (and the apollo_msgs install) must be sourced. Without
# arguments the installed cyber package is tested; the build passes its own
# directories. Uses ROS_DOMAIN_ID (default 87) and both installed RMW
# implementations unless RMW_IMPLEMENTATION is set.

set -u

BIN_DIR="${1:-}"
if [[ -z "${BIN_DIR}" ]]; then
  BIN_DIR="$(ros2 pkg prefix cyber 2>/dev/null)/lib/cyber"
fi
EXAMPLE_DAG="${2:-${BIN_DIR}/../../share/cyber/examples/common_component_example/common.dag}"
if [[ -n "${3:-}" ]]; then
  export APOLLO_LIB_PATH="$3${APOLLO_LIB_PATH:+:${APOLLO_LIB_PATH}}"
fi
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-87}"
WORK_DIR="$(mktemp -d)"
cd "${WORK_DIR}" || exit 2
PIDS=()
PASSED=0
FAILED=0

cleanup() {
  for pid in "${PIDS[@]:-}"; do
    [[ -n "${pid}" ]] && kill "${pid}" 2>/dev/null
  done
  wait 2>/dev/null
  PIDS=()
}
trap 'cleanup; rm -rf "${WORK_DIR}"' EXIT

start() {  # start <log> <command...>
  local log="$1"
  shift
  "$@" > "${log}" 2>&1 &
  PIDS+=("$!")
}

check() {  # check <description> <command...>: passes when the command does
  local description="$1"
  shift
  if "$@" > check.out 2>&1; then
    PASSED=$((PASSED + 1))
    echo "  ok    ${description}"
  else
    FAILED=$((FAILED + 1))
    echo "  FAIL  ${description}"
    sed 's/^/        /' check.out | tail -n 8
  fi
}

# retry <seconds> <command...>: until the command passes
retry() {
  local deadline=$((SECONDS + $1))
  shift
  until "$@"; do
    [[ ${SECONDS} -ge ${deadline} ]] && return 1
    sleep 0.5
  done
}

cli() { timeout 30 ros2 "$@" 2> /dev/null; }
has() { grep -q -- "$1"; }

run_suite() {
  echo "== ${RMW_IMPLEMENTATION}"

  # --- typed topics, remapping, node name -----------------------------------
  start talker.log "${BIN_DIR}/cyber_example_talker" --ros-args \
    -r __node:=my_talker -r /channel/chatter:=/renamed/chatter \
    -p gflags.minloglevel:=1
  check "the process node takes the name given with __node" \
    retry 20 bash -c "timeout 30 ros2 node list --no-daemon 2>/dev/null | grep -qx /my_talker"
  check "the Cyber node keeps its own name" \
    bash -c "timeout 30 ros2 node list --no-daemon 2>/dev/null | grep -qx /talker"
  check "the channel is a typed ROS 2 topic under its remapped name" \
    bash -c "timeout 30 ros2 topic list -t --no-daemon 2>/dev/null | grep -qF '/renamed/chatter [apollo_msgs/msg/CyberExamplesProtoChatter]'"
  check "the original topic name is not used" \
    bash -c "! timeout 30 ros2 topic list --no-daemon 2>/dev/null | grep -qx /channel/chatter"
  check "ros2 topic echo shows the message fields" \
    retry 60 bash -c "timeout 20 ros2 topic echo --once --no-daemon /renamed/chatter 2>/dev/null | grep -q '^seq: '"
  check "ros2 interface show knows the generated type" \
    bash -c "timeout 30 ros2 interface show apollo_msgs/msg/CyberExamplesProtoChatter 2>/dev/null | grep -q 'uint64 seq'"

  # --- gflags as parameters -------------------------------------------------
  check "a gflag given with -p is applied and readable (ros2 param get)" \
    bash -c "timeout 30 ros2 param get --no-daemon /my_talker gflags.minloglevel 2>/dev/null | grep -q 'Integer value is: 1'"
  check "ros2 param set changes a gflag" \
    bash -c "timeout 30 ros2 param set --no-daemon /my_talker gflags.minloglevel 2 2>/dev/null | grep -q 'Set parameter successful'"
  check "the new value is read back" \
    bash -c "timeout 30 ros2 param get --no-daemon /my_talker gflags.minloglevel 2>/dev/null | grep -q 'Integer value is: 2'"
  check "a wrong type is refused" \
    bash -c "timeout 30 ros2 param set --no-daemon /my_talker gflags.minloglevel abc 2>&1 | grep -qi 'fail'"
  check "the Cyber node lists the gflag it was given" \
    bash -c "timeout 30 ros2 param list --no-daemon /talker 2>/dev/null | grep -q 'gflags.minloglevel'"
  check "every node has use_sim_time" \
    bash -c "timeout 30 ros2 param get --no-daemon /talker use_sim_time 2>/dev/null | grep -q 'Boolean value is: False'"
  cleanup

  # --- ros2 topic pub -> Cyber listener, namespace --------------------------
  GLOG_alsologtostderr=1 start listener.log "${BIN_DIR}/cyber_example_listener" \
    --ros-args -r __ns:=/robot1
  check "the namespace applies to the Cyber node" \
    retry 20 bash -c "timeout 30 ros2 node list --no-daemon 2>/dev/null | grep -qx /robot1/listener"
  cli topic pub --once -w 1 /channel/chatter \
    apollo_msgs/msg/CyberExamplesProtoChatter \
    "{seq: 123, content: [104, 105]}" > /dev/null
  check "a Cyber reader receives what ros2 topic pub sends" \
    retry 10 grep -q "Received message seq-> 123" listener.log
  check "bytes arrive as bytes" grep -q "msgcontent->hi" listener.log
  cleanup

  # --- typed services ---------------------------------------------------------
  start service.log "${BIN_DIR}/cyber_example_service"
  check "the Cyber service has a native service type" \
    retry 20 bash -c "timeout 30 ros2 service list -t --no-daemon 2>/dev/null | grep -qF '/test_server [apollo_msgs/srv/CyberExamplesProtoDriver]'"
  check "ros2 service call gets the response of the Cyber service" \
    bash -c "timeout 30 ros2 service call /test_server apollo_msgs/srv/CyberExamplesProtoDriver '{msg_id: 41}' 2>/dev/null | grep -q 'msg_id='"
  cleanup

  # --- remapping inside one process -------------------------------------------
  # The transport tests of the build tree, started with remapping rules for
  # their channels and service: zero-copy delivery between a writer and a
  # reader of one process, raw readers, services and graph queries must all
  # follow the rules.
  if [[ -x "${BIN_DIR}/cyber_ros2_test" ]]; then
    check "readers, writers, services and graph queries of one process follow remapping rules" \
      "${BIN_DIR}/cyber_ros2_test" "--gtest_filter=TransportTest.*" --ros-args \
      -r /test/zero_copy:=/renamed/zero_copy -r /test/add:=/renamed/add \
      -r /test/history:=/renamed/history \
      -r /test/history_raw:=/renamed/history_raw \
      -r /test/topology:=/renamed/topology
  fi

  # --- parameter file, use_sim_time, mainboard ---------------------------------
  cat > params.yaml << 'EOF'
/**:
  ros__parameters:
    use_sim_time: true
    gflags.minloglevel: 3
EOF
  start mainboard.log "${BIN_DIR}/mainboard" -d "${EXAMPLE_DAG}" \
    --ros-args -r __node:=apollo_main --params-file "${WORK_DIR}/params.yaml"
  check "mainboard accepts --ros-args" \
    retry 20 bash -c "timeout 30 ros2 node list --no-daemon 2>/dev/null | grep -qx /apollo_main"
  check "a parameter file sets gflags" \
    bash -c "timeout 30 ros2 param get --no-daemon /apollo_main gflags.minloglevel 2>/dev/null | grep -q 'Integer value is: 3'"
  check "a parameter file switches simulation time on" \
    bash -c "timeout 30 ros2 param get --no-daemon /apollo_main use_sim_time 2>/dev/null | grep -q 'Boolean value is: True'"
  check "the process subscribes to /clock" \
    bash -c "timeout 30 ros2 topic info --no-daemon /clock 2>/dev/null | grep -q 'Subscription count: [1-9]'"
  cleanup
}

if [[ -n "${RMW_IMPLEMENTATION:-}" ]]; then
  run_suite
else
  for rmw in rmw_fastrtps_cpp rmw_cyclonedds_cpp; do
    if ros2 pkg prefix "${rmw}" > /dev/null 2>&1; then
      export RMW_IMPLEMENTATION="${rmw}"
      run_suite
      ROS_DOMAIN_ID=$((ROS_DOMAIN_ID + 1))
    fi
  done
fi

echo "${PASSED} passed, ${FAILED} failed"
[[ ${FAILED} -eq 0 ]]
