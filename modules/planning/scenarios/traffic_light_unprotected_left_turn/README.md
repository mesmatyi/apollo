planning-scenario-traffic-light-unprotected-left-turn
============

## Introduction

`TrafficLightUnprotectedLeftTurnScenario` is the traffic light left-turn scenario without right-of-way protection. In this scenario, the ego vehicle is on the left-turn lane line

## Directory Structure

```shell

modules/planning/scenarios/traffic_light_unprotected_left_turn/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   └── traffic_light_unprotected_left_turn_intersection_cruise
├── context.h
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── traffic_light_unprotected_left_turn.proto
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_creep.cc
├── stage_creep.h
├── stage_creep_test.cc
├── stage_intersection_cruise.cc
├── stage_intersection_cruise.h
├── traffic_light_unprotected_left_turn_scenario.cc
├── traffic_light_unprotected_left_turn_scenario.h
└── traffic_light_unprotected_left_turn_scenario_test.cc

```

## Module

### TrafficLightUnprotectedLeftTurnScenario plugin

apollo::planning::TrafficLightUnprotectedLeftTurnScenario

#### Scenario entry conditions
  1. The first overlap on the reference line ahead of the vehicle is a traffic-light-type overlap
  2. The distance to the stop sign overlap is less than start_traffic_light_scenario_distance
  3. The traffic light of the overlap ahead shows red
  4. The lane type of the reference line overlap is a left-turn lane
  
#### Stages

| Stage name                                                    | Type                                                                       | <div style="width: 300pt">Description</div>                   |
| --------------------------------------------------------- | -------------------------------------------------------------------------- | ------------------------ |
| `TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_APPROACH`            | `apollo::planning::TrafficLightUnprotectedLeftTurnStageApproach`           | Stage of stopping before the traffic light stop line |
| `TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_CREEP`               | `apollo::planning::TrafficLightUnprotectedLeftTurnStageCreep`              | Creep after the green light to observe vehicles coming at the intersection   |
| `TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_INTERSECTION_CRUISE` | `apollo::planning::TrafficLightUnprotectedLeftTurnStageIntersectionCruise` | Stage of passing quickly through the traffic light intersection   |




#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/traffic_light_unprotected_left_turn/conf/scenario_conf.pb.txt` |`apollo::planning::ScenarioTrafficLightUnprotectedLeftTurnConfig`| Scenario configuration file   |
| `modules/planning/scenarios/traffic_light_unprotected_left_turn/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline` |Scenario pipeline file |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |
