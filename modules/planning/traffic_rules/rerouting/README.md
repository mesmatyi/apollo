planning-traffic-rules-rerouting
============

## Introduction

The `Rerouting` task is used to reroute when a lane change has not succeeded
### Module flow
The rerouting decision is mainly produced in the Rerouting::ChangeLaneFailRerouting() function. The conditions for producing rerouting include:
- The current lane is not a through lane
- The vehicle is within the lane of the current reference line
- The current lane passage has no exit
- Rerouting was produced not long ago
- The ego vehicle is very close to the current passage
## Directory Structure

```shell

modules/planning/traffic_rules/rerouting
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── rerouting.proto
├── README.md
├── rerouting.cc
└── rerouting.h

```

## Module

### Rerouting plugin

apollo::planning::Rerouting

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/traffic_rules/rerouting/conf/default_conf.pb.txt` | apollo::planning::ReroutingConfig | Default configuration file of Rerouting |

#### Module parameters

The algorithm parameter configuration is defined in modules/planning/tasks/rerouting/proto/rerouting.proto

| cooldown_time | Interval between two reroutings     |
| ------------------------------- | ---------------------- |
| prepare_rerouting_time  | Time for preparing rerouting         |

#### Usage

##### Configuring the Rerouting plugin to load

Add the configuration of the `Rerouting` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`. In the configuration parameters, `name` is the name of the rule, which is user-defined and only needs to make clear which rule it is, and `type` is the class name of the rule, that is, `Rerouting`.

```
rule {
    name: "REROUTING"
    type: "Rerouting"
}
```
        
