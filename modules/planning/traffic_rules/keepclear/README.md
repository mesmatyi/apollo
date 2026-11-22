planning-traffic_rules-keep-clear
============

## Introduction

`KeepClear` is used to generate virtual static obstacles in no-stopping (keep-clear) zones, preventing the ego vehicle from entering them

#### Module flow
- After the no-stopping zone switch is turned on, traverse the no-stopping zone information on the ego vehicle's reference line. If the ego vehicle has already entered a no-stopping zone, it keeps driving; if it has not yet entered one, a virtual static obstacle is generated at the start position of the no-stopping zone
- After the junction no-stopping switch is turned on, if the ego vehicle is not in a creeping state, the junction area is treated as a no-stopping zone

## Directory Structure

```shell
modules/planning/traffic_rules/keep_clear/
├── BUILD                       // Build rules file
├── README.md                   // Documentation
├── keep_clear.cc               // Source file
├── keep_clear.h                // Header file
├── conf                        // Parameter configuration folder
│   └── default_conf.pb.txt
├── cyberfile.xml               // Package management configuration file
├── plugins.xml                 // Plugin rules file
└── proto                       // Configuration definition folder
    ├── BUILD
    └── keep_clear.proto
```

## Module

### KeepClear plugin

apollo::planning::KeepClear

#### Configuration files and configuration items

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/traffic_rules/keep_clear/conf/default_conf.pb.txt` | `apollo::planning::KeepClearConfig` |Configuration file |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |

| Configuration item | Description |
| ----- | ----- |
| enable_keep_clear_zone            | Switch enabling no-stopping zones |
| enable_junction                   | Switch enabling junction no-stopping |
| min_pass_s_distance               | Distance threshold for judging whether the ego vehicle has entered a no-stopping zone |
| align_with_traffic_sign_tolerance | Distance threshold for aligning the junction with traffic lights/crosswalks/stop signs |

#### Usage

Add the configuration of the `KeepClear` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`

```
rule {
name: "KEEP_CLEAR"
type: "KeepClear"
}
```
