cmake_minimum_required(VERSION 3.16)
project(cyber VERSION 11.0.0 LANGUAGES C CXX)

# The Apollo Cyber API implemented on ROS 2 (rclcpp).

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(CYBER_BUILD_EXAMPLES "Build the Cyber API examples" ON)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
# Defines PYTHON_INSTALL_DIR and registers the PYTHONPATH environment hook.
_ament_cmake_python_register_environment_hook()
find_package(ament_index_cpp REQUIRED)
find_package(apollo_msgs REQUIRED)
find_package(class_loader REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(rcpputils REQUIRED)
find_package(rosgraph_msgs REQUIRED)
find_package(rosidl_typesupport_introspection_cpp REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(tinyxml2 CONFIG QUIET)
if(NOT TARGET tinyxml2::tinyxml2)
  find_package(TinyXML2 REQUIRED)  # tinyxml2_vendor fallback
endif()
find_package(nlohmann_json REQUIRED)

# glog / gflags: CMake packages on Ubuntu 24.04 and conda; pkg-config fallback.
find_package(gflags CONFIG QUIET)
find_package(glog CONFIG QUIET)
if(NOT TARGET glog::glog)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLOG REQUIRED IMPORTED_TARGET libglog)
  add_library(glog::glog ALIAS PkgConfig::GLOG)
endif()
if(TARGET gflags::gflags)
  set(CYBER_GFLAGS gflags::gflags)
elseif(TARGET gflags)
  set(CYBER_GFLAGS gflags)
else()
  find_library(GFLAGS_LIBRARY gflags REQUIRED)
  set(CYBER_GFLAGS ${GFLAGS_LIBRARY})
endif()

# google::base::Logger::Write() differs between glog 0.5 / 0.6 / 0.7.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES glog::glog)
foreach(_api 3 2 1)
  if(_api EQUAL 3)
    set(_sig "bool, const std::chrono::system_clock::time_point&, const char*, size_t")
  elseif(_api EQUAL 2)
    set(_sig "bool, time_t, const char*, size_t")
  else()
    set(_sig "bool, time_t, const char*, int")
  endif()
  check_cxx_source_compiles("
    #include <chrono>
    #include <ctime>
    #include <glog/logging.h>
    struct L : google::base::Logger {
      void Write(${_sig}) override {}
      void Flush() override {}
      uint32_t LogSize() override { return 0; }
    };
    int main() { L l; return 0; }" CYBER_GLOG_API_${_api})
  if(CYBER_GLOG_API_${_api})
    set(CYBER_GLOG_LOGGER_API ${_api})
    break()
  endif()
endforeach()
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT CYBER_GLOG_LOGGER_API)
  message(FATAL_ERROR "unsupported glog version")
endif()
message(STATUS "glog Logger API level: ${CYBER_GLOG_LOGGER_API}")

get_filename_component(APOLLO_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CYBER_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)

# ---------------------------------------------------------------------------
# Protobuf messages of the Cyber API (cyber/proto)
# ---------------------------------------------------------------------------
file(GLOB CYBER_PROTOS ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
set(CYBER_PROTO_SRCS)
set(CYBER_PROTO_HDRS)
set(CYBER_PROTO_PY)
file(MAKE_DIRECTORY ${CYBER_GEN_DIR}/cyber/proto)
foreach(proto ${CYBER_PROTOS})
  get_filename_component(name ${proto} NAME_WE)
  set(out_cc ${CYBER_GEN_DIR}/cyber/proto/${name}.pb.cc)
  set(out_h ${CYBER_GEN_DIR}/cyber/proto/${name}.pb.h)
  set(out_py ${CYBER_GEN_DIR}/cyber/proto/${name}_pb2.py)
  add_custom_command(
    OUTPUT ${out_cc} ${out_h} ${out_py}
    COMMAND protobuf::protoc
    ARGS -I${APOLLO_SOURCE_ROOT} --cpp_out=${CYBER_GEN_DIR}
         --python_out=${CYBER_GEN_DIR} ${proto}
    DEPENDS ${proto} protobuf::protoc
    COMMENT "protoc ${name}.proto"
    VERBATIM)
  list(APPEND CYBER_PROTO_SRCS ${out_cc})
  list(APPEND CYBER_PROTO_HDRS ${out_h})
  list(APPEND CYBER_PROTO_PY ${out_py})
endforeach()
add_custom_target(cyber_proto_py ALL DEPENDS ${CYBER_PROTO_PY})

# ---------------------------------------------------------------------------
# libcyber
# ---------------------------------------------------------------------------
set(CYBER_SRCS
  binary.cc
  cyber.cc
  init.cc
  state.cc
  class_loader/class_loader.cc
  class_loader/class_loader_manager.cc
  class_loader/shared_library/shared_library.cc
  class_loader/utility/class_factory.cc
  class_loader/utility/class_loader_utility.cc
  common/file.cc
  common/global_data.cc
  component/timer_component.cc
  io/session.cc
  logger/async_logger.cc
  logger/log_file_object.cc
  logger/logger.cc
  logger/logger_util.cc
  message/protobuf_factory.cc
  node/node.cc
  parameter/parameter.cc
  parameter/parameter_client.cc
  parameter/parameter_server.cc
  plugin_manager/plugin_description.cc
  plugin_manager/plugin_manager.cc
  profiler/block.cc
  profiler/block_manager.cc
  profiler/frame.cc
  record/file/record_file_base.cc
  record/file/record_file_reader.cc
  record/file/record_file_writer.cc
  record/header_builder.cc
  record/record_reader.cc
  record/record_viewer.cc
  record/record_writer.cc
  ros2/component_registry.cc
  ros2/naming.cc
  ros2/native_types.cc
  ros2/parameters.cc
  ros2/paths.cc
  ros2/rosout.cc
  ros2/runtime.cc
  ros2/transport.cc
  service_discovery/topology_manager.cc
  task/task_manager.cc
  time/clock.cc
  time/duration.cc
  time/rate.cc
  time/time.cc
  timer/timer.cc
  transport/qos/qos_profile_conf.cc
  tools/cyber_recorder/info.cc
  tools/cyber_recorder/recorder.cc
  tools/cyber_recorder/recoverer.cc
  tools/cyber_recorder/spliter.cc
  tools/cyber_recorder/player/play_task.cc
  tools/cyber_recorder/player/play_task_buffer.cc
  tools/cyber_recorder/player/play_task_consumer.cc
  tools/cyber_recorder/player/play_task_producer.cc
  tools/cyber_recorder/player/player.cc
)

add_library(cyber SHARED ${CYBER_SRCS} ${CYBER_PROTO_SRCS})
target_include_directories(cyber PUBLIC
  $<BUILD_INTERFACE:${APOLLO_SOURCE_ROOT}>
  $<BUILD_INTERFACE:${CYBER_GEN_DIR}>
  $<INSTALL_INTERFACE:include/cyber>)
target_link_libraries(cyber PUBLIC
  rclcpp::rclcpp
  rclcpp_components::component
  class_loader::class_loader
  ${apollo_msgs_TARGETS}
  protobuf::libprotobuf
  glog::glog
  ${CYBER_GFLAGS}
  Threads::Threads
  ${CMAKE_DL_LIBS}
  rt)
target_link_libraries(cyber PRIVATE
  ament_index_cpp::ament_index_cpp
  rcpputils::rcpputils
  rosidl_typesupport_introspection_cpp::rosidl_typesupport_introspection_cpp
  ${rosgraph_msgs_TARGETS}
  ${rcl_interfaces_TARGETS}
  nlohmann_json::nlohmann_json
  tinyxml2::tinyxml2
  uuid)
# Cyber's profiler macros are opt-in, as in the Bazel build.
target_compile_definitions(cyber PUBLIC ENABLE_PROFILER=0
  CYBER_GLOG_LOGGER_API=${CYBER_GLOG_LOGGER_API})

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------
add_executable(mainboard
  mainboard/mainboard.cc
  mainboard/module_argument.cc
  mainboard/module_controller.cc)
target_link_libraries(mainboard cyber)

# cyber_recorder: record / play / info / split / recover .record files over
# ROS 2 (rosbag2 conversion: tools/cyber_recorder/*_rosbag2*.py).
add_executable(cyber_recorder tools/cyber_recorder/main.cc)
target_link_libraries(cyber_recorder cyber)

# The profiler flags of mainboard and cyber_recorder use gperftools. Linking
# it swaps the allocator of the process for tcmalloc, so it is opt-in.
option(CYBER_ENABLE_GPERFTOOLS
  "Link mainboard and cyber_recorder with gperftools" OFF)
foreach(tool mainboard cyber_recorder)
  if(CYBER_ENABLE_GPERFTOOLS)
    find_library(CYBER_TCMALLOC_PROFILER_LIB tcmalloc_and_profiler REQUIRED)
    target_link_libraries(${tool} ${CYBER_TCMALLOC_PROFILER_LIB})
    target_compile_definitions(${tool} PRIVATE APOLLO_HAS_GPERFTOOLS=1)
  else()
    target_compile_definitions(${tool} PRIVATE APOLLO_HAS_GPERFTOOLS=0)
  endif()
endforeach()

if(CYBER_BUILD_EXAMPLES)
  set(EXAMPLE_GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
  file(MAKE_DIRECTORY ${EXAMPLE_GEN}/cyber/examples/proto)
  add_custom_command(
    OUTPUT ${EXAMPLE_GEN}/cyber/examples/proto/examples.pb.cc
           ${EXAMPLE_GEN}/cyber/examples/proto/examples.pb.h
    COMMAND protobuf::protoc
    ARGS -I${APOLLO_SOURCE_ROOT} --cpp_out=${EXAMPLE_GEN}
         ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/examples.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/examples.proto
    VERBATIM)
  add_custom_command(
    OUTPUT ${EXAMPLE_GEN}/cyber/examples/proto/examples_pb2.py
    COMMAND protobuf::protoc
    ARGS -I${APOLLO_SOURCE_ROOT} --python_out=${EXAMPLE_GEN}
         ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/examples.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/examples.proto
    VERBATIM)
  add_custom_target(cyber_examples_proto_py ALL
    DEPENDS ${EXAMPLE_GEN}/cyber/examples/proto/examples_pb2.py)
  install(FILES ${EXAMPLE_GEN}/cyber/examples/proto/examples_pb2.py
    DESTINATION ${PYTHON_INSTALL_DIR}/cyber/examples/proto)
  add_library(cyber_examples_proto SHARED
    ${EXAMPLE_GEN}/cyber/examples/proto/examples.pb.cc)
  target_link_libraries(cyber_examples_proto PUBLIC cyber)

  foreach(example talker listener service paramserver record)
    add_executable(cyber_example_${example} examples/${example}.cc)
    target_link_libraries(cyber_example_${example} cyber_examples_proto)
  endforeach()

  add_library(common_component_example SHARED
    examples/common_component_example/common_component_example.cc)
  target_link_libraries(common_component_example cyber_examples_proto)
  add_library(timer_component_example SHARED
    examples/timer_component_example/timer_component_example.cc)
  target_link_libraries(timer_component_example cyber_examples_proto)
  foreach(writer channel_test_writer channel_prediction_writer)
    add_executable(cyber_example_${writer}
      examples/common_component_example/${writer}.cc)
    target_link_libraries(cyber_example_${writer} cyber_examples_proto)
  endforeach()

  install(TARGETS cyber_examples_proto common_component_example
                  timer_component_example
    LIBRARY DESTINATION lib)
  install(TARGETS
    cyber_example_talker cyber_example_listener cyber_example_service
    cyber_example_paramserver cyber_example_record
    cyber_example_channel_test_writer cyber_example_channel_prediction_writer
    RUNTIME DESTINATION lib/${PROJECT_NAME})
  # ROS 2 component index for the example components.
  set(_index ${CMAKE_CURRENT_BINARY_DIR}/ament_index/rclcpp_components)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/rclcpp_components_cyber
    "CommonComponentSample;lib/libcommon_component_example.so\n"
    "TimerComponentSample;lib/libtimer_component_example.so\n")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rclcpp_components_cyber
    DESTINATION share/ament_index/resource_index/rclcpp_components
    RENAME ${PROJECT_NAME})
  install(DIRECTORY examples/common_component_example
                    examples/timer_component_example
    DESTINATION share/${PROJECT_NAME}/examples
    FILES_MATCHING PATTERN "*.dag" PATTERN "*.launch" PATTERN "*.launch.py")
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS cyber
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(TARGETS mainboard cyber_recorder RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS mainboard cyber_recorder RUNTIME DESTINATION bin)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION include/cyber/cyber
  FILES_MATCHING PATTERN "*.h"
  PATTERN "python" EXCLUDE
  PATTERN "docs" EXCLUDE
  PATTERN "doxy-docs" EXCLUDE
  PATTERN "test" EXCLUDE)
