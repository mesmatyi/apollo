# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
#
# CONFIG_EXTRAS of package apollo (loaded by find_package(apollo)).
# Import root containing modules/**/*.proto (use with protoc -I). Do not link
# code generated from these protos into a process that also loads Apollo
# libraries; link apollo::apollo_proto instead.
set(apollo_PROTO_IMPORT_DIR "${apollo_DIR}/../proto")
get_filename_component(apollo_PROTO_IMPORT_DIR "${apollo_PROTO_IMPORT_DIR}" ABSOLUTE)
