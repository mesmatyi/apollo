# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
#
# Maps the external Bazel repositories used by modules/**/BUILD (@eigen,
# @boost, @opencv, @local_config_pcl, ...) and the vendored //third_party
# packages to CMake targets. A repository that cannot be found is declared
# MISSING with a reason; every target that (transitively) depends on it is
# skipped and listed in the configure summary.
include_guard(GLOBAL)

find_package(PkgConfig QUIET)

macro(_apollo_ext_targets label)
  set(_items)
  set(_missing)
  foreach(_t ${ARGN})
    if(TARGET ${_t})
      list(APPEND _items ${_t})
    else()
      list(APPEND _missing ${_t})
    endif()
  endforeach()
  if(_items AND NOT _missing)
    apollo_external("${label}" ${_items})
  else()
    apollo_external("${label}" MISSING "CMake target(s) ${_missing} not found")
  endif()
endmacro()

function(_apollo_pkg_config label module)
  string(MAKE_C_IDENTIFIER "APOLLO_PC_${module}" var)
  if(PkgConfig_FOUND)
    pkg_check_modules(${var} QUIET IMPORTED_TARGET GLOBAL ${module})
  endif()
  if(${var}_FOUND)
    apollo_external("${label}" PkgConfig::${var})
  else()
    apollo_external("${label}" MISSING "pkg-config module ${module} not found")
  endif()
endfunction()

###############################################################################
# Always required
###############################################################################
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
apollo_external("@com_google_protobuf//:protobuf" protobuf::libprotobuf)
apollo_external("@com_google_protobuf//:any_proto" protobuf::libprotobuf)
apollo_external("@com_google_protobuf" protobuf::libprotobuf)

find_package(gflags QUIET)
if(TARGET gflags::gflags_shared)
  apollo_external("@com_github_gflags_gflags" gflags::gflags_shared)
elseif(TARGET gflags)
  apollo_external("@com_github_gflags_gflags" gflags)
else()
  apollo_external("@com_github_gflags_gflags" MISSING "gflags not found")
endif()

find_package(glog QUIET)
_apollo_ext_targets("@com_github_google_glog" glog::glog)

find_package(GTest QUIET)
_apollo_ext_targets("@com_google_googletest//:gtest" GTest::gtest)
_apollo_ext_targets("@com_google_googletest//:gtest_main" GTest::gtest_main)

find_package(Eigen3 QUIET NO_MODULE)
_apollo_ext_targets("@eigen" Eigen3::Eigen)

find_package(absl QUIET)
set(_absl)
foreach(_c base strings str_format time synchronization flat_hash_map
        flat_hash_set node_hash_map node_hash_set btree hash optional span
        memory status statusor any variant utility algorithm container_memory
        inlined_vector fixed_array cord random_random log check)
  if(TARGET absl::${_c})
    list(APPEND _absl absl::${_c})
  endif()
endforeach()
if(_absl)
  apollo_external("@com_google_absl" ${_absl})
else()
  apollo_external("@com_google_absl" MISSING "abseil not found")
endif()

find_package(Boost QUIET COMPONENTS filesystem program_options regex system thread)
if(Boost_FOUND)
  apollo_external("@boost" Boost::headers Boost::filesystem Boost::program_options
                  Boost::regex Boost::system Boost::thread)
else()
  apollo_external("@boost" MISSING "Boost not found")
endif()

find_package(nlohmann_json QUIET)
_apollo_ext_targets("@com_github_nlohmann_json" nlohmann_json::nlohmann_json)

find_package(yaml-cpp QUIET)
if(TARGET yaml-cpp::yaml-cpp)
  apollo_external("@com_github_jbeder_yaml_cpp" yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
  apollo_external("@com_github_jbeder_yaml_cpp" yaml-cpp)
else()
  apollo_external("@com_github_jbeder_yaml_cpp" MISSING "yaml-cpp not found")
endif()

find_package(tinyxml2 QUIET)
_apollo_ext_targets("@tinyxml2" tinyxml2::tinyxml2)

find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs highgui)
foreach(_c core imgproc imgcodecs highgui)
  _apollo_ext_targets("@opencv//:${_c}" opencv_${_c})
