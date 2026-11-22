planning-traffic_rules-backside-vhicle
============

## Introduction

`BacksideVehicle` is used to produce the decision on whether to ignore vehicles approaching from behind. The decision result is saved in the path_decision of reference_line_info

#### Module flow
Traverse all obstacles and, according to their position information, determine whether to produce an ignore decision. The conditions include:
- Obstacles in front of or beside the ego vehicle cannot be ignored
- Obstacles approaching from directly behind the ego vehicle are ignored

## Directory Structure

```shell
modules/planning/traffic_rules/backside_vehicle/
├── BUILD                       // Build rules file
├── README.md                   // Documentation
├── backside_vehicle.cc         // Source file
├── backside_vehicle.h          // Header file
├── conf                        // Parameter configuration folder
│   └── default_conf.pb.txt
├── cyberfile.xml               // Package management configuration file
├── plugins.xml                 // Plugin rules file
└── proto                       // Configuration definition folder
    ├── BUILD
    └── backside_vehicle.proto
```

## Module

### BacksideVehicle plugin

apollo::planning::BacksideVehicle

#### Configuration files and configuration items

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/traffic_rules/backside_vehicle/conf/default_conf.pb.txt` | `apollo::planning::BacksideVehicleConfig` | Default configuration file of the traffic rule |

#### Usage

Add the configuration of the `BacksideVehicle` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`

```
rule {
name: "BACKSIDE_VEHICLE"
type: "BacksideVehicle"
}
```
