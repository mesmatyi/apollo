planning-scenario-valet-parking-park
============

## Introduction

`ValetParkingParkScenario` can park the vehicle in a specified parking spot in a parking area.

## Directory Structure

```shell

modules/planning/scenarios/valet_parking_park/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   ├── valet_parking_approaching_parking_spot_park
│   │   └── open_space_pre_stop_decider.pb.txt
│   ├── valet_parking_parking_park
│   │   ├── open_space_fallback_decider_park.pb.txt
│   │   ├── open_space_path_planning.pb.txt
│   │   ├── open_space_replan_decider.pb.txt
│   │   ├── open_space_roi_decider.pb.txt
│   │   ├── open_space_trajectory_optimizer.pb.txt
│   │   └── open_space_trajectory_post_process.pb.txt
│   └── valet_parking_retry_park
│       ├── open_space_fallback_decider_park.pb.txt
│       ├── open_space_path_planning.pb.txt
│       ├── open_space_replan_decider.pb.txt
│       ├── open_space_roi_decider.pb.txt
│       ├── open_space_trajectory_optimizer.pb.txt
│       └── open_space_trajectory_post_process.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── valet_parking_park.proto
├── README.md
├── stage_approaching_parking_spot_park.cc
├── stage_approaching_parking_spot_park.h
├── stage_parking_park.cc
├── stage_parking_park.h
├── stage_parking_retry_park.cc
├── stage_parking_retry_park.h
├── valet_parking_scenario.cc
└── valet_parking_scenario.h

```

## Module

### ValetParkingParkScenario plugin

apollo::planning::ValetParkingParkScenario

#### Scenario entry conditions

  1. A parking command exists in the planning command
  2. The distance to the parking point is within parking_spot_range_to_start
  
#### Stages

| Stage name                                        | Type                                                | <div style="width: 300pt">Description</div> |
| --------------------------------------------- | --------------------------------------------------- | ------------------------------------ |
| `VALET_PARKING_APPROACHING_PARKING_SPOT_PARK` | `apollo::planning::StageApproachingParkingSpotPark` | Guides the vehicle along the main road to the parking spot           |
| `VALET_PARKING_PARKING_PARK`                  | `apollo::planning::StageParkingPark`                | Parks into the parking spot                             |
| `VALET_PARKING_RETRY_PARK`                    | `apollo::planning::StageParkingRetryPark`           | Adjusts the pose when the accuracy of the first parking attempt is low     |


#### Configuration

| File path                                                                  | Type/Structure                                      | <div style="width: 300pt">Description</div> |
| ------------------------------------------------------------------------- | ---------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/valet_parking_park/conf/scenario_conf.pb.txt` | `apollo::planning::ScenarioValetParkingConfig` | Scenario configuration file                       |
| `modules/planning/scenarios/valet_parking_park/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline`           | Scenario pipeline file                     |

