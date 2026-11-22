planning-scenario-stop-sign-unprotected
============

## Introduction

The `StopSignUnprotectedScenario` scenario lets the vehicle stop at an intersection with a stop sign marked in the HD map, observe the surrounding vehicles, creep after the surrounding vehicles have left, and then pass quickly through the intersection.

## Directory Structure

```shell

modules/planning/scenarios/stop_sign_unprotected/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   └── stop_sign_unprotected_intersection_cruise
├── context.h
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── stop_sign_unprotected.proto
├── README.md
├── stage_creep.cc
├── stage_creep.h
├── stage_creep_test.cc
├── stage_intersection_cruise.cc
├── stage_intersection_cruise.h
├── stage_pre_stop.cc
├── stage_pre_stop.h
├── stage_pre_stop_test.cc
├── stage_stop.cc
├── stage_stop.h
├── stage_stop_test.cc
├── stop_sign_unprotected_scenario.cc
├── stop_sign_unprotected_scenario.h
└── stop_sign_unprotected_scenario_test.cc

```

## Module

### StopSignUnprotectedScenario plugin

apollo::planning::StopSignUnprotectedScenario

#### Scenario entry conditions

  1. The first overlap on the reference line ahead of the vehicle is a stop-sign-type overlap
  2. The distance to the stop sign overlap is less than start_stop_sign_scenario_distance
  
#### Stages

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `STOP_SIGN_UNPROTECTED_PRE_STOP`            | `apollo::planning::StopSignUnprotectedStagePreStop`            | Stage of stopping before the stop sign                     |
| `STOP_SIGN_UNPROTECTED_STOP`                | `apollo::planning::StopSignUnprotectedStageStop`               | Stop-and-observe stage, waiting for surrounding vehicles to move away before continuing |
| `STOP_SIGN_UNPROTECTED_CREEP`               | `apollo::planning::StopSignUnprotectedStageCreep`              | Creep stage                               |
| `STOP_SIGN_UNPROTECTED_INTERSECTION_CRUISE` | `apollo::planning::StopSignUnprotectedStageIntersectionCruise` | Stage of passing quickly through the intersection                       |


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| --------------------------------------------------------------------- | ---------------- | ---------------- |
| `modules/planning/scenarios/stop_sign_unprotected/conf/scenario_conf.pb.txt` | `apollo::planning::ScenarioStopSignUnprotectedConfig` |Scenario configuration file   |
| `modules/planning/scenarios/stop_sign_unprotected/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline` |Scenario pipeline file |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

