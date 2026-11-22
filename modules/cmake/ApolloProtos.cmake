# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
#
# Compiles every .proto file under modules/ (listed by the generated
# cmake/generated/apollo_protos.cmake) with protoc:
#   * C++: one shared library `apollo_proto` (libapollo_proto.so) with all
#     files, except the few files whose symbols clash with another file; each
#     of those gets its own library (see tools/ros2/bazel2cmake.py).
#     Generated headers: ${APOLLO_PROTO_GEN_DIR}/modules/.../*.pb.h, installed
#     to include/apollo/modules/...
#   * Python: *_pb2.py installed to <site-packages>/modules/...
include_guard(GLOBAL)

set(APOLLO_PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/proto_gen")
set(APOLLO_PROTO_PY_DIR "${CMAKE_BINARY_DIR}/proto_py")

function(apollo_proto_file file lib)
  set_property(GLOBAL APPEND PROPERTY APOLLO_PROTO_FILES "${file}")
  set_property(GLOBAL APPEND PROPERTY APOLLO_PROTO_LIBRARIES "${lib}")
  set_property(GLOBAL PROPERTY "APOLLO_PROTO_LIB::${file}" "${lib}")
  set_property(GLOBAL PROPERTY "APOLLO_PROTO_IMPORTS::${file}" "${ARGN}")
endfunction()

function(apollo_build_protos)
  get_property(files GLOBAL PROPERTY APOLLO_PROTO_FILES)
  get_property(libs GLOBAL PROPERTY APOLLO_PROTO_LIBRARIES)
  list(REMOVE_DUPLICATES libs)
  file(MAKE_DIRECTORY "${APOLLO_PROTO_GEN_DIR}" "${APOLLO_PROTO_PY_DIR}")

  if(TARGET protobuf::protoc)
    set(protoc protobuf::protoc)
  else()
    set(protoc "${Protobuf_PROTOC_EXECUTABLE}")
  endif()
  set(import_dirs -I "${APOLLO_SOURCE_ROOT}")
  foreach(d IN LISTS Protobuf_INCLUDE_DIRS)
    list(APPEND import_dirs -I "${d}")
  endforeach()
  # modules/ protos import only modules/ and google/protobuf protos; the
  # cyber import root (cyber_PROTO_IMPORT_DIR) is not needed.

  set(py_outputs)
  foreach(lib IN LISTS libs)
    set(_srcs_${lib})
    set(_deps_${lib})
  endforeach()
  foreach(f IN LISTS files)
    get_property(lib GLOBAL PROPERTY "APOLLO_PROTO_LIB::${f}")
    get_property(imports GLOBAL PROPERTY "APOLLO_PROTO_IMPORTS::${f}")
    string(REGEX REPLACE "\\.proto$" "" stem "${f}")
    set(cc "${APOLLO_PROTO_GEN_DIR}/${stem}.pb.cc")
    set(h "${APOLLO_PROTO_GEN_DIR}/${stem}.pb.h")
    set(py "${APOLLO_PROTO_PY_DIR}/${stem}_pb2.py")
    set(depends "${APOLLO_SOURCE_ROOT}/${f}")
    foreach(i IN LISTS imports)
      if(EXISTS "${APOLLO_SOURCE_ROOT}/${i}")
        list(APPEND depends "${APOLLO_SOURCE_ROOT}/${i}")
      endif()
      get_property(ilib GLOBAL PROPERTY "APOLLO_PROTO_LIB::${i}")
      if(ilib AND NOT ilib STREQUAL lib)
        list(APPEND _deps_${lib} ${ilib})
      endif()
    endforeach()
    add_custom_command(
      OUTPUT "${cc}" "${h}" "${py}"
      COMMAND ${protoc} ${import_dirs}
              "--cpp_out=${APOLLO_PROTO_GEN_DIR}"
              "--python_out=${APOLLO_PROTO_PY_DIR}"
              "${f}"
      DEPENDS ${depends} ${protoc}
      WORKING_DIRECTORY "${APOLLO_SOURCE_ROOT}"
      COMMENT "protoc ${f}"
      VERBATIM)
    list(APPEND _srcs_${lib} "${cc}" "${h}")
    list(APPEND py_outputs "${py}")
  endforeach()

  foreach(lib IN LISTS libs)
    add_library(${lib} SHARED ${_srcs_${lib}})
    set_target_properties(${lib} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      INSTALL_RPATH "$ORIGIN")
    target_include_directories(${lib} PUBLIC
      "$<BUILD_INTERFACE:${APOLLO_PROTO_GEN_DIR}>"
      "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")
    target_link_libraries(${lib} PUBLIC protobuf::libprotobuf)
    if(_deps_${lib})
      list(REMOVE_DUPLICATES _deps_${lib})
      target_link_libraries(${lib} PUBLIC ${_deps_${lib}})
    endif()
    install(TARGETS ${lib} EXPORT apollo_targets LIBRARY DESTINATION lib)
  endforeach()
  # isolated libraries depend on apollo_proto through their imports; make
  # sure the dependency order is right even when they don't.
  set(APOLLO_PROTO_LIBRARIES "${libs}" PARENT_SCOPE)

  add_custom_target(apollo_proto_py ALL DEPENDS ${py_outputs})
  install(DIRECTORY "${APOLLO_PROTO_PY_DIR}/modules"
          DESTINATION "${PYTHON_INSTALL_DIR}"
          FILES_MATCHING PATTERN "*_pb2.py")
  install(DIRECTORY "${APOLLO_PROTO_GEN_DIR}/modules"
          DESTINATION "include/${PROJECT_NAME}"
          FILES_MATCHING PATTERN "*.pb.h")
  # .proto sources: import root share/apollo/proto (apollo_PROTO_IMPORT_DIR)
  foreach(f IN LISTS files)
    get_filename_component(dir "${f}" DIRECTORY)
    set_property(GLOBAL APPEND PROPERTY "APOLLO_PROTO_SRC_DIR::${dir}" "${APOLLO_SOURCE_ROOT}/${f}")
    set_property(GLOBAL APPEND PROPERTY APOLLO_PROTO_SRC_DIRS "${dir}")
  endforeach()
  get_property(dirs GLOBAL PROPERTY APOLLO_PROTO_SRC_DIRS)
  list(REMOVE_DUPLICATES dirs)
  foreach(dir IN LISTS dirs)
    get_property(srcs GLOBAL PROPERTY "APOLLO_PROTO_SRC_DIR::${dir}")
    install(FILES ${srcs} DESTINATION "share/${PROJECT_NAME}/proto/${dir}")
  endforeach()
endfunction()
