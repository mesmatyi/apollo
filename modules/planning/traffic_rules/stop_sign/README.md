planning-traffic-rules-stop-sign
============

## Introduction

The `StopSign` task is used to generate a stopping traffic decision for stop signs in the map
### Module flow
The stop sign decision is mainly produced in the StopSign::MakeDecisions function. The conditions for producing a stop sign decision include:
- A stop sign exists on the reference line ahead of the ego vehicle
- The stop sign is not marked as done in injector_
## Directory Structure

```shell

modules/planning/traffic_rules/stop_sign/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── stop_sign.proto
├── README.md
├── stop_sign.cc
└── stop_sign.h

```

## Module

### StopSign plugin

apollo::planning::StopSign

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/traffic_rules/stop_sign/conf/default_conf.pb.txt` | apollo::planning::StopSignConfig | Default configuration file of StopSign |

#### Module parameters

The algorithm parameter configuration is defined in modules/planning/tasks/stop_sign/proto/stop_sign.proto

| enabled | Whether enabled     |
| ------------------------------- | ---------------------- |
| stop_distance  | Distance for stopping before the stop line         |

#### Usage

##### Configuring the StopSign plugin to load

Add the configuration of the `StopSign` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`. In the configuration parameters, `name` is the name of the rule, which is user-defined and only needs to make clear which rule it is, and `type` is the class name of the rule, that is, `StopSign`.

```
  rule {
      name: "STOP_SIGN"
      type: "StopSign"
  }
  ```
