planning-scenario-lane-follow-park
============

## Introduction

`LaneFollowParkScenario` is similar to `LaneFollowScenario` and adds, on top of it, the ability to reverse out of a stuck situation after being blocked by an obstacle.

## Directory Structure

```shell

modules/planning/scenarios/lane_follow_park/
├── BUILD
├── cyberfile.xml
├── lane_escape_park_stage.cc
├── lane_escape_park_stage.h
├── lane_follow_park_scenario.cc
├── lane_follow_park_scenario.h
├── lane_follow_park_stage.cc
├── lane_follow_park_stage.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── lane_follow_park_scenario.proto
└── README.md
```

## Module

### LaneFollowParkScenario

apollo::planning::LaneFollowParkScenario

#### Scenario entry conditions

  1. Check whether the frame's planning command contains a lane follow command. If not, the function returns false and the transfer is not possible.
  2. Check whether the frame's reference line information is empty. If it is empty, return false; the transfer is not possible.
  3. Check whether other_scenario is empty. If it is empty, return true; the transfer is possible.

#### LaneEscapeParkStage entry conditions

  1. The vehicle is stationary.
  2. A planned path exists.
  3. The distance to the destination exceeds 20 m.
  4. There is an obstacle with a stop decision within the threshold ahead; this obstacle is farther from the intersection than min_distance_block_obs_to_junction, there is enough space on the obstacle's side to bypass it, the situation is not a queuing scenario, and the obstacle stably blocks the ego vehicle's travel.

#### LaneEscapeParkStage exit conditions

  1. A non-fallback path is generated, its length is greater than forward_path_length, and the path is generated continuously and stably for more than success_path_count frames.
  2. The vehicle has been stationary for more than stop_check_window frames.
  
#### Stages

| Stage name                | Type                                    | <div style="width: 300pt">Description</div>                           |
| --------------------- | --------------------------------------- | -------------------------------------------------------------- |
| `LaneFollowParkStage` | `apollo::planning::LaneFollowParkStage` | Plans the driving trajectory toward the routing point                                    |
| `LaneEscapeParkStage` | `apollo::planning::LaneEscapeParkStage` | Executes a reversing trajectory when an obstacle blocks the way ahead, until a forward path is generated or the reversing reaches a threshold |


#### Configuration

| File path                                                                | Type/Structure                                        | <div style="width: 300pt">Description</div> |
| ----------------------------------------------------------------------- | ------------------------------------------------ | ------------------------------------ |
| `modules/planning/scenarios/lane_follow_park/conf/scenario_conf.pb.txt` | `apollo::planning::ScenarioLaneFollowParkConfig` | Scenario configuration file                       |
| `modules/planning/scenarios/lane_follow_park/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline`             | Scenario pipeline file                     |

