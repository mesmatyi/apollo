# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
#
# ament_cmake superbuild of all Apollo modules (colcon package "apollo").
#
# The Bazel BUILD files stay the source of truth: tools/ros2/bazel2cmake.py
# turns each modules/<pkg>/BUILD into a declarative modules/<pkg>/BUILD.cmake.
# This file reads all declarations, resolves Bazel labels and external
# repositories, skips (transitively) what cannot be built here, and creates
# the remaining targets. See cmake/ApolloTargets.cmake.
#
# Options:
#   APOLLO_BUILD_TESTS   build cc_test targets with ament_add_gtest (OFF)
#   APOLLO_BUILD_ONLY    list of Bazel package prefixes to build (plus their
#                        dependencies), e.g. "modules/control;modules/canbus"
#   APOLLO_REQUIRE_CYBER fail when the cyber package is missing (ON)
#   APOLLO_WITH_<cond>   select() conditions: GPU, CUDA, ROCM, TELEOP,
#                        PROFILER, ESD_CAN (OFF); x86_64/aarch64 automatic
cmake_minimum_required(VERSION 3.16)
project(apollo LANGUAGES C CXX)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)  # use BoostConfig.cmake
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

option(APOLLO_BUILD_TESTS "Build the cc_test targets (ament_add_gtest)" OFF)
option(APOLLO_REQUIRE_CYBER "Fail if the cyber package cannot be found" ON)
set(APOLLO_BUILD_ONLY "" CACHE STRING
    "Bazel package prefixes to build (empty: everything that can be built)")

# select() conditions used by the BUILD files
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(APOLLO_WITH_x86_64 ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(APOLLO_WITH_aarch64 ON)
endif()
foreach(_cond gpu cuda rocm teleop profiler esd_can)
  string(TOUPPER "${_cond}" _COND)
  option(APOLLO_WITH_${_COND} "select() condition ${_cond}" OFF)
  set(APOLLO_WITH_${_cond} ${APOLLO_WITH_${_COND}})
endforeach()

get_filename_component(APOLLO_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." REALPATH)

# Apollo sources are not warning-clean; keep the few -Werror flags Bazel used
# out, and silence the noisiest GCC 13 warnings.
add_compile_options(
  "$<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>"
  "$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>")
# Apollo's x86 code uses SSE4 intrinsics unconditionally (e.g.
# perception/common/algorithm/i_lib); conda's default is -march=nocona.
# Applied to the BUILD.cmake targets (not to the generated protos).
if(APOLLO_WITH_x86_64)
  set(APOLLO_TARGET_COMPILE_OPTIONS -msse4.2)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
# defines PYTHON_INSTALL_DIR and registers the PYTHONPATH environment hook
# (the *_pb2.py and py_library sources are installed under modules/ there)
_ament_cmake_python_register_environment_hook()

if(APOLLO_REQUIRE_CYBER)
  find_package(cyber REQUIRED)
else()
  find_package(cyber QUIET)
  if(NOT cyber_FOUND)
    message(WARNING "Apollo: package cyber not found; every target using //cyber is skipped")
  endif()
endif()
# Tests: plain gtest executables registered with add_test() (ctest / colcon
# test), labelled with their module: `ctest -L '^control$'`. They run from the
# repository root with the Apollo runtime environment, so /apollo/... paths
# resolve to $APOLLO_ROOT/... through the cyber file utilities.
set(APOLLO_TEST_TIMEOUT 300 CACHE STRING "Per-test timeout (seconds)")
if(APOLLO_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  if(cyber_DIR)
    get_filename_component(_cyber_share "${cyber_DIR}/.." ABSOLUTE)
  endif()
  set(APOLLO_TEST_ENVIRONMENT
    "APOLLO_ROOT=${APOLLO_SOURCE_ROOT}"
    "CYBER_PATH=${_cyber_share}"
    "APOLLO_CONF_PATH=${APOLLO_SOURCE_ROOT}"
    "APOLLO_FLAG_PATH=${APOLLO_SOURCE_ROOT}"
    "APOLLO_PLUGIN_DESCRIPTION_PATH=${APOLLO_SOURCE_ROOT}"
    "APOLLO_PLUGIN_INDEX_PATH=${CMAKE_BINARY_DIR}/cyber_plugin_index"
    "APOLLO_PLUGIN_LIB_PATH=${CMAKE_BINARY_DIR}/lib/apollo"
    "APOLLO_LIB_PATH=${CMAKE_BINARY_DIR}/lib/apollo"
    "GLOG_logtostderr=1"
    # conda-forge's MUMPS 5.8 (IPOPT's linear solver) deadlocks in its OpenMP
    # L0 factorization with more than one thread (see module_status.md).
    "OMP_NUM_THREADS=1")
endif()

include(cmake/ApolloTargets.cmake)
include(cmake/ApolloThirdParty.cmake)
include(cmake/ApolloExternalDeps.cmake)
include(cmake/ApolloProtos.cmake)

# protos
include(cmake/generated/apollo_protos.cmake)
apollo_build_protos()

# target declarations (one BUILD.cmake per Bazel package)
include(cmake/generated/apollo_packages.cmake)
foreach(_pkg IN LISTS APOLLO_BAZEL_PACKAGES)
  include("${APOLLO_SOURCE_ROOT}/${_pkg}/BUILD.cmake")
endforeach()

apollo_resolve_targets()
apollo_create_targets()

# launch files written next to the BUILD files (modules/**/launch/*.launch.py)
file(GLOB_RECURSE _launch_files CONFIGURE_DEPENDS
     RELATIVE "${APOLLO_SOURCE_ROOT}"
     "${APOLLO_SOURCE_ROOT}/modules/*.launch.py")
foreach(_f IN LISTS _launch_files)
  get_filename_component(_dir "${_f}" DIRECTORY)
  install(FILES "${APOLLO_SOURCE_ROOT}/${_f}" DESTINATION "share/${PROJECT_NAME}/${_dir}")
endforeach()

apollo_print_summary()

# Exported for other ROS 2 packages (e.g. apollo_ros2_bridge):
#   find_package(apollo REQUIRED) -> apollo::apollo_proto (all modules/ protos
#   except the few clashing ones, which are apollo::apollo_proto_<path>), with
#   headers under include/apollo (#include "modules/.../x.pb.h"), and
#   apollo_PROTO_IMPORT_DIR (share/apollo/proto) holding modules/**/*.proto.
#   Never compile a second copy of these protos into a process that loads
#   Apollo libraries: duplicate descriptor registration aborts.
ament_export_targets(apollo_targets HAS_LIBRARY_TARGET)
ament_export_dependencies(Protobuf cyber)
ament_package(CONFIG_EXTRAS cmake/apollo-extras.cmake)
