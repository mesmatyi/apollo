planning-traffic-rules-traffic-light
============

## Introduction

The `TrafficLight` task is used to generate a stopping traffic decision for traffic lights in the map
### Module flow
The traffic light decision is mainly produced in the TrafficLight::MakeDecisions function. The conditions for producing a traffic light decision include:
- A traffic light stop line exists on the reference line ahead of the ego vehicle
- The traffic light id is not marked as done in injector_
- The color of the traffic light is not green
- The minimum deceleration needed to brake to a stop at the traffic light from the vehicle's current position is less than the configured maximum braking max_stop_deceleration
## Directory Structure

```shell

modules/planning/traffic_rules/traffic_light/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── traffic_light.proto
├── README.md
├── traffic_light.cc
└── traffic_light.h

```

## Module

### TrafficLight plugin

apollo::planning::TrafficLight

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/traffic_rules/traffic_light/conf/default_conf.pb.txt` | apollo::planning::TrafficLightConfig | Default configuration file of TrafficLight |

#### Module parameters

The algorithm parameter configuration is defined in modules/planning/tasks/traffic_light/proto/traffic_light.proto

| enabled | Whether enabled     |
| ------------------------------- | ---------------------- |
| stop_distance  | Distance for stopping before the stop line         |
|max_stop_deceleration|Maximum braking deceleration|

#### Usage

  ##### Configuring the TrafficLight plugin to load

  Add the configuration of the `TrafficLight` plugin to `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`. In the configuration parameters, `name` is the name of the rule, which is user-defined and only needs to make clear which rule it is, and `type` is the class name of the rule, that is, `TrafficLight`.

  ```
    rule {
        name: "TRAFFIC_LIGHT"
        type: "TrafficLight"
    }
    ```
