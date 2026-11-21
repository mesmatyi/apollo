planning-task-speed-bounds-decider
============

## Introduction

The `SpeedBoundsDecider` task is used to produce the drivable ST interval for speed planning
### Module flow

1. Project the predicted obstacle trajectories into the ST space and build ST boundaries according to the decisions; if there is no decision, keep the upper and lower boundaries of the obstacle.
  ```c++
    if (boundary_mapper.ComputeSTBoundary(path_decision).code() ==
      ErrorCode::PLANNING_ERROR) {
    const std::string msg = "Mapping obstacle failed.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }
  ```
  2. Produce the speed limit curve from the road speed limit and the obstacle nudge speed limit
   ```c++
   if (!speed_limit_decider
           .GetSpeedLimits(path_decision->obstacles(), &speed_limit)
           .ok()) {
    const std::string msg = "Getting speed limits failed!";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }
   ``` 

## Directory Structure

```shell

modules/planning/tasks/speed_bounds_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── speed_bounds_decider.proto
├── README.md
├── speed_bounds_decider.cc
├── speed_bounds_decider.h
├── speed_limit_decider.cc
├── speed_limit_decider.h
├── st_boundary_mapper.cc
├── st_boundary_mapper.h
└── st_boundary_mapper_test.cc

```

## Module

### SpeedBoundsDecider plugin

apollo::planning::SpeedBoundsDecider

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/speed_bounds_decider/conf/default_conf.pb.txt` | apollo::planning::SpeedBoundsDeciderConfig | Default configuration file of SpeedBoundsDecider |
|`modules/common/data/vehicle_param.pb.txt`|`apollo::common::VehicleConfig`|Vehicle chassis configuration file|

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Module parameters
  
   The algorithm parameter configuration is defined in modules/planning/tasks/rule_based_stop_decider/proto/rule_based_stop_decider.proto
   
   | total_time                          | Total duration of the ST boundary                          |
   | ----------------------------------- | ------------------------------------- |
   | max_centric_acceleration_limit      | Maximum centripetal acceleration                        |
   | minimal_kappa                       | Minimum curvature                              |
   | point_extension                     | Extension length of the ST projection in the S direction                   |
   | lowest_speed                        | Lowest speed limit                              |
   | collision_safety_range              | Nudge safety distance; speed is reduced within the safety distance |
   | static_obs_nudge_speed_ratio        | Speed limit ratio for nudging static obstacles               |
   | dynamic_obs_nudge_speed_ratio       | Speed limit ratio for nudging dynamic obstacles              |
   | enable_nudge_slowdown               | Whether to slow down when nudging                       |
   | lane_change_obstacle_nudge_l_buffer | Nudge buffer during lane changes                       |
   | max_trajectory_len                  | Maximum trajectory length                          |

#### Command-line parameters / gflags

The command-line parameters used are defined in `modules/planning/planning_base/common/planning_gflags.cc`, and their values are defined in planning.conf.
| flag | Description |
  |  ---- | ---- |
|  FLAGS_trajectory_check_collision_time_step | Collision check time step |

#### Usage

##### Configuring the SpeedBoundsDecider Task plugin to load

    In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `SpeedBoundsDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `SpeedBoundsDecider`.
    ```
    task {
      name: "SPEED_BOUNDS_PRIORI_DECIDER"
      type: "SpeedBoundsDecider"
    }
      ```
      