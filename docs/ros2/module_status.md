# Apollo modules on ROS 2: build and test status

Generated 2026-11-21 18:18 UTC from the colcon/CMake build of package `apollo` (`modules/`), GCC 13, CPU only (no CUDA/TensorRT/libtorch/Paddle).

How to reproduce, per module `<m>` (build dir of package apollo, configured with `-DAPOLLO_BUILD_TESTS=ON`):

```
ninja apollo_module_<m> && ninja apollo_tests_<m> && ctest -L '^<m>$' --timeout 300 --output-on-failure
```

C++ targets = libraries, components, plugins, shared modules and binaries (tests counted separately). "skipped" = an external dependency is not available (decided at configure time, transitively); "failed" = attempted but did not compile/link; "not attempted" = module not built yet.

**Totals:** 325 of 504 non-test C++ targets build (0 fail to compile, 179 skipped because a dependency is not available here: mostly CUDA/TensorRT, gRPC, proprietary SDKs). 387 of 401 tests that build pass; the 14 failures are listed per module below (environment, legacy model files or upstream test bugs).

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
| monitor | 2 | 0 | 0 | 1 | 1 | 0 | 0 |
| guardian | 1 | 0 | 0 | 0 | - | - | 0 |
| storytelling | 2 | 0 | 0 | 0 | - | - | 0 |
| task_manager | 2 | 0 | 0 | 0 | - | - | 0 |
| transform | 2 | 0 | 0 | 1 | 1 | 0 | 0 |
| external_command | 18 | 0 | 0 | 0 | - | - | 0 |
| drivers | 41 | 0 | 11 | 10 | 10 | 0 | 0 |
| audio | 3 | 0 | 0 | 2 | 1 | 1 | 0 |
| bridge | 7 | 0 | 0 | 3 | 3 | 0 | 0 |
| contrib | 2 | 0 | 0 | 0 | - | - | 0 |
| data | 2 | 0 | 0 | 0 | - | - | 0 |
| dreamview | 4 | 0 | 0 | 5 | 4 | 1 | 1 |
| dreamview_plus | 2 | 0 | 0 | 0 | - | - | 1 |
| perception | 16 | 0 | 138 | 27 | 26 | 1 | 44 |
| prediction_plugin | 0 | 0 | 4 | 0 | - | - | 0 |
| statistics | 2 | 0 | 1 | 0 | - | - | 0 |
| third_party_perception | 2 | 0 | 0 | 0 | - | - | 0 |
| tools | 4 | 0 | 0 | 0 | - | - | 0 |
| v2x | 0 | 0 | 5 | 0 | - | - | 11 |

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

Failed tests (4), all for the same reason, which is not related to the ROS 2
migration:

- `prediction__cruise_mlp_evaluator_test`, `prediction__junction_mlp_evaluator_test`,
  `prediction__junction_predictor_test`, `prediction__pedestrian_interaction_evaluator_test`:
  libtorch rejects the models in `modules/prediction/data/*.pt` with
  "Legacy model format is not supported". The files are TorchScript archives
  in the legacy format written by an old PyTorch; upstream Apollo pins
  libtorch 1.7, which can still read it, and PyTorch 2.x cannot. Re-exporting
  the models with a current PyTorch (load and `torch.jit.save` again) fixes
  this. Model paths themselves now resolve through
  `apollo::cyber::common::ResolvePath()` (`/apollo/...` -> `$APOLLO_ROOT/...`).

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

Skipped C++ targets (11), by root cause:

- 3: //third_party/camera_library/smartereye:smartereye: smartereye SDK libraries are not in the source tree
- 2: @adv_plat: proprietary adv_plat library not available
- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 1: missing library -lhesai2-driver
- 1: missing library -llivox_lidar_sdk_shared
- 1: @rs_driver: RoboSense rs_driver SDK not installed
- 1: missing library -linnoclientsdk
- 1: @vanjee_driver: Vanjee vanjee_driver SDK not installed

### audio

Failed tests:

- `audio__siren_detection_test`: Failed - environment: torch::jit::load() opens /apollo/modules/audio/data/torch_siren_detection_model.pt literally (no /apollo -> $APOLLO_ROOT remap in libtorch)

### bridge

### contrib

### data

Builds since `cyber_recorder` is part of package cyber again
(`smart_recorder` library and binary).

### dreamview

Failed tests:

- `dreamview_backend_common__websocket_handler_test`: Failed - not resolved: WebSocketTest.IntegrationTest receives no messages (server-side mg_websocket_write fails) with civetweb 1.16 (Apollo used an older civetweb); other cases pass

Skipped tests (1): 1: //cyber/blocker:blocker: not provided by package cyber (cyber/blocker/blocker_manager.h)

### dreamview_plus

The backend library and the `dreamview_plus` binary build; all
`cyber::record::Player` symbols resolve from libcyber. In this environment the
backend cannot be loaded because conda's PCL 1.8.1 needs Boost 1.66 at runtime
(see Environment notes); with a working PCL (Ubuntu's `libpcl-dev`) it loads.
The web frontend (`modules/dreamview_plus/frontend`) is a separate Node.js
build and is not part of the colcon workspace.

Skipped tests (1): `simulation_world_service_test` needs the removed
`cyber::blocker::BlockerManager`.

### perception

Skipped C++ targets (138), by root cause:

- 136: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 2: @local_config_rocm: requires CUDA/ROCm/TensorRT (no GPU support)

Failed tests:

- `perception_common_base__blob_test`: Subprocess aborted***Exception: - environment: BlobSimpleTest.TestPointersCPUGPU needs a CUDA device (aborts without GPU)

Skipped tests (44): 42: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support); 2: @local_config_rocm: requires CUDA/ROCm/TensorRT (no GPU support)

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

## Environment notes

- **Tests** are plain gtest executables registered with `add_test()`, label =
  module name. They run from the repository root with `APOLLO_ROOT=<repo>`,
  `CYBER_PATH=<cyber>/share/cyber`, `APOLLO_PLUGIN_*` pointing at the build
  tree, and `OMP_NUM_THREADS=1` (see MUMPS below). `/apollo/...` paths are
  remapped only by the cyber file utilities; code that opens such paths
  directly (sqlite, libtorch `torch::jit::load`) fails here although the files
  exist in the source tree.
- **MUMPS deadlock (affects runtime too):** conda-forge's MUMPS 5.8.2
  (`mumps-seq`, IPOPT's linear solver) hangs forever in its OpenMP L0
  factorization (`dmumps_perform_copies`) when more than one OpenMP thread is
  used. Every IPOPT user (planning smoothers, open space) hangs unless
  `OMP_NUM_THREADS=1`. The tests set it; launch files for planning should too
  (or install a MUMPS build without OpenMP).
- **libtorch:** CPU libtorch 2.7.1 from the PyPI wheel in `/opt/libtorch_cpu`
  (`-DAPOLLO_LIBTORCH_ROOT`). Its `lib/` also holds stub `libcudart.so.12` /
  `libcupti.so.12` that report "no CUDA device" (test environment only).
  `@libtorch_gpu` resolves to the CPU libtorch when CUDA is off.
- **PCL:** PCL 1.8.1 from conda-forge (the only build compatible with the
  protobuf 3.21 pin). Its headers need `boost/detail/endian.hpp` (removed in
  Boost 1.73), provided by `modules/ros2_compat/pcl`. **Its libraries are
  linked against Boost 1.66 (`libboost_*.so.1.66.0`, not installed)**: PCL
  users compile and link (conda's `--allow-shlib-undefined`), but cannot be
  loaded at runtime until a matching PCL/Boost is available.
- **ADOL-C** (planning IPOPT smoothers) and **civetweb** (dreamview, monitor,
  task_manager) are not on conda-forge; they were built from the Ubuntu
  source tarballs with `tools/ros2/install_adolc.sh` and
  `tools/ros2/install_civetweb.sh` (on Ubuntu: `libadolc-dev`,
  `libcivetweb-dev`).
- **No CUDA/TensorRT/Paddle/gRPC**: targets needing them are skipped
  automatically; C++ sources calling the CUDA API without a `USE_GPU` guard
  (dependency missing from the BUILD file) are detected by the generator.

## Cyber API gaps hit by the modules

- `apollo::cyber::record::Recorder` / `Player` (resolved): the C++
  `cyber_recorder` (record, play, info, split, recover) is part of package
  `cyber` again and works over ROS 2. A channel published by another process
  is recorded with the protobuf type carried by its messages, and the
  descriptor comes from the process's protobuf pool. `cyber_recorder` loads
  `libapollo_proto.so` for this when it is installed. This brings back
  `modules/data/tools/smart_recorder` and the dreamview_plus backend.
- `apollo::cyber::blocker::BlockerManager` (`cyber/blocker/`, removed): the
  only remaining user is `dreamview_plus` / `dreamview`
  `simulation_world_service_test`, which stays skipped.
- Headers of the new cyber no longer pull in some transitive includes the old
  ones did (`cyber/task/task.h`, `cyber/common/util.h`,
  `cyber/service_discovery/topology_manager.h`); the module sources now
  include them directly.
