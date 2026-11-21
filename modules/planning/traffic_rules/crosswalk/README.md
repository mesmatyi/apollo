planning-traffic_rules-crosswalk
============

## Introduction

`Crosswalk` is used to generate virtual obstacles at crosswalks, yielding to pedestrians so that they have priority

#### Module flow
1. Traverse the crosswalk information on the reference line; if the front of the ego vehicle has already passed a crosswalk, ignore that crosswalk
2. Traverse the obstacle information; if the following conditions are met, the vehicle must stop and yield
    - It is of the pedestrian or non-motorized vehicle type
    - It is within the area of the crosswalk
    - The obstacle will cross the ego vehicle's path, or is within the road range ahead of the ego vehicle
    - The deceleration required for the ego vehicle to stop is less than the maximum allowed deceleration
3. Record the obstacles that each crosswalk requires stopping and yielding for
4. At crosswalks where stopping and yielding is required, generate a virtual obstacle to stop at

## Directory Structure

```shell
modules/planning/traffic_rules/crosswalk/
├── BUILD                       // Build rules file
├── README.md                   // Documentation
├── crosswalk.cc                // Source file
├── crosswalk.h                 // Header file
├── conf                        // Parameter configuration folder
│   └── default_conf.pb.txt
├── cyberfile.xml               // Package management configuration file
├── plugins.xml                 // Plugin rules file
└── proto                       // Configuration definition folder
    ├── BUILD
    └── crosswalk.proto
```

## Module

### Crosswalk plugin

apollo::planning::Crosswalk

#### Configuration files and configuration items

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/traffic_rules/crosswalk/conf/default_conf.pb.txt` | `apollo::planning::CrosswalkConfig` |Configuration file |

| Configuration item | Description |
| ----- | ----- |
| stop_distance         | Distance between the position of the generated virtual obstacle and the crosswalk |
| max_stop_deceleration | Maximum deceleration allowed when judging stopping |
| min_pass_s_distance   | Distance for judging whether the ego vehicle has passed the crosswalk |
| expand_s_distance     | Extension distance of the crosswalk in the s direction |
| stop_strict_l_distance| When the obstacle's distance in the l direction is less than this threshold, stop if it is within the road |
| stop_loose_l_distance | When the obstacle's distance in the l direction is greater than this threshold, stop if it crosses the ego vehicle's path |
| stop_timeout          | If the obstacle has been stationary for longer than this threshold, the vehicle no longer stops to yield to it |

#### Usage

Add the configuration of the `Crosswalk` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`

```
rule {
name: "CROSSWALK"
type: "Crosswalk"
}
```
