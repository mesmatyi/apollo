# Apollo modules on ROS 2: build and test status

Generated 2026-11-21 18:06 UTC from the colcon/CMake build of package `apollo` (`modules/`), GCC 13, CPU only (no CUDA/TensorRT/libtorch/Paddle).

How to reproduce, per module `<m>` (build dir of package apollo, configured with `-DAPOLLO_BUILD_TESTS=ON`):

```
ninja apollo_module_<m> && ninja apollo_tests_<m> && ctest -L '^<m>$' --timeout 300 --output-on-failure
```

C++ targets = libraries, components, plugins, shared modules and binaries (tests counted separately). "skipped" = an external dependency is not available (decided at configure time, transitively); "failed" = attempted but did not compile/link; "not attempted" = module not built yet.

## Summary

| module | C++ built | C++ failed | C++ skipped | tests built | tests passed | tests failed | tests skipped |
|---|---|---|---|---|---|---|---|
| common | 18 | 0 | 0 | 35 | 34 | 1 | 0 |
| canbus | 4 | 0 | 0 | 1 | 1 | 0 | 0 |
| canbus_vehicle | 22 | 0 | 0 | 168 | 168 | 0 | 0 |
| control | 48 | 0 | 0 | 16 | 13 | 3 | 0 |
| map | 9 | 0 | 5 | 4 | 3 | 1 | 0 |
| routing | 6 | 0 | 0 | 5 | 5 | 0 | 0 |
| planning | 86 | 0 | 2 | 86 | 84 | 2 | 1 |
| prediction | 17 | 0 | 0 | 36 | 32 | 4 | 0 |
| localization | 3 | 0 | 13 | 1 | 1 | 0 | 24 |
| monitor | 2 | 0 | 0 | 1 | 0 | 1 | 0 |
| guardian | 1 | 0 | 0 | 0 | - | - | 0 |
| storytelling | 2 | 0 | 0 | 0 | - | - | 0 |
| task_manager | 0 | 2 | 0 | 0 | - | - | 0 |
| transform | 2 | 0 | 0 | 1 | 1 | 0 | 0 |
| external_command | 18 | 0 | 0 | 0 | - | - | 0 |
| drivers | 41 | 2 | 9 | 10 | 10 | 0 | 0 |
| audio | 0 | not attempted (3) | 0 | not attempted | - | - | 0 |
| bridge | 1 | not attempted (6) | 0 | not attempted | - | - | 0 |
| contrib | 2 | n/a | 0 | 0 | - | - | 0 |
| data | 0 | not attempted (2) | 0 | 0 | - | - | 0 |
| dreamview | 1 | not attempted (3) | 0 | not attempted | - | - | 0 |
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

Skipped C++ targets (2), by root cause:

- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 1: @ad_rss_lib: ad_rss_lib not available

Failed tests:

- `planning_planning_base__model_inference_test`: Subprocess aborted***Exception: - environment: needs map data modules/map/data/sunnyvale_with_two_offices (not in the source tree) and a trained model; tagged "exclude" in Bazel
- `planning_tasks_open_space_roi_decider__open_space_roi_decider_test`: Subprocess aborted***Exception: - environment: needs map data modules/map/data/sunnyvale_loop (not in the source tree); CHECK(hdmap_) aborts

Skipped tests (1): 1: @ad_rss_lib: ad_rss_lib not available

### prediction

Failed tests:

- `prediction__cruise_mlp_evaluator_test`: Failed - environment: same (/apollo/modules/prediction/data/cruise_go_vehicle_model.pt opened by libtorch)
- `prediction__junction_mlp_evaluator_test`: Failed - environment: same as junction_predictor_test (/apollo/modules/prediction/data/junction_mlp_vehicle_model.pt opened by libtorch)
- `prediction__junction_predictor_test`: Failed - environment: torch::jit::load() opens /apollo/modules/prediction/data/junction_mlp_vehicle_model.pt literally (libtorch does not go through the cyber /apollo -> $APOLLO_ROOT remap; /apollo does not exist here). The model is in the source tree.
- `prediction__pedestrian_interaction_evaluator_test`: Failed - environment: same (/apollo/modules/prediction/data/*.pt opened by libtorch)

### localization

Skipped C++ targets (13), by root cause:

- 13: @localization_msf: proprietary localization_msf library not available

Skipped tests (24): 24: @localization_msf: proprietary localization_msf library not available

### monitor

Failed tests:

- `monitor__recurrent_runner_test`: Failed - was: undefined absl::flags symbol (abseil flags components not linked) - fixed in ApolloExternalDeps.cmake

### guardian

### storytelling

### task_manager

Failed to build:

- `//modules/task_manager:apollo_task_manager` (cc_library)
- `//modules/task_manager:libtask_manager_component.so` (component)

### transform

### external_command

### drivers

Failed to build:

- `//modules/drivers/lidar/rslidar:librslidar_component.so` (component)
- `//modules/drivers/lidar/vanjeelidar:libvanjeelidar_component.so` (component)

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

