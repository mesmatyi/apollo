# Apollo modules on ROS 2: build and test status

Generated 2026-11-21 17:44 UTC from the colcon/CMake build of package `apollo` (`modules/`), GCC 13, CPU only (no CUDA/TensorRT/libtorch/Paddle).

How to reproduce, per module `<m>` (build dir of package apollo, configured with `-DAPOLLO_BUILD_TESTS=ON`):

```
ninja apollo_module_<m> && ninja apollo_tests_<m> && ctest -L '^<m>$' --timeout 300 --output-on-failure
```

C++ targets = libraries, components, plugins, shared modules and binaries (tests counted separately). "skipped" = an external dependency is not available (decided at configure time, transitively); "failed" = attempted but did not compile/link; "not attempted" = module not built yet.

## Summary

| module | C++ built | C++ failed | C++ skipped | tests built | tests passed | tests failed | tests skipped |
|---|---|---|---|---|---|---|---|
| common | 18 | 0 | 0 | 35 | 32 | 3 | 0 |
| canbus | 4 | 0 | 0 | 1 | 1 | 0 | 0 |
| canbus_vehicle | 22 | 0 | 0 | 168 | 168 | 0 | 0 |
| control | 48 | 0 | 0 | 16 | 13 | 3 | 0 |
| map | 9 | 0 | 5 | 4 | 3 | 1 | 0 |
| routing | 6 | 0 | 0 | 5 | 5 | 0 | 0 |
| planning | 84 | 2 | 2 | 85 | 82 | 4 | 1 |
| prediction | 1 | 16 | 0 | not attempted | - | - | 0 |
| localization | 2 | not attempted (1) | 13 | not attempted | - | - | 24 |
| monitor | 0 | not attempted (2) | 0 | not attempted | - | - | 0 |
| guardian | 1 | n/a | 0 | 0 | - | - | 0 |
| storytelling | 2 | n/a | 0 | 0 | - | - | 0 |
| task_manager | 0 | not attempted (2) | 0 | 0 | - | - | 0 |
| transform | 2 | 0 | 0 | 1 | 1 | 0 | 0 |
| external_command | 8 | not attempted (10) | 0 | 0 | - | - | 0 |
| drivers | 37 | not attempted (6) | 9 | not attempted | - | - | 0 |
| audio | 0 | not attempted (3) | 0 | not attempted | - | - | 0 |
| bridge | 1 | not attempted (6) | 0 | not attempted | - | - | 0 |
| contrib | 2 | n/a | 0 | 0 | - | - | 0 |
| data | 0 | not attempted (2) | 0 | 0 | - | - | 0 |
| dreamview | 0 | not attempted (4) | 0 | not attempted | - | - | 0 |
| dreamview_plus | 0 | not attempted (1) | 1 | not attempted | - | - | 0 |
| perception | 12 | not attempted (8) | 134 | not attempted | - | - | 43 |
| prediction_plugin | 0 | n/a | 4 | 0 | - | - | 0 |
| statistics | 2 | n/a | 1 | 0 | - | - | 0 |
| third_party_perception | 2 | n/a | 0 | 0 | - | - | 0 |
| tools | 2 | not attempted (2) | 0 | 0 | - | - | 0 |
| v2x | 0 | n/a | 5 | 0 | - | - | 11 |

## Details

### common

Failed tests:

- `common_kv_db__kv_db_test`: Failed - environment: default --kv_db_path is /apollo/data/kv_db.sqlite (sqlite opens the literal path; /apollo/data does not exist here)
- `common_math__mpc_osqp_test`: Failed
- `common_monitor_log__monitor_log_buffer_test`: Exception: SegFault - all test cases pass, then SIGSEGV at process exit inside libglog's static destructor (__tcf_0) after cyber::Init(): cyber logger / glog teardown order (reported to cyber owner)

### canbus

### canbus_vehicle

### control

Failed tests:

- `control_control_component_controller_task_base_common__interpolation_2d_test`: Failed - upstream: the test's 3-point table is all-positive and is rejected by Interpolation2D::CheckMap() ("calibration has all pos map"); independent of the port
- `control_control_component_controller_task_base_integration_tests__relative_position_test`: Exception: SegFault - environment: same as simple_control_test (hard-coded APOLLO_PLUGIN_LIB_PATH=/apollo/bazel-bin:...)
- `control_control_component_controller_task_base_integration_tests__simple_control_test`: Exception: SegFault - environment: the test sets APOLLO_PLUGIN_LIB_PATH=/apollo/bazel-bin:/opt/apollo/neo/lib (Bazel output tree), so controller plugins are not found and the null controller crashes

### map

Skipped C++ targets (5), by root cause:

- 5: @com_github_grpc_grpc//:grpc++: CMake target(s) gRPC::grpc++ not found

Failed tests:

- `map__pnc_path_test`: Exception: SegFault - upstream: TestSuite.hdmap_curvy_path builds MapPathPoints without lane waypoints; Path::InitWidth() reads lane_waypoints()[0] of an empty vector (UB -> SIGSEGV)

### routing

### planning

Failed to build:

- `//modules/planning/planning_base:inference_demo` (cc_binary)
- `//modules/planning/planning_base:planning_libtorch_inference` (cc_library)

Skipped C++ targets (2), by root cause:

- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 1: @ad_rss_lib: ad_rss_lib not available

Failed tests:

- `planning_planning_base__spiral_reference_line_smoother_test`: Timeout
- `planning_planning_open_space__dual_variable_warm_start_osqp_interface_test`: Timeout
- `planning_tasks_open_space_roi_decider__open_space_roi_decider_test`: Subprocess aborted***Exception: - environment: needs map data modules/map/data/sunnyvale_loop (not in the source tree); CHECK(hdmap_) aborts

Tests that did not compile/link:

- `//modules/planning/planning_base:model_inference_test`

Skipped tests (1): 1: @ad_rss_lib: ad_rss_lib not available

### prediction

Failed to build:

- `//modules/prediction/evaluator/model_manager/model/multi_agent_pedestrian_torch_cpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/multi_agent_pedestrian_torch_gpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/multi_agent_vehicle_torch_cpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/multi_agent_vehicle_torch_gpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/semantic_lstm_pedestrian_torch_cpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/semantic_lstm_pedestrian_torch_gpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/semantic_lstm_vehicle_torch_cpu:libmodel.so` (plugin)
- `//modules/prediction/evaluator/model_manager/model/semantic_lstm_vehicle_torch_gpu:libmodel.so` (plugin)
- `//modules/prediction:apollo_prediction` (cc_library)
- `//modules/prediction:evaluator_submodule.so` (cc_shared)
- `//modules/prediction:libprediction_component.so` (component)
- `//modules/prediction:prediction_lego.so` (cc_shared)
- `//modules/prediction:predictor_submodule.so` (cc_shared)
- `//modules/prediction:records_to_offline_data` (cc_binary)
- `//modules/prediction:vector_net_feature` (cc_binary)
- `//modules/prediction:vector_net_offline_data` (cc_binary)

### localization

Skipped C++ targets (13), by root cause:

- 13: @localization_msf: proprietary localization_msf library not available

Skipped tests (24): 24: @localization_msf: proprietary localization_msf library not available

### monitor

### guardian

### storytelling

### task_manager

### transform

### external_command

### drivers

Skipped C++ targets (9), by root cause:

- 3: //third_party/camera_library/smartereye:smartereye: smartereye SDK libraries are not in the source tree
- 2: @adv_plat: proprietary adv_plat library not available
- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 1: missing library -lhesai2-driver
- 1: missing library -llivox_lidar_sdk_shared
- 1: missing library -linnoclientsdk

### audio

### bridge

### contrib

### data

### dreamview

### dreamview_plus

Skipped C++ targets (1), by root cause:

- 1: missing library -ltcmalloc

### perception

Skipped C++ targets (134), by root cause:

- 129: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 5: @local_config_rocm: requires CUDA/ROCm/TensorRT (no GPU support)

Skipped tests (43): 41: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support); 2: @local_config_rocm: requires CUDA/ROCm/TensorRT (no GPU support)

### prediction_plugin

Skipped C++ targets (4), by root cause:

- 4: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)

### statistics

Skipped C++ targets (1), by root cause:

- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)

### third_party_perception

### tools

### v2x

Skipped C++ targets (5), by root cause:

- 5: @com_github_grpc_grpc//:grpc++: CMake target(s) gRPC::grpc++ not found

Skipped tests (11): 11: @com_github_grpc_grpc//:grpc++: CMake target(s) gRPC::grpc++ not found