endforeach()
apollo_external("@opencv" MISSING "unknown OpenCV component")

# PROJ, with the proj_api.h compatibility layer when needed (ApolloThirdParty)
_apollo_ext_targets("@proj" apollo_proj)

# (conda's FFTW3Config.cmake has an imported target without a location)
_apollo_pkg_config("@fftw3" fftw3)

find_package(SQLite3 QUIET)
_apollo_ext_targets("@sqlite3" SQLite::SQLite3)

find_package(ZLIB QUIET)
_apollo_ext_targets("@zlib" ZLIB::ZLIB)

_apollo_pkg_config("@ipopt" ipopt)
_apollo_pkg_config("@portaudio" portaudio-2.0)
foreach(_c avcodec avformat avutil swscale)
  _apollo_pkg_config("@ffmpeg//:${_c}" lib${_c})
endforeach()

# OSQP: Apollo is written against the OSQP 0.5/0.6 API. When only OSQP >= 1.0
# is installed, the compatibility layer in modules/ros2_compat/osqp provides
# the old API on top of it (see ApolloThirdParty.cmake).
if(TARGET apollo_osqp)
  apollo_external("@osqp" apollo_osqp)
else()
  apollo_external("@osqp" MISSING "OSQP not found")
endif()

# ADOL-C (automatic differentiation for the IPOPT smoothers in planning).
find_path(ADOLC_INCLUDE_DIR adolc/adolc.h
          HINTS ${ADOLC_ROOT} $ENV{ADOLC_ROOT} PATH_SUFFIXES include)
find_library(ADOLC_LIBRARY NAMES adolc
             HINTS ${ADOLC_ROOT} $ENV{ADOLC_ROOT} PATH_SUFFIXES lib lib64)
if(ADOLC_INCLUDE_DIR AND ADOLC_LIBRARY)
  add_library(apollo_ext_adolc INTERFACE)
  target_include_directories(apollo_ext_adolc SYSTEM INTERFACE "${ADOLC_INCLUDE_DIR}")
  target_link_libraries(apollo_ext_adolc INTERFACE "${ADOLC_LIBRARY}")
  apollo_external("@adolc" apollo_ext_adolc)
else()
  apollo_external("@adolc" MISSING "ADOL-C not found (tools/ros2/install_adolc.sh, or set ADOLC_ROOT)")
endif()

# civetweb (dreamview, monitor, task_manager): C and C++ (CivetServer) API
find_path(CIVETWEB_INCLUDE_DIR CivetServer.h HINTS ${CIVETWEB_ROOT} $ENV{CIVETWEB_ROOT}
          PATH_SUFFIXES include)
find_library(CIVETWEB_LIBRARY NAMES civetweb HINTS ${CIVETWEB_ROOT} $ENV{CIVETWEB_ROOT}
             PATH_SUFFIXES lib)
find_library(CIVETWEB_CPP_LIBRARY NAMES civetweb-cpp HINTS ${CIVETWEB_ROOT} $ENV{CIVETWEB_ROOT}
             PATH_SUFFIXES lib)
if(CIVETWEB_INCLUDE_DIR AND CIVETWEB_LIBRARY)
  add_library(apollo_ext_civetweb INTERFACE)
  target_include_directories(apollo_ext_civetweb SYSTEM INTERFACE "${CIVETWEB_INCLUDE_DIR}")
  if(CIVETWEB_CPP_LIBRARY)  # Ubuntu splits the C++ API into libcivetweb-cpp
    target_link_libraries(apollo_ext_civetweb INTERFACE "${CIVETWEB_CPP_LIBRARY}")
  endif()
  target_link_libraries(apollo_ext_civetweb INTERFACE "${CIVETWEB_LIBRARY}")
  apollo_external("@civetweb" apollo_ext_civetweb)
else()
  apollo_external("@civetweb" MISSING "civetweb not found (tools/ros2/install_civetweb.sh)")
endif()

