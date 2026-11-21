planning-traffic-rules-reference-line-end
==============

## Introduction
`ReferenceLineEnd` is a traffic rules plugin in planning, used to generate a stopping decision at the end of the reference_line (reference line). The plugin is loaded by TrafficDecider and, through the ApplyRule interface method, generates a virtual obstacle stop wall decision at a certain distance (FLAGS_virtual_stop_wall_length) before the end of the reference line when the ego vehicle is within the threshold distance of the reference line.

### MakeDecisions
The function generates a virtual stopping decision wall when the remaining distance from the ego vehicle to the end of the reference line is within the threshold (parameter configuration min_reference_line_remain_length). It builds a virtual obstacle from the lane IDs in the current reference line, builds a stop line within a certain threshold (configuration parameter stop_distance) before the virtual obstacle, and then generates the stopping decision of the path decision.

## Directory Structure
```shell
modules/planning/traffic_rules/reference_line_end/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── reference_line_end.cc
├── reference_line_end.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── reference_line_end.proto
└── README.md
```

## Module Input, Output, and Configuration

### planning-traffic-rules-reference-line-end plugin

#### Input
| Channel name | Type | Description |
| --- | --- | --- |
| reference_line.Length() | ReferenceLineInfo | Reference line length |
| reference_line_info->Lanes() | ReferenceLineInfo | Lane information of the reference line |
| reference_line_info->AdcSlBoundary() | ReferenceLineInfo | Boundary of the ego vehicle |

#### Output
| Channel name | Type | Description |
| --- | --- | --- |
| reference_line_info->path_decision() | ReferenceLineInfo | Path decision |

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/traffic_rules/reference_line_end/conf/default_conf.pb.txt` | apollo::planning::ReferenceLineEndConfig | Configuration file of the ReferenceLineEnd plugin |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the ReferenceLineEnd plugin to load
In the file `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`, in the configuration parameters, `name` is the name of the traffic rule, which is user-defined, and `type` is the class name of ReferenceLineEnd, that is, `ReferenceLineEnd`.
```
task {
  name: "REFERENCE_LINE_END"
  type: "ReferenceLineEnd"
}
```

##### Configuring ReferenceLineEnd parameters
In `modules/planning/traffic_rules/reference_line_end/conf/default_conf.pb.txt`, configure the parameters of the `ReferenceLineEnd` plugin.

##### Starting with mainboard
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
