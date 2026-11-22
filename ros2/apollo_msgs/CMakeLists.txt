cmake_minimum_required(VERSION 3.16)
project(apollo_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# Native ROS 2 interfaces for the Apollo protobuf messages: one .msg per
# protobuf message and one .srv per Apollo service, generated at configure
# time by tools/ros2/proto2ros.py from the files listed in proto_files.txt
# and services.txt. With -DAPOLLO_MSGS_NATIVE_TYPES=OFF only the protobuf
# wrapper types (ProtoMessage, ProtoService) are built and Apollo falls back
# to them on the wire.
option(APOLLO_MSGS_NATIVE_TYPES
  "Generate native ROS 2 interfaces from the Apollo protobuf files" ON)

set(interfaces "msg/ProtoMessage.msg" "srv/ProtoService.srv")

if(APOLLO_MSGS_NATIVE_TYPES)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_package(Protobuf REQUIRED)
  if(TARGET protobuf::protoc)
    get_target_property(_protoc protobuf::protoc IMPORTED_LOCATION)
    if(NOT _protoc)
      get_target_property(_protoc protobuf::protoc IMPORTED_LOCATION_RELEASE)
    endif()
    if(NOT _protoc)
      get_target_property(_protoc protobuf::protoc IMPORTED_LOCATION_NOCONFIG)
    endif()
  endif()
  if(NOT _protoc)
    set(_protoc "${Protobuf_PROTOC_EXECUTABLE}")
  endif()
  if(NOT _protoc)
    find_program(_protoc protoc REQUIRED)
  endif()

  get_filename_component(APOLLO_SOURCE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
  set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/proto_interfaces")
  set(_generator "${APOLLO_SOURCE_ROOT}/tools/ros2/proto2ros.py")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${_generator}"
      --apollo_root "${APOLLO_SOURCE_ROOT}"
      --proto_list "${CMAKE_CURRENT_SOURCE_DIR}/proto_files.txt"
      --services "${CMAKE_CURRENT_SOURCE_DIR}/services.txt"
      --package ${PROJECT_NAME}
      --output_dir "${_gen_dir}"
      --protoc "${_protoc}"
    RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "tools/ros2/proto2ros.py failed (it needs the Python protobuf module)")
  endif()
  include("${_gen_dir}/interfaces.cmake")
  list(APPEND interfaces ${APOLLO_PROTO_INTERFACES})
  list(LENGTH APOLLO_PROTO_INTERFACES _count)
  message(STATUS "apollo_msgs: ${_count} native interfaces from Apollo protobuf files")

  # Regenerate when the protobuf files, the lists or the generator change.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/proto_files.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/services.txt"
    "${_generator}"
    ${APOLLO_PROTO_INTERFACE_INPUTS})

  # Registry (protobuf type <-> ROS 2 type, member layout) and descriptors,
  # found at run time through the ament index.
  install(FILES "${_gen_dir}/registry.txt" "${_gen_dir}/protos.desc"
    DESTINATION share/${PROJECT_NAME}/proto_interfaces)
  ament_index_register_resource(apollo_proto_types
    CONTENT "share/${PROJECT_NAME}/proto_interfaces")
endif()

rosidl_generate_interfaces(${PROJECT_NAME} ${interfaces})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
