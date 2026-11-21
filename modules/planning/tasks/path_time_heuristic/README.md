planning-task-path-time-heuristic
============

## Introduction

The `PathTimeHeuristicOptimizer` task performs coarse speed planning in the non-convex ST space based on a dynamic programming algorithm. The processed data is saved in st_graph_data_ of reference_line_info_. The algorithm flow is:

1. Build the ST grid graph
  ```c++
  GriddedPathTimeGraph st_graph(
      reference_line_info_->st_graph_data(), dp_st_speed_optimizer_config,
      reference_line_info_->path_decision()->obstacles().Items(), init_point_);
  ```
  2. Search the grid graph
   ```c++
   if (!st_graph.Search(speed_data).ok()) {
    AERROR << "failed to search graph with dynamic programming.";
    return false;
  }
   ```
    - Initialize the cost table. The coordinates of each grid point are its coordinates in the ST graph, and the cost of every grid point is set to infinity.
    `InitCostTable()`
    - Initialize the speed limit table: initialize the speed limit at each index position in the s direction. This speed limit comes from the speed limits on the reference line, which may be lane speed limits on the map or other speed limits at fixed positions.
    `InitSpeedLimitLookUp()`
    - Compute the cost of each grid point recursively
    `CalculateTotalCost()`
    - Backtrack from the end point to obtain the complete ST curve
    `RetrieveSpeedProfile()`
  
## Directory Structure

```shell

modules/planning/tasks/path_time_heuristic/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── dp_st_cost.cc
├── dp_st_cost.h
├── gridded_path_time_graph.cc
├── gridded_path_time_graph.h
├── gridded_path_time_graph_test.cc
├── path_time_heuristic_optimizer.cc
├── path_time_heuristic_optimizer.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── path_time_heuristic.proto
├── README.md
├── st_graph_point.cc
└── st_graph_point.h

```

## Module

### PathTimeHeuristicOptimizer plugin

apollo::planning::PathTimeHeuristicOptimizer

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/path_time_heuristic/conf/default_conf.pb.txt` | apollo::planning::SpeedHeuristicOptimizerConfig | Default configuration file of PathTimeHeuristicOptimizer |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |
|`modules/common/data/vehicle_param.pb.txt`|`apollo::common::VehicleConfig`|Vehicle chassis configuration file|

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Module parameters

The module parameter configuration is defined in modules/planning/tasks/path_time_heuristic/proto/path_time_heuristic.proto
   
  | unit_t | Sampling time interval     |
  | ------------------------------- | ---------------------- |
  | dense_dimension_s  | Number of densely sampled displacements    |
  | dense_unit_s  | Dense sampling displacement interval    |
  | sparse_unit_s  | Sparse sampling displacement interval    |
  | speed_weight  | Speed weight    |
  | accel_weight  | Acceleration weight    |
  | jerk_weight  | Jerk weight    |
  | obstacle_weight  | Obstacle weight    |
  | default_obstacle_cost | Obstacle collision cost      |
  | default_speed_cost |  Speed cost     |
  | exceed_speed_penalty | Speeding penalty weight      |
  | low_speed_penalty | Low-speed penalty weight      |
  | reference_speed_penalty | Penalty for deviating from the reference speed      |
  | keep_clear_low_speed_penalty | Low-speed penalty in KeepClear areas      |
  | accel_penalty | Acceleration penalty  |
  | decel_penalty | Deceleration penalty      |
  | positive_jerk_coeff | Positive jerk penalty      |
  | negative_jerk_coeff | Negative jerk penalty      |
  | max_acceleration | Maximum acceleration      |
  | max_deceleration | Maximum deceleration      |
  | safe_distance | Safe distance to keep from the vehicle ahead      |
  | spatial_potential_penalty | Penalty for distance to the end point      |
  | enable_multi_thread_in_dp_st_graph | Whether to enable multi-threaded computation      |
  | enable_dp_reference_speed | Whether to add the cost of deviating from the cruise speed     |

#### Command-line parameters / gflags
The command-line parameters used are defined in `modules/planning/planning_base/common/planning_gflags.cc`, and their values are defined in planning.conf.
| flag | Description |
  |  ---- | ---- |
|  FLAGS_speed_lon_decision_horizon | Maximum distance of the longitudinal decision |

#### Usage
##### Configuring the PathTimeHeuristicOptimizer Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `PathTimeHeuristicOptimizer` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `PathTimeHeuristicOptimizer`.
```
task {
  name: "SPEED_HEURISTIC_OPTIMIZER"
  type: "PathTimeHeuristicOptimizer"
}
```
