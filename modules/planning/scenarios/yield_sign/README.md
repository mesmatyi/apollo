planning-scenario-yield-sign
============

## Introduction

The `YieldSignScenario` scenario lets the vehicle slow down and observe in scenarios with a yield sign, and then pass slowly.

## Directory Structure

```shell

modules/planning/scenarios/yield_sign/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   └── scenario_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── yield_sign.proto
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_creep.cc
├── stage_creep.h
├── stage_creep_test.cc
├── yield_sign_scenario.cc
├── yield_sign_scenario.h
└── yield_sign_scenario_test.cc

```

## Module

### YieldSignScenario plugin

apollo::planning::YieldSignScenario

#### Scenario entry conditions

  1. The first overlap on the reference line ahead of the vehicle is a stop-sign-type overlap
  2. The distance to the stop sign overlap is less than start_stop_sign_scenario_distance
  
#### Stages

| Stage name                | Type                                       | Description                           |
| --------------------- | ------------------------------------------ | ------------------------------ |
| `YIELD_SIGN_APPROACH` | `apollo::planning::YieldSignStageApproach` | Stops before the yield sign to yield to moving obstacles |
| `YIELD_SIGN_CREEP`    | `apollo::planning::YieldSignStageCreep`    | Creeps through the yield zone                 |


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/yield_sign/conf/scenario_conf.pb.txt` |`apollo::planning::ScenarioYieldSignConfig` |Scenario configuration file   |
| `modules/planning/scenarios/yield_sign/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline`|Scenario pipeline file |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

