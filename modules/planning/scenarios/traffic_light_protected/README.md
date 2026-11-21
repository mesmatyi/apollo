planning-scenario-traffic-light-protected
============

## Introduction

`TrafficLightProtectedScenario` is the traffic light scenario with right-of-way protection. In this scenario, the vehicle can stop at a red light before the traffic light intersection and pass through the intersection on a green light.

## Directory Structure

```shell

modules/planning/scenarios/traffic_light_protected/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   └── traffic_light_protected_intersection_cruise
├── context.h
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── traffic_light_protected.proto
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_intersection_cruise.cc
├── stage_intersection_cruise.h
├── traffic_light_protected_scenario.cc
├── traffic_light_protected_scenario.h
└── traffic_light_protected_scenario_test.c

```

## Module

### TrafficLightProtectedScenario plugin

apollo::planning::TrafficLightProtectedScenario

#### Scenario entry conditions
  1. The first overlap on the reference line ahead of the vehicle is a traffic-light-type overlap
  2. The distance to the stop sign overlap is less than start_traffic_light_scenario_distance
  3. The traffic light of the overlap ahead shows red
  
#### Stages

| Stage name                                        | Type                                                             | <div style="width: 300pt">Description</div> |
| --------------------------------------------- | ---------------------------------------------------------------- | ------------------------ |
| `TRAFFIC_LIGHT_PROTECTED_APPROACH`            | `apollo::planning::TrafficLightProtectedStageApproach`           | Stage of stopping before the traffic light stop line |
| `TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_CREEP`   | `apollo::planning::TrafficLightUnprotectedLeftTurnStageCreep`    | Creep after the green light to observe vehicles coming at the intersection   |
| `TRAFFIC_LIGHT_PROTECTED_INTERSECTION_CRUISE` | `apollo::planning::TrafficLightProtectedStageIntersectionCruise` | Stage of passing through the traffic light intersection       |


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/traffic_light_protected/conf/scenario_conf.pb.txt` | `apollo::planning::ScenarioTrafficLightProtectedConfig`|Scenario configuration file   |
| `modules/planning/scenarios/traffic_light_protected/conf/pipeline.pb.txt`      |`apollo::planning::ScenarioPipeline` | Scenario pipeline file |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |

