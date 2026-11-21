# Apollo modules on ROS 2: build and test status

Generated 2026-11-21 17:29 UTC from the colcon/CMake build of package `apollo` (`modules/`), GCC 13, CPU only (no CUDA/TensorRT/libtorch/Paddle).

How to reproduce, per module `<m>` (build dir of package apollo, configured with `-DAPOLLO_BUILD_TESTS=ON`):

```
ninja apollo_module_<m> && ninja apollo_tests_<m> && ctest -L '^<m>$' --timeout 300 --output-on-failure
```

C++ targets = libraries, components, plugins, shared modules and binaries (tests counted separately). "skipped" = an external dependency is not available (decided at configure time, transitively); "failed" = attempted but did not compile/link; "not attempted" = module not built yet.

## Summary

| module | C++ built | C++ failed | C++ skipped | tests built | tests passed | tests failed | tests skipped |
|---|---|---|---|---|---|---|---|
| common | 18 | 0 | 0 | 35 | 32 | 2 | 0 |
| canbus | 4 | 0 | 0 | 1 | 1 | 0 | 0 |
| canbus_vehicle | 22 | 0 | 0 | 168 | 168 | 0 | 0 |
| control | 48 | 0 | 0 | 16 | 13 | 1 | 0 |
| map | 9 | 0 | 5 | 4 | 3 | 0 | 0 |
| routing | 6 | 0 | 0 | 5 | 5 | 0 | 0 |
| planning | 0 | 84 | 4 | not attempted | - | - | 2 |
| prediction | 1 | n/a | 16 | 0 | - | - | 36 |
| localization | 2 | not attempted (1) | 13 | not attempted | - | - | 24 |
| monitor | 0 | n/a | 2 | 0 | - | - | 1 |
| guardian | 1 | n/a | 0 | 0 | - | - | 0 |
| storytelling | 2 | n/a | 0 | 0 | - | - | 0 |
| task_manager | 0 | n/a | 2 | 0 | - | - | 0 |
| transform | 2 | 0 | 0 | 1 | 1 | 0 | 0 |
| external_command | 8 | not attempted (10) | 0 | 0 | - | - | 0 |
| drivers | 37 | not attempted (6) | 9 | not attempted | - | - | 0 |
| audio | 0 | n/a | 3 | 0 | - | - | 2 |
| bridge | 1 | not attempted (6) | 0 | not attempted | - | - | 0 |
| contrib | 2 | n/a | 0 | 0 | - | - | 0 |
| data | 0 | n/a | 2 | 0 | - | - | 0 |
| dreamview | 0 | n/a | 4 | 0 | - | - | 6 |
| dreamview_plus | 0 | n/a | 2 | 0 | - | - | 1 |
| perception | 11 | not attempted (5) | 138 | not attempted | - | - | 44 |
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

### canbus

### canbus_vehicle

### control

Failed tests:

- `control_control_component_controller_task_base_common__interpolation_2d_test`: Failed - upstream: the test's 3-point table is all-positive and is rejected by Interpolation2D::CheckMap() ("calibration has all pos map"); independent of the port

### map

Skipped C++ targets (5), by root cause:

- 5: @com_github_grpc_grpc//:grpc++: CMake target(s) gRPC::grpc++ not found

### routing

### planning

Failed to build:

- `//modules/planning/park_data_center:apollo_park_data_center` (cc_library)
- `//modules/planning/planners/lattice:lattice_planner_base` (cc_library)
- `//modules/planning/planners/lattice:liblattice_planner.so` (plugin)
- `//modules/planning/planners/navi:libnavi_planner.so` (plugin)
- `//modules/planning/planners/navi:navi_planner_base` (cc_library)
- `//modules/planning/planners/public_road:libpublic_road_planner.so` (plugin)
- `//modules/planning/planners/rtk:librtk_replay_planner.so` (plugin)
- `//modules/planning/planning_base:apollo_planning_planning_base` (cc_library)
- `//modules/planning/planning_base:evaluate_trajectory` (cc_binary)
- `//modules/planning/planning_base:planning_pad_terminal` (cc_binary)
- `//modules/planning/planning_base:record_to_learning_data` (cc_binary)
- `//modules/planning/planning_base:smoother_util` (cc_binary)
- `//modules/planning/planning_base:spiral_smoother_util` (cc_binary)
- `//modules/planning/planning_component:libplanning_component.so` (component)
- `//modules/planning/planning_interface_base:apollo_planning_planning_interface_base` (cc_library)
- `//modules/planning/planning_open_space:apollo_planning_open_space` (cc_library)
- `//modules/planning/planning_open_space:distance_approach_problem_wrapper_lib.so` (cc_shared)
- `//modules/planning/planning_open_space:hybrid_a_star_wrapper_lib.so` (cc_shared)
- `//modules/planning/planning_open_space:open_space_roi_wrapper_lib.so` (cc_shared)
- `//modules/planning/pnc_map/lane_follow_map:liblane_follow_map.so` (plugin)
- `//modules/planning/scenarios/bare_intersection_unprotected:libbare_intersection_unprotected_scenario.so` (plugin)
- `//modules/planning/scenarios/emergency_pull_over:libemergency_pull_over_scenario.so` (plugin)
- `//modules/planning/scenarios/emergency_stop:libemergency_stop_scenario.so` (plugin)
- `//modules/planning/scenarios/free_space:libfree_space_scenario.so` (plugin)
- `//modules/planning/scenarios/lane_escape:liblane_escape_scenario.so` (plugin)
- `//modules/planning/scenarios/lane_follow:liblane_follow_scenario.so` (plugin)
- `//modules/planning/scenarios/lane_follow_park:liblane_follow_park_scenario.so` (plugin)
- `//modules/planning/scenarios/large_curvature:liblarge_curvature_scenario.so` (plugin)
- `//modules/planning/scenarios/park_and_go:libpark_and_go_scenario.so` (plugin)
- `//modules/planning/scenarios/precise_parking:libprecise_parking_scenario.so` (plugin)
- `//modules/planning/scenarios/pull_over:libpull_over_scenario.so` (plugin)
- `//modules/planning/scenarios/square:libsquare_scenario.so` (plugin)
- `//modules/planning/scenarios/stop_sign_unprotected:libstop_sign_unprotected_scenario.so` (plugin)
- `//modules/planning/scenarios/traffic_light_protected:libtraffic_light_protected_scenario.so` (plugin)
- `//modules/planning/scenarios/traffic_light_unprotected_left_turn:libtraffic_light_unprotected_left_turn_scenario.so` (plugin)
- `//modules/planning/scenarios/traffic_light_unprotected_right_turn:libtraffic_light_unprotected_right_turn_scenario.so` (plugin)
- `//modules/planning/scenarios/valet_parking:libvalet_parking_scenario.so` (plugin)
- `//modules/planning/scenarios/valet_parking_park:libvalet_parking_scenario_park.so` (plugin)
- `//modules/planning/scenarios/yield_sign:libyield_sign_scenario.so` (plugin)
- `//modules/planning/tasks/fallback_path:libfallback_path.so` (plugin)
- `//modules/planning/tasks/fast_stop_trajectory_fallback:libfast_stop_trajectory_fallback.so` (plugin)
- `//modules/planning/tasks/lane_borrow_path:liblane_borrow_path.so` (plugin)
- `//modules/planning/tasks/lane_borrow_path_generic:liblane_borrow_path_generic.so` (plugin)
- `//modules/planning/tasks/lane_change_path:liblane_change_path.so` (plugin)
- `//modules/planning/tasks/lane_change_path_generic:liblane_change_path_generic.so` (plugin)
- `//modules/planning/tasks/lane_follow_path:liblane_follow_path.so` (plugin)
- `//modules/planning/tasks/obstacle_nudge_decider:libobstacle_nudge_decider.so` (plugin)
- `//modules/planning/tasks/open_space_fallback_decider:libopen_space_fallback_decider.so` (plugin)
- `//modules/planning/tasks/open_space_fallback_decider_park:libopen_space_fallback_decider_park.so` (plugin)
- `//modules/planning/tasks/open_space_path_planning:libopen_space_path_planning.so` (plugin)
- `//modules/planning/tasks/open_space_pre_stop_decider:libopen_space_pre_stop_decider.so` (plugin)
- `//modules/planning/tasks/open_space_replan_decider:libopen_space_replan_decider.so` (plugin)
- `//modules/planning/tasks/open_space_roi_decider:libopen_space_roi_decider.so` (plugin)
- `//modules/planning/tasks/open_space_roi_decider_park:libopen_space_roi_decider_park.so` (plugin)
- `//modules/planning/tasks/open_space_trajectory_optimizer_park:libopen_space_trajectory_optimizer_park.so` (plugin)
- `//modules/planning/tasks/open_space_trajectory_partition:libopen_space_trajectory_partition.so` (plugin)
- `//modules/planning/tasks/open_space_trajectory_post_process:libopen_space_trajectory_post_process.so` (plugin)
- `//modules/planning/tasks/open_space_trajectory_provider:libopen_space_trajectory_provider.so` (plugin)
- `//modules/planning/tasks/path_decider:libpath_decider.so` (plugin)
- `//modules/planning/tasks/path_reference_decider:libpath_reference_decider.so` (plugin)
- `//modules/planning/tasks/path_time_heuristic:libpath_time_heuristic_optimizer.so` (plugin)
- `//modules/planning/tasks/piecewise_jerk_speed:libpiecewise_jerk_speed_optimizer.so` (plugin)
- `//modules/planning/tasks/piecewise_jerk_speed_nonlinear:libpiecewise_jerk_speed_nonlinear_optimizer.so` (plugin)
- `//modules/planning/tasks/pull_over_path:libpull_over_path.so` (plugin)
- `//modules/planning/tasks/reuse_path:libreuse_path.so` (plugin)
- `//modules/planning/tasks/reverse_path:libreverse_path.so` (plugin)
- `//modules/planning/tasks/reverse_speed:libreverse_speed.so` (plugin)
- `//modules/planning/tasks/rule_based_stop_decider:librule_based_stop_decider.so` (plugin)
- `//modules/planning/tasks/smooth_stop_trajectory_fallback:libsmooth_stop_trajectory_fallback.so` (plugin)
- `//modules/planning/tasks/speed_bounds_decider:libspeed_bounds_decider.so` (plugin)
- `//modules/planning/tasks/speed_bounds_decider:st_boundary_mapper` (cc_library)
- `//modules/planning/tasks/speed_decider:libspeed_decider.so` (plugin)
- `//modules/planning/tasks/square_path:libsquare_path.so` (plugin)
- `//modules/planning/tasks/st_bounds_decider:libst_bounds_decider.so` (plugin)
- `//modules/planning/traffic_rules/backside_vehicle:libbackside_vehicle.so` (plugin)
- `//modules/planning/traffic_rules/crosswalk:libcrosswalk.so` (plugin)
- `//modules/planning/traffic_rules/destination:libdestination.so` (plugin)
- `//modules/planning/traffic_rules/keepclear:libkeepclear.so` (plugin)
- `//modules/planning/traffic_rules/reference_line_end:libreference_line_end.so` (plugin)
- `//modules/planning/traffic_rules/rerouting:librerouting.so` (plugin)
- `//modules/planning/traffic_rules/speed_setting:libspeed_setting.so` (plugin)
- `//modules/planning/traffic_rules/stop_sign:libstop_sign.so` (plugin)
- `//modules/planning/traffic_rules/traffic_light:libtraffic_light.so` (plugin)
- `//modules/planning/traffic_rules/yield_sign:libyield_sign.so` (plugin)

