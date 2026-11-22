planning-task-speed-decider
============

## Introduction

The `SpeedDecider` task is used to produce longitudinal decisions on obstacles according to the planned coarse speed curve
### Module flow

1. Produce an ignore decision for obstacles that have no ST boundary or whose ST boundary lies outside the drivable ST area
```c++
    if (boundary.IsEmpty() || boundary.max_s() < 0.0 ||
        boundary.max_t() < 0.0 ||
        boundary.min_t() >= speed_profile.back().t()) {
      AppendIgnoreDecision(mutable_obstacle);
      continue;
    }
    if (obstacle->HasLongitudinalDecision()) {
      AppendIgnoreDecision(mutable_obstacle);
      continue;
    }
```
2. Produce an ignore decision for virtual obstacles that are not on the lane
```c++
    if (obstacle->IsVirtual()) {
        const auto& obstacle_box = obstacle->PerceptionBoundingBox();
        if (!reference_line_->IsOnLane(obstacle_box.center())) {
          continue;
        }
      }
```   
3. Produce a stop decision for pedestrians
```c++
    if (config_.is_stop_for_pedestrain() &&
          CheckStopForPedestrian(*mutable_obstacle)) {
        ObjectDecisionType stop_decision;
        if (CreateStopDecision(*mutable_obstacle, &stop_decision,
                              -FLAGS_min_stop_distance_obstacle)) {
          mutable_obstacle->AddLongitudinalDecision("dp_st_graph/pedestrian",
                                                    stop_decision);
        }
        continue;
      }
```   
4. Produce decisions for obstacles according to the position of the ST curve relative to the obstacle's ST boundary:
      * If the ST curve is below the obstacle and the obstacle travels in the same direction as the ego vehicle, produce a follow decision
      * If the ST curve is below the obstacle and the obstacle crosses the ego vehicle's path, produce a yield decision
      * If the ST curve is below the obstacle and the obstacle is too close to the ego vehicle, produce a stop decision
      * If the ST curve passes through the middle of the obstacle and the obstacle is a blocking obstacle, produce a stop decision
## Directory Structure

```shell

modules/planning/tasks/speed_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── speed_decider.proto
├── README.md
├── speed_decider.cc
└── speed_decider.h

```

## Module

### SpeedDecider plugin

apollo::planning::SpeedDecider

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/speed_decider/conf/default_conf.pb.txt` | apollo::planning::SpeedDeciderConfig | Default configuration file of SpeedDecider |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Module parameters
  
The algorithm parameter configuration is defined in modules/planning/tasks/speed_decider/proto/speed_decider.proto
   
   | follow_min_obs_lateral_distance | Minimum lateral distance for following     |
   | ------------------------------- | ---------------------- |
   | max_centric_acceleration_limit  | Maximum centripetal acceleration         |
   | follow_min_time_sec             | Following time gap               |
   | keep_clear_last_point_speed     | Minimum speed in KEEP_CLEAR areas |

#### Command-line parameters / gflags

The command-line parameters used are defined in `modules/planning/planning_base/common/planning_gflags.cc`, and their values are defined in planning.conf.
| flag | Description |
  |  ---- | ---- |
|  FLAGS_min_stop_distance_obstacle | Minimum braking distance for obstacles |

 #### Usage

  ##### Configuring the SpeedDecider Task plugin to load

  In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `SpeedDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `SpeedDecider`.
  
    ```
    task {
      name: "SPEED_DECIDER"
      type: "SpeedDecider"
    }
      ```
