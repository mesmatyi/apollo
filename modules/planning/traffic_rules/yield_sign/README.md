planning-traffic-rules-yield-sign
============

## Introduction

The `YieldSign` task is used to generate a stopping traffic decision for traffic lights in the map
### Module flow

The traffic light decision is mainly produced in the YieldSign::MakeDecisions function. The conditions for producing a traffic light decision include:
- A yield sign exists on the reference line ahead of the ego vehicle
- The yield sign id is not marked as done in injector_
## Directory Structure

```shell

modules/planning/traffic_rules/yield_sign/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── yield_sign.proto
├── README.md
├── yield_sign.cc
└── yield_sign.h

```

## Module

### YieldSign plugin

apollo::planning::YieldSign

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/traffic_rules/yield_sign/conf/default_conf.pb.txt` | apollo::planning::YieldSignConfigConfig | Default configuration file of YieldSignConfig |

#### Module parameters

The algorithm parameter configuration is defined in modules/planning/tasks/yield_sign/proto/yield_sign.proto

| enabled | Whether enabled     |
| ------------------------------- | ---------------------- |
| stop_distance  | Distance for stopping before the stop line         |

#### Usage

##### Configuring the YieldSign plugin to load

Add the configuration of the `YieldSign` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`. In the configuration parameters, `name` is the name of the rule, which is user-defined and only needs to make clear which rule it is, and `type` is the class name of the rule, that is, `YieldSign`.
```
  rule {
      name: "YIELD_SIGN"
      type: "YieldSign"
  }
  ```