Skipped C++ targets (4), by root cause:

- 2: @libtorch_cpu: requires libtorch
- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 1: @ad_rss_lib: ad_rss_lib not available

Skipped tests (2): 1: @libtorch_cpu: requires libtorch; 1: @ad_rss_lib: ad_rss_lib not available

### prediction

Skipped C++ targets (16), by root cause:

- 16: @npp: requires CUDA/ROCm/TensorRT (no GPU support)

Skipped tests (36): 36: @npp: requires CUDA/ROCm/TensorRT (no GPU support)

### localization

Skipped C++ targets (13), by root cause:

- 13: @local_config_pcl: PCL not found

Skipped tests (24): 24: @local_config_pcl: PCL not found

### monitor

Skipped C++ targets (2), by root cause:

- 2: @civetweb: civetweb not available

Skipped tests (1): 1: @civetweb: civetweb not available

### guardian

### storytelling

### task_manager

Skipped C++ targets (2), by root cause:

- 2: @civetweb: civetweb not available

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

Skipped C++ targets (3), by root cause:

- 3: @libtorch_gpu: requires libtorch

Skipped tests (2): 2: @libtorch_gpu: requires libtorch

### bridge

### contrib

### data

Skipped C++ targets (2), by root cause:

- 2: @civetweb: civetweb not available

### dreamview

Skipped C++ targets (4), by root cause:

- 4: @civetweb: civetweb not available

Skipped tests (6): 6: @civetweb: civetweb not available

### dreamview_plus

Skipped C++ targets (2), by root cause:

- 2: @civetweb: civetweb not available

Skipped tests (1): 1: @civetweb: civetweb not available

### perception

Skipped C++ targets (138), by root cause:

- 101: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)
- 19: @local_config_pcl: PCL not found
- 13: @libtorch_gpu: requires libtorch
- 5: @local_config_rocm: requires CUDA/ROCm/TensorRT (no GPU support)

Skipped tests (44): 33: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support); 7: @libtorch_gpu: requires libtorch; 2: @local_config_rocm: requires CUDA/ROCm/TensorRT (no GPU support); 2: @local_config_pcl: PCL not found

### prediction_plugin

Skipped C++ targets (4), by root cause:

- 4: @npp: requires CUDA/ROCm/TensorRT (no GPU support)

### statistics

Skipped C++ targets (1), by root cause:

- 1: @local_config_cuda: requires CUDA/ROCm/TensorRT (no GPU support)

### third_party_perception

### tools

### v2x

Skipped C++ targets (5), by root cause:

- 5: @com_github_grpc_grpc//:grpc++: CMake target(s) gRPC::grpc++ not found

Skipped tests (11): 11: @com_github_grpc_grpc//:grpc++: CMake target(s) gRPC::grpc++ not found