install(FILES ${CYBER_PROTO_HDRS} DESTINATION include/cyber/cyber/proto)
install(FILES ${CYBER_PROTOS} DESTINATION share/${PROJECT_NAME}/proto/cyber/proto)
install(FILES conf/cyber.pb.conf DESTINATION share/${PROJECT_NAME}/conf)

# Python: cyber.proto.*_pb2 and cyber.python.cyber_py3.
# `cyber` and `cyber.proto` are namespace packages (no __init__.py) so other
# installs or a source checkout can contribute cyber.* modules too.
set(_py_root ${CMAKE_CURRENT_BINARY_DIR}/python)
file(WRITE ${_py_root}/__init__.py "")
install(FILES ${CYBER_PROTO_PY} DESTINATION ${PYTHON_INSTALL_DIR}/cyber/proto)
install(FILES ${_py_root}/__init__.py DESTINATION ${PYTHON_INSTALL_DIR}/cyber/python)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/python/cyber_py3)
  install(DIRECTORY python/cyber_py3
    DESTINATION ${PYTHON_INSTALL_DIR}/cyber/python
    FILES_MATCHING PATTERN "*.py"
    PATTERN "test" EXCLUDE
    PATTERN "__pycache__" EXCLUDE)
endif()

# Python command line tools (cyber_channel, cyber_node, cyber_service,
# cyber_launch, cyber_recorder converters).
file(GLOB CYBER_PY_TOOLS
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cyber_channel/cyber_channel*
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cyber_node/cyber_node*
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cyber_service/cyber_service*
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cyber_launch/cyber_launch*
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cyber_recorder/*.py)
foreach(tool ${CYBER_PY_TOOLS})
  get_filename_component(tool_name ${tool} NAME)
  if(tool_name MATCHES "BUILD|README|\\.md$|_test\\.py$|^test_")
    continue()
  endif()
  string(REGEX REPLACE "\\.py$" "" tool_exe ${tool_name})
  install(PROGRAMS ${tool} DESTINATION lib/${PROJECT_NAME} RENAME ${tool_exe})
  install(PROGRAMS ${tool} DESTINATION bin RENAME ${tool_exe})
endforeach()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  file(MAKE_DIRECTORY ${CYBER_GEN_DIR}/cyber/ros2/test)
  add_custom_command(
    OUTPUT ${CYBER_GEN_DIR}/cyber/ros2/test/test_msgs.pb.cc
           ${CYBER_GEN_DIR}/cyber/ros2/test/test_msgs.pb.h
    COMMAND protobuf::protoc
    ARGS -I${APOLLO_SOURCE_ROOT} --cpp_out=${CYBER_GEN_DIR}
         ${CMAKE_CURRENT_SOURCE_DIR}/ros2/test/test_msgs.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ros2/test/test_msgs.proto
    VERBATIM)
  # The native-type tests use the examples protos (which have generated
  # ROS 2 interfaces); the test compiles its own copy.
  file(MAKE_DIRECTORY ${CYBER_GEN_DIR}/test_protos)
  add_custom_command(
    OUTPUT ${CYBER_GEN_DIR}/test_protos/cyber/examples/proto/examples.pb.cc
           ${CYBER_GEN_DIR}/test_protos/cyber/examples/proto/examples.pb.h
    COMMAND protobuf::protoc
    ARGS -I${APOLLO_SOURCE_ROOT} --cpp_out=${CYBER_GEN_DIR}/test_protos
         ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/examples.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/examples.proto
    VERBATIM)
  ament_add_gtest(cyber_ros2_test
    ros2/test/naming_test.cc
    ros2/test/transport_test.cc
    ros2/test/component_test.cc
    ros2/test/native_types_test.cc
    ros2/test/ros2_features_test.cc
    ${CYBER_GEN_DIR}/cyber/ros2/test/test_msgs.pb.cc
    ${CYBER_GEN_DIR}/test_protos/cyber/examples/proto/examples.pb.cc
    TIMEOUT 300)
  target_include_directories(cyber_ros2_test PRIVATE
    ${CYBER_GEN_DIR}/test_protos)
  target_link_libraries(cyber_ros2_test cyber)

  # The executables as the stock `ros2` command line tools see them: typed
  # topics and services, ROS arguments, gflags as parameters.
  find_program(CYBER_ROS2_CLI ros2)
  if(CYBER_ROS2_CLI AND CYBER_BUILD_EXAMPLES)
    add_test(NAME cyber_cli_interop
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ros2/test/cli_interop_test.sh
              $<TARGET_FILE_DIR:mainboard>
              ${CMAKE_CURRENT_SOURCE_DIR}/examples/common_component_example/common.dag
              $<TARGET_FILE_DIR:common_component_example>)
    set_tests_properties(cyber_cli_interop PROPERTIES TIMEOUT 900)
  endif()
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp rclcpp_components class_loader apollo_msgs
  Protobuf gflags glog)
ament_package(CONFIG_EXTRAS cmake/cyber-extras.cmake)
