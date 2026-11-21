planning-scenario-valet-parking
============

## Introduction

`ValetParkingScenario` can park the vehicle in a specified parking spot in a parking area.

## Directory Structure

```shell

modules/planning/scenarios/valet_parking/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   ├── valet_parking_approaching_parking_spot
│   │   └── open_space_pre_stop_decider.pb.txt
│   └── valet_parking_parking
│       ├── open_space_roi_decider.pb.txt
│       ├── open_space_trajectory_partition.pb.txt
│       └── open_space_trajectory_provider.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── valet_parking.proto
├── README.md
├── stage_approaching_parking_spot.cc
├── stage_approaching_parking_spot.h
├── stage_approaching_parking_spot_test.cc
├── stage_parking.cc
├── stage_parking.h
├── stage_parking_test.cc
├── valet_parking_scenario.cc
├── valet_parking_scenario.h
└── valet_parking_scenario_test.cc

```

## Module

### ValetParkingScenario plugin

apollo::planning::ValetParkingScenario

#### Scenario entry conditions

  1. A parking command exists in the planning command
  2. The distance to the parking point is within parking_spot_range_to_start
  
#### Stages

| Stage name                                   | Type                                            | <div style="width: 300pt">Description</div>                      |
| ---------------------------------------- | ----------------------------------------------- | -------------------------- |
| `VALET_PARKING_APPROACHING_PARKING_SPOT` | `apollo::planning::StageApproachingParkingSpot` | Guides the vehicle along the main road to the parking spot |
| `VALET_PARKING_PARKING`                  | `apollo::planning::StageParking`                | Parks into the parking spot                   |


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/valet_parking/conf/scenario_conf.pb.txt` |`apollo::planning::ScenarioValetParkingConfig` |Scenario configuration file   |
| `modules/planning/scenarios/valet_parking/conf/pipeline.pb.txt`      |`apollo::planning::ScenarioPipeline` |Scenario pipeline file |

