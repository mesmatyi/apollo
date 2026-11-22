planning-task-piecewise-jerk-speed-nonlinear
============

## Introduction

The `PiecewiseJerkSpeedNonlinearOptimizer` task is a speed planning algorithm in the convex ST space based on nonlinear programming
### Module flow

1. Check whether the destination has been reached; if so, speed planning is no longer performed
  ```c++
   if (reference_line_info_->ReachedDestination()) {
    return Status::OK();
  }
  ```
  2. Build the optimization variable bounds
   ```c++
   const auto problem_setups_status =
      SetUpStatesAndBounds(path_data, *speed_data);
   ```
  3. Smooth the coarse ST curve from dynamic programming with a QP algorithm
   ```c++
     const auto qp_smooth_status =
      OptimizeByQP(speed_data, &distance, &velocity, &acceleration);
   ```
  4.  Smooth the path curvature curve with a QP algorithm
  ```c++
   const auto path_curvature_smooth_status = SmoothPathCurvature(path_data);
   ```
   5. Smooth the speed limit curve with a QP algorithm
   ```c++
   const auto speed_limit_smooth_status = SmoothSpeedLimit();
   ```
   6. Optimize the ST curve with nonlinear programming
    ```c++
      const auto nlp_smooth_status =
          OptimizeByNLP(&distance, &velocity, &acceleration);
    ```
  
  7. Fill in speed points to satisfy the trajectory duration requirement
  ```c++
  SpeedProfileGenerator::FillEnoughSpeedPoints(speed_data);
  ```
  8. Record the results for PNC Monitor visualization
  ```c++
  RecordDebugInfo(*speed_data, st_graph_data->mutable_st_graph_debug());
  ```
  
## Directory Structure

```shell

modules/planning/tasks/piecewise_jerk_speed_nonlinear/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── piecewise_jerk_speed_nonlinear_ipopt_interface.cc
├── piecewise_jerk_speed_nonlinear_ipopt_interface.h
├── piecewise_jerk_speed_nonlinear_optimizer.cc
├── piecewise_jerk_speed_nonlinear_optimizer.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── piecewise_jerk_speed_nonlinear.proto
└── README.md

```

## Module

### PiecewiseJerkSpeedNonlinearOptimizer plugin

apollo::planning::PiecewiseJerkSpeedNonlinearOptimizer

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/piecewise_jerk_speed_nonlinear/conf/default_conf.pb.txt` | apollo::planning::PiecewiseJerkNonlinearSpeedOptimizerConfig | Default configuration file of PiecewiseJerkNonlinearSpeedOptimizer |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Module parameters
  
   The algorithm parameter configuration is defined in modules/planning/tasks/piecewise_jerk_speed_nonlinear/proto/piecewise_jerk_speed_nonlinear.proto
   
   | acc_weight             | Acceleration weight   |
   | ---------------------- | ------------ |
   | jerk_weight            | Jerk weight |
   | kappa_penalty_weight   | Curvature penalty weight |
   | ref_s_weight           | Reference position weight |
   | ref_v_weight           | Reference speed weight |
   | follow_distance_buffer | Following distance buffer |

#### Command-line parameters / gflags
  The command-line parameters used are defined in `modules/planning/planning_base/common/planning_gflags.cc`, and their values are defined in planning.conf.

  | flag | Description |
  |  ---- | ---- |
  |  FLAGS_longitudinal_jerk_lower_bound | Lower bound of jerk for speed planning |
  |  FLAGS_longitudinal_jerk_upper_bound | Upper bound of jerk for speed planning |
  |  FLAGS_planning_upper_speed_limit | Maximum speed |
  |  FLAGS_follow_min_distance | Minimum following distance |
  |  FLAGS_follow_time_buffer | Minimum following time gap |

#### Usage

##### Configuring the PiecewiseJerkSpeedNonlinearOptimizer Task plugin to load

In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `PiecewiseJerkSpeedNonlinearOptimizer` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `PiecewiseJerkSpeedNonlinearOptimizer`.
```
task {
  name: "PIECEWISE_JERK_SPEED_NONLINEAR"
  type: "PiecewiseJerkSpeedNonlinearOptimizer"
}
  ```
