planning-scenario-park-and-go
============

## Introduction

`ParkAndGoScenario` is used, when the vehicle is far from the destination and stationary, at a position that is off urban lanes or cannot be matched to a road point, to drive the vehicle from open space onto the road through freespace planning.

### Scenario entry conditions
  1. A lane-following command exists
  2. A reference line has been generated and the scenario is entered from another scenario
  3. The vehicle is stationary and far from the destination
  4. There is no road near the vehicle, or the road is not an urban lane

### Stages.

| Stage name                                                    | Type                                                                       | Description                     |
| --------------------------------------------------------- | -------------------------------------------------------------------------- | ------------------------ |
| `PARK_AND_GO_CHECK`            | `apollo::planning::ParkAndGoStageCheck`           | Checks whether the current vehicle state meets the requirements for driving on the road |
| `PARK_AND_GO_ADJUST`               | `apollo::planning::ParkAndGoStageAdjust`              |  Stage in which the vehicle drives toward the road  |
| `PARK_AND_GO_PRE_CRUISE` | `apollo::planning::ParkAndGoStagePreCruise` | Stage for adjusting the trajectory curvature   |
| `PARK_AND_GO_CRUISE` | `apollo::planning::ParkAndGoStageCruise` | Stage of approaching the reference line   |


`PARK_AND_GO_CHECK` calls the `CheckADCReadyToCruise` function to evaluate conditions such as the vehicle's gear, speed, whether obstacles ahead can be nudged, the heading difference from the road reference line, and the longitudinal distance from the reference line, and decides whether the vehicle meets the requirements for driving on the road. If the requirements are met, the next stage is `PARK_AND_GO_CRUISE`; if not, the next stage is `PARK_AND_GO_ADJUST`.

`PARK_AND_GO_ADJUST` is the stage in which the vehicle adjusts its own pose and drives toward the road. In this stage, the position obtained by moving the projection point of the ego vehicle onto the reference line by a certain distance along the longitudinal direction is used as the target point to generate the corresponding trajectory. At the same time, the `CheckADCReadyToCruise` function is called to check whether the requirements for driving on the road are met. If the requirements are met or the trajectory has been driven to completion, it is checked whether the trajectory curvature is below a threshold; if so, the next stage is `PARK_AND_GO_CRUISE`, otherwise the next stage is `PARK_AND_GO_PRE_CRUISE`.

`PARK_AND_GO_PRE_CRUISE` is the same as `PARK_AND_GO_ADJUST` and is used to adjust the ego vehicle pose so that the trajectory curvature is below a threshold. When the trajectory curvature is below the threshold, it moves on to the next stage `PARK_AND_GO_CRUISE`.

`PARK_AND_GO_CRUISE` is the public road driving stage; this stage ends when the lateral distance between the ego vehicle and the reference line is below a threshold.

## Directory Structure

```shell

modules/planning/scenarios/park_and_go/
├── BUILD
├── conf
│   ├── park_and_go_adjust
│   │   ├── open_space_roi_decider.pb.txt
│   │   └── open_space_trajectory_provider.pb.txt
│   ├── park_and_go_check
│   │   └── open_space_roi_decider.pb.txt
│   ├── park_and_go_cruise
│   │   └── path_decider.pb.txt
│   ├── park_and_go_pre_cruse
│   │   ├── open_space_roi_decider.pb.txt
│   │   └── open_space_trajectory_provider.pb.txt
│   ├── pipeline.pb.txt
│   └── scenario_conf.pb.txt
├── context.h
├── cyberfile.xml
├── park_and_go_scenario.cc
├── park_and_go_scenario.h
├── park_and_go_scenario_test.cc
├── plugins.xml
├── proto
│   ├── BUILD
│   └── park_and_go.proto
├── README.md
├── stage_adjust.cc
├── stage_adjust.h
├── stage_adjust_test.cc
├── stage_check.cc
├── stage_check.h
├── stage_check_test.cc
├── stage_cruise.cc
├── stage_cruise.h
├── stage_cruise_test.cc
├── stage_pre_cruise.cc
├── stage_pre_cruise.h
├── stage_pre_cruise_test.cc
├── util.cc
└── util.h

```

## Module

### ParkAndGoScenario plugin

apollo::planning::ParkAndGoScenario

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| --------------------------------------------------------------------- | ---------------- | ---------------- |
| `modules/planning/scenarios/park_and_go/conf/scenario_conf.pb.txt` | `apollo::planning::ScenarioParkAndGoConfig` |Scenario configuration file   |
| `modules/planning/scenarios/park_and_go/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline`| Scenario pipeline file |
| `modules/planning/scenarios/park_and_go/conf/park_and_go_adjust/open_space_roi_decider.pb.txt`      | `apollo::planning::OpenSpaceRoiDeciderConfig` |Configuration file of the `open_space_roi_decider` task in the `park_and_go_adjust` stage |
| `modules/planning/scenarios/park_and_go/conf/park_and_go_adjust/open_space_trajectory_provider.pb.txt`      |`apollo::planning::OpenSpaceTrajectoryProviderConfig` | Configuration file of the `open_space_trajectory_provider` task in the `park_and_go_adjust` stage |
| `modules/planning/scenarios/park_and_go/conf/park_and_go_check/open_space_roi_decider.pb.txt`      |`apollo::planning::OpenSpaceRoiDeciderConfig` | Configuration file of the `open_space_roi_decider` task in the `park_and_go_check` stage |
| `modules/planning/scenarios/park_and_go/conf/park_and_go_cruise/path_decider.pb.txt`      | `apollo::planning::PathDeciderConfig` |Configuration file of the `path_decider` task in the `park_and_go_cruise` stage |
| `modules/planning/scenarios/park_and_go/conf/park_and_go_pre_cruse/open_space_roi_decider.pb.txt`      |`apollo::planning::OpenSpaceRoiDeciderConfig` | Configuration file of the `open_space_roi_decider` task in the `park_and_go_pre_cruse` stage |
| `modules/planning/scenarios/park_and_go/conf/park_and_go_pre_cruse/open_space_trajectory_provider.pb.txt`      |`apollo::planning::OpenSpaceTrajectoryProviderConfig` | Configuration file of the `open_space_trajectory_provider` task in the `park_and_go_pre_cruse` stage |
