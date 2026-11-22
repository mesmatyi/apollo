planning-traffic-rules-destination
==============

## Introduction
`Destination` is a traffic rules plugin in planning, used to handle the decision-making for the planning destination. The plugin is loaded by TrafficDecider and, through the ApplyRule interface method, generates a stopping decision wall at the planning destination according to the end position of the global routing; if the destination is the end point of a pull over scenario, only a pull over stopping decision wall is generated.

### MakeDecisions
The function decides the vehicle's destination stopping decision by evaluating the input pull_over end position and the global routing end position. First, the S distance is computed by projecting the global routing end position into the SL coordinate system. If the routing end S distance plus the distance from the center of the vehicle's rear axle to the front edge of the vehicle is greater than the length of the reference line, a warning is output that the destination is not on the reference line; a stopping decision based on the routing end point is nevertheless still generated: the stopping decision before the destination is generated from the routing end position minus the destination safety distance (configuration) and the virtual wall distance (gflag configuration). If the destination is the end point of a pull over scenario, a pull over stopping decision wall is generated. If the current vehicle start S distance is greater than the S distance of the routing end point, that is, the reference line is still far from the destination, no stopping decision wall based on the routing end point is generated.

## Directory Structure
```shell
modules/planning/traffic_rules/destination/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── destination.cc
├── destination.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── destination.proto
└── README.md
```

## Module Input, Output, and Configuration

### planning-traffic-rules-destination plugin

#### Input
| Channel name | Type | Description |
| --- | --- | --- |
| frame->local_view().end_lane_way_point | Frame | End point of the global routing path |
| frame->is_near_destination() | Frame | Check of whether the destination is being approached |
| reference_line_info->reference_line() | ReferenceLineInfo | Reference line |
| reference_line.Length() | ReferenceLineInfo | Reference line length |
| reference_line_info->AdcSlBoundary() | ReferenceLineInfo | Boundary of the ego vehicle |

#### Output
| Channel name | Type | Description |
| --- | --- | --- |
| reference_line_info->path_decision() | ReferenceLineInfo | Path decision |

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/traffic_rules/destination/conf/default_conf.pb.txt` | apollo::planning::DestinationConfig | Configuration file of the Destination plugin |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage

##### Configuring the Destination plugin to load
In the file `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`, in the configuration parameters, `name` is the name of the traffic rule, which is user-defined, and `type` is the class name of Destination, that is, `Destination`.
```
task {
  name: "DESTINATION"
  type: "Destination"
}
```

##### Configuring Destination parameters
In `modules/planning/traffic_rules/destination/conf/default_conf.pb.txt`, configure the parameters of the `Destination` plugin.

##### Starting with mainboard
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
