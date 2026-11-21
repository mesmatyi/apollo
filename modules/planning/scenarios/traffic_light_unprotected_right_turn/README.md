planning-scenario-traffic-light-unprotected-right-turn
============

## Introduction

`TrafficLightUnprotectedRightTurnScenario` is the traffic light right-turn scenario with right-of-way protection. In this scenario, the vehicle can stop at a red light before the traffic light intersection and pass through the intersection on a green light.

## Directory Structure

```shell

modules/planning/scenarios/traffic_light_unprotected_right_turn/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   └── traffic_light_unprotected_right_turn_intersection_cruise
├── context.h
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── traffic_light_unprotected_right_turn.proto
├── README.md
├── stage_creep.cc
├── stage_creep.h
├── stage_creep_test.cc
├── stage_intersection_cruise.cc
├── stage_intersection_cruise.h
├── stage_stop.cc
├── stage_stop.h
├── stage_stop_test.cc
├── traffic_light_unprotected_right_turn_scenario.cc
├── traffic_light_unprotected_right_turn_scenario.h
└── traffic_light_unprotected_right_turn_scenario_test.cc

```

## Module

### TrafficLightUnprotectedRightTurnScenario plugin

apollo::planning::TrafficLightUnprotectedRightTurnScenario

#### Scenario entry conditions
  1. The first overlap on the reference line ahead of the vehicle is a traffic-light-type overlap
  2. The distance to the stop sign overlap is less than start_traffic_light_scenario_distance
  3. The traffic light of the overlap ahead shows red
  4. The lane type of the reference line is a right-turn lane
  
#### Stages

| Stage name                                                     | Type                                                                        | <div style="width: 300pt">Description</div>                    |
| ---------------------------------------------------------- | --------------------------------------------------------------------------- | ------------------------ |
| `TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN_STOP`                | `apollo::planning::TrafficLightUnprotectedRightTurnStageStop`               | Stage of stopping before the traffic light stop line |
| `TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN_CREEP`               | `apollo::planning::TrafficLightUnprotectedRightTurnStageCreep`              | Creep after the green light to observe vehicles coming at the intersection   |
| `TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN_INTERSECTION_CRUISE` | `apollo::planning::TrafficLightUnprotectedRightTurnStageIntersectionCruise` | Stage of passing through the traffic light intersection       |


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/traffic_light_unprotected_right_turn/conf/scenario_conf.pb.txt` |`apollo::planning::ScenarioTrafficLightUnprotectedRightTurnConfig`| Scenario configuration file   |
| `modules/planning/scenarios/traffic_light_unprotected_right_turn/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline`|Scenario pipeline file |
