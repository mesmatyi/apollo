# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
#
# Vendored third-party code compiled into the apollo package, and
# compatibility layers for third-party APIs that changed.
include_guard(GLOBAL)

set(_APOLLO_TP "${APOLLO_SOURCE_ROOT}/third_party")

function(_apollo_tp_library t)
  set_target_properties(${t} PROPERTIES
    OUTPUT_NAME ${t}
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    INSTALL_RPATH "$ORIGIN")
  install(TARGETS ${t} LIBRARY DESTINATION lib)
endfunction()

###############################################################################
# //third_party/tf2: Apollo's vendored ROS1-era tf2 (used by modules/transform
# and everything reading TF through apollo::transform::Buffer).
#
# It must not clash with ROS 2's tf2 (same namespace `tf2`, same library name
# libtf2.so, different ABI): it is built as libapollo_tf2.so and its namespace
# is renamed to `apollo_tf2` with the macro tf2=apollo_tf2, which is part of
# the target's public compile definitions, so every user of the vendored
# headers (tf2::BufferCore, tf2::TransformException, ...) consistently gets
# apollo_tf2::... symbols. ROS 2's tf2 headers live under include/tf2/tf2/ and
# are never on the include path of these targets.
###############################################################################
find_package(Boost QUIET)
add_library(apollo_tf2 SHARED
  "${_APOLLO_TP}/tf2/src/buffer_core.cpp"
  "${_APOLLO_TP}/tf2/src/cache.cpp"
  "${_APOLLO_TP}/tf2/src/static_cache.cpp"
  "${_APOLLO_TP}/tf2/src/time.cpp")
target_include_directories(apollo_tf2 PUBLIC
  "$<BUILD_INTERFACE:${_APOLLO_TP}/tf2/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}/third_party/tf2>")
target_compile_definitions(apollo_tf2 PUBLIC tf2=apollo_tf2)
if(TARGET Boost::headers)
  target_link_libraries(apollo_tf2 PUBLIC Boost::headers)
endif()
_apollo_tp_library(apollo_tf2)

###############################################################################
# //third_party/rtklib (GNSS RTCM decoding, used by modules/drivers/gnss)
###############################################################################
add_library(apollo_rtklib SHARED
  "${_APOLLO_TP}/rtklib/novatel.c"
  "${_APOLLO_TP}/rtklib/rcvraw.c"
  "${_APOLLO_TP}/rtklib/rtcm.c"
  "${_APOLLO_TP}/rtklib/rtcm3.c"
  "${_APOLLO_TP}/rtklib/rtkcmn.c")
target_include_directories(apollo_rtklib PUBLIC "$<BUILD_INTERFACE:${APOLLO_SOURCE_ROOT}>")
target_link_libraries(apollo_rtklib PUBLIC m)
_apollo_tp_library(apollo_rtklib)

###############################################################################
# //third_party/can_card_library/hermes_can: prebuilt libbcan.so, headers
# included as "hermes_can/bcan.h" (Bazel include_prefix).
###############################################################################
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(_bcan "${_APOLLO_TP}/can_card_library/hermes_can/lib_x86_64/libbcan.so")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(_bcan "${_APOLLO_TP}/can_card_library/hermes_can/lib_aarch64/libbcan.so")
endif()
if(_bcan AND EXISTS "${_bcan}")
  set(_inc "${CMAKE_BINARY_DIR}/third_party_include/hermes_can")
  file(MAKE_DIRECTORY "${_inc}/hermes_can")
  configure_file("${_APOLLO_TP}/can_card_library/hermes_can/include/bcan.h"
                 "${_inc}/hermes_can/bcan.h" COPYONLY)
  add_library(apollo_hermes_can INTERFACE)
  target_include_directories(apollo_hermes_can INTERFACE "$<BUILD_INTERFACE:${_inc}>")
  target_link_libraries(apollo_hermes_can INTERFACE "$<BUILD_INTERFACE:${_bcan}>")
  install(FILES "${_bcan}" DESTINATION lib)
endif()

###############################################################################
# OSQP 0.5/0.6 API (Apollo) on OSQP >= 1.0: modules/ros2_compat/osqp
###############################################################################
find_package(osqp QUIET)
if(TARGET osqp::osqp OR TARGET osqp::osqpstatic)
  if(TARGET osqp::osqp)
    set(_osqp osqp::osqp)
  else()
    set(_osqp osqp::osqpstatic)
  endif()
  add_library(apollo_osqp SHARED "${APOLLO_SOURCE_ROOT}/modules/ros2_compat/osqp/osqp_compat.cc")
  target_include_directories(apollo_osqp BEFORE PUBLIC
    "$<BUILD_INTERFACE:${APOLLO_SOURCE_ROOT}/modules/ros2_compat/osqp/include>")
  target_link_libraries(apollo_osqp PUBLIC ${_osqp})
  _apollo_tp_library(apollo_osqp)
endif()

###############################################################################
# PROJ 4 API (proj_api.h, used by map/gnss/localization) on PROJ >= 6:
# modules/ros2_compat/proj. PROJ 8 removed proj_api.h.
###############################################################################
find_package(PROJ QUIET CONFIG)
if(NOT TARGET PROJ::proj)
  find_package(PROJ4 QUIET CONFIG)
  if(TARGET PROJ4::proj)
    add_library(PROJ::proj ALIAS PROJ4::proj)
  endif()
endif()
if(TARGET PROJ::proj)
  get_target_property(_proj_inc PROJ::proj INTERFACE_INCLUDE_DIRECTORIES)
  find_file(APOLLO_PROJ_API_H proj_api.h HINTS ${_proj_inc} NO_CACHE)
  if(APOLLO_PROJ_API_H)
    add_library(apollo_proj INTERFACE)
    target_link_libraries(apollo_proj INTERFACE PROJ::proj)
  else()
    add_library(apollo_proj SHARED "${APOLLO_SOURCE_ROOT}/modules/ros2_compat/proj/proj_api_compat.cc")
    target_include_directories(apollo_proj BEFORE PUBLIC
      "$<BUILD_INTERFACE:${APOLLO_SOURCE_ROOT}/modules/ros2_compat/proj/include>")
    target_link_libraries(apollo_proj PUBLIC PROJ::proj)
    _apollo_tp_library(apollo_proj)
  endif()
endif()

# An empty translation unit for shared libraries that only aggregate deps
# (e.g. cc_binary(name = "control_module.so", linkshared = True, deps = [...])).
set(APOLLO_EMPTY_SOURCE "${CMAKE_BINARY_DIR}/apollo_empty.cc")
if(NOT EXISTS "${APOLLO_EMPTY_SOURCE}")
  file(WRITE "${APOLLO_EMPTY_SOURCE}" "// intentionally empty\n")
endif()
