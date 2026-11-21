planning-task-piecewise-jerk-speed
============

## Introduction

The `PiecewiseJerkSpeedOptimizer` task is a speed planning algorithm in the convex ST space based on quadratic programming. The optimized speed planning result is saved in st_graph_data_ of reference_line_info_.

### Module flow
1. Check whether the destination has been reached; if so, speed planning is no longer performed
  ```c++
   if (reference_line_info_->ReachedDestination()) {
    return Status::OK();
  }
  ```
  2. Build the speed planning problem
   ```c++
   PiecewiseJerkSpeedProblem piecewise_jerk_problem(num_of_knots, delta_t,  init_s);
   ```
  3. Build the constraints on s according to the obstacle decisions and the ST occupied space
   ```c++
     for (int i = 0; i < num_of_knots; ++i) {
    double curr_t = i * delta_t;
    double s_lower_bound = 0.0;
    double s_upper_bound = total_length;
    for (const STBoundary* boundary : st_graph_data.st_boundaries()) {
      double s_lower = 0.0;
      double s_upper = 0.0;
      if (!boundary->GetUnblockSRange(curr_t, &s_upper, &s_lower)) {
        continue;
      }
      switch (boundary->boundary_type()) {
        case STBoundary::BoundaryType::STOP:
        case STBoundary::BoundaryType::YIELD:
          s_upper_bound = std::fmin(s_upper_bound, s_upper);
          break;
        case STBoundary::BoundaryType::FOLLOW:
          // TODO(Hongyi): unify follow buffer on decision side
          s_upper_bound = std::fmin(s_upper_bound,
                                    s_upper - config_.follow_distance_buffer());
          break;
        case STBoundary::BoundaryType::OVERTAKE:
          s_lower_bound = std::fmax(s_lower_bound, s_lower);
          break;
        default:
          break;
      }
    }
    print_debug.AddPoint("st_bounds_lower", curr_t, s_lower_bound);
    print_debug.AddPoint("st_bounds_upper", curr_t, s_upper_bound);
    if (s_lower_bound > s_upper_bound) {
      const std::string msg =
          "s_lower_bound larger than s_upper_bound on STGraph";
      AERROR << msg;
      speed_data->clear();
      print_debug.PrintToLog();
      return Status(ErrorCode::PLANNING_ERROR, msg);
    }

    s_bounds.emplace_back(s_lower_bound, s_upper_bound);
  }
   ```
  4.  Optimize to obtain the optimal solution
   `piecewise_jerk_problem.Optimize()`
   5. Fill in speed points to satisfy the trajectory time length
   `SpeedProfileGenerator::FillEnoughSpeedPoints(speed_data);`

## Directory Structure

```shell

modules/planning/tasks/piecewise_jerk_speed/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── piecewise_jerk_speed_optimizer.cc
├── piecewise_jerk_speed_optimizer.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── piecewise_jerk_speed.proto
└── README.md

```

## Module

### PiecewiseJerkSpeedOptimizer plugin

apollo::planning::PiecewiseJerkSpeedOptimizer

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/piecewise_jerk_speed/conf/default_conf.pb.txt` | apollo::planning::SpeedHeuristicOptimizerConfig | Default configuration file of PathTimeHeuristicOptimizer |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Module parameters
  
   The algorithm parameter configuration is defined in modules/planning/tasks/piecewise_jerk_speed/proto/piecewise_jerk_speed.proto
   
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

#### Usage

##### Configuring the PiecewiseJerkSpeedOptimizer Task plugin to load

In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `PiecewiseJerkSpeedOptimizer` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `PiecewiseJerkSpeedOptimizer`.
```
task {
  name: "PIECEWISE_JERK_SPEED"
  type: "PiecewiseJerkSpeedOptimizer"
}
  ```