# PCL (all components the installed PCL provides)
find_package(PCL QUIET)
if(PCL_FOUND)
  add_library(apollo_ext_pcl INTERFACE)
  target_include_directories(apollo_ext_pcl SYSTEM INTERFACE ${PCL_INCLUDE_DIRS})
  target_link_libraries(apollo_ext_pcl INTERFACE ${PCL_LIBRARIES})
  target_compile_definitions(apollo_ext_pcl INTERFACE ${PCL_DEFINITIONS})
  apollo_external("@local_config_pcl" apollo_ext_pcl)
else()
  apollo_external("@local_config_pcl" MISSING "PCL not found")
endif()

find_package(Qt5 QUIET COMPONENTS Core Gui Widgets OpenGL)
_apollo_ext_targets("@qt//:qt_core" Qt5::Core)
_apollo_ext_targets("@qt//:qt_gui" Qt5::Gui)
_apollo_ext_targets("@qt//:qt_widgets" Qt5::Widgets)
_apollo_ext_targets("@qt//:qt_opengl" Qt5::OpenGL)

find_package(fastrtps QUIET)
_apollo_ext_targets("@fastrtps" fastrtps)

find_package(gRPC QUIET CONFIG)
_apollo_ext_targets("@com_github_grpc_grpc//:grpc++" gRPC::grpc++)

###############################################################################
# Not available in a ROS 2 / CPU-only environment
###############################################################################
foreach(_repo local_config_cuda local_config_rocm local_config_tensorrt npp nvjpeg)
  apollo_external("@${_repo}" MISSING "requires CUDA/ROCm/TensorRT (no GPU support)")
endforeach()
# libtorch: find_package(Torch) (e.g. CMAKE_PREFIX_PATH=<torch>/share/cmake).
# Without CUDA, @libtorch_gpu also resolves to the CPU libtorch (Bazel's
# if_gpu() picks libtorch_cpu on CPU-only builds).
find_package(Torch QUIET)
if(Torch_FOUND OR TORCH_FOUND)
  add_library(apollo_ext_torch INTERFACE)
  if(TARGET torch_cpu AND TARGET c10)
    target_link_libraries(apollo_ext_torch INTERFACE torch_cpu c10)
  else()
    target_link_libraries(apollo_ext_torch INTERFACE ${TORCH_LIBRARIES})
  endif()
  if(TORCH_INCLUDE_DIRS)
    target_include_directories(apollo_ext_torch SYSTEM INTERFACE ${TORCH_INCLUDE_DIRS})
  endif()
  apollo_external("@libtorch_cpu" apollo_ext_torch)
  if(APOLLO_WITH_cuda AND TARGET torch_cuda)
    apollo_external("@libtorch_gpu" torch)
  else()
    apollo_external("@libtorch_gpu" apollo_ext_torch)
  endif()
else()
  foreach(_repo libtorch_cpu libtorch_gpu)
    apollo_external("@${_repo}" MISSING "requires libtorch (find_package(Torch) failed)")
  endforeach()
endif()
foreach(_repo paddleinference-x86_64 paddleinference-aarch64
        centerpoint_infer_op-x86_64 centerpoint_infer_op-aarch64
        caddn_infer_op-x86_64 caddn_infer_op-aarch64)
  apollo_external("@${_repo}" MISSING "requires Paddle inference")
endforeach()
apollo_external("@local_config_vtk" MISSING "VTK not supported")
apollo_external("@ad_rss_lib" MISSING "ad_rss_lib not available")
apollo_external("@localization_msf" MISSING "proprietary localization_msf library not available")
apollo_external("@adv_plat" MISSING "proprietary adv_plat library not available")

###############################################################################
# Vendored //third_party packages (see ApolloThirdParty.cmake)
###############################################################################
apollo_external("//third_party/tf2:tf2" apollo_tf2)
apollo_external("//third_party/rtklib:rtklib" apollo_rtklib)
if(TARGET apollo_hermes_can)
  apollo_external("//third_party/can_card_library/hermes_can:hermes_can" apollo_hermes_can)
else()
  apollo_external("//third_party/can_card_library/hermes_can:hermes_can"
                  MISSING "prebuilt libbcan.so not available for ${CMAKE_SYSTEM_PROCESSOR}")
endif()
apollo_external("//third_party/camera_library/smartereye:smartereye"
                MISSING "smartereye SDK libraries are not in the source tree")
