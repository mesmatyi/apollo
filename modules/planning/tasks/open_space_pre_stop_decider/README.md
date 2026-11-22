planning-task-open-space-pre-stop-decider
==============

## Introduction
When performing parking and pull-over tasks in open space, `OpenSpacePreStopDecider` generates a stopping point on the public road. After the vehicle stops at the stopping point, it switches to the open-space algorithm.

### CheckPullOverPreStop
This function computes the longitudinal projection point of the pull-over target point on the road.

### CheckParkingSpotPreStop
Computes the longitudinal projection point of the center point of the target parking spot on the road

### SetParkingSpotStopFence
This function computes and generates a stop wall according to the longitudinal projection point of the parking spot.

### SetPullOverStopFence
This function computes and generates a stop wall according to the longitudinal projection point of the pull-over point.

## Directory Structure 
```shell
modules/planning/tasks/open_space_pre_stop_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── open_space_pre_stop_decider.cc
├── open_space_pre_stop_decider.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── open_space_pre_stop_decider.proto
└── README.md
```

## Module Input, Output, and Configuration

### planning-task-open-space-pre-stop-decider plugin

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/open_space_pre_stop_decider/conf/default_conf.pb.txt` | apollo::planning::OpenSpacePreStopDeciderConfig | Configuration file of OpenSpacePreStopDecider |

#### Usage
##### Configuring the OpenSpacePreStopDecider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `OpenSpacePreStopDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `OpenSpacePreStopDecider`.
```
task {
  name: "OPEN_SPACE_PRE_STOP_DECIDER"
  type: "OpenSpacePreStopDecider"
}
```
##### Configuring OpenSpacePreStopDecider parameters
In `modules/planning/tasks/open_space_pre_stop_decider/conf/default_conf.pb.txt`, configure the parameters of the `OpenSpacePreStopDecider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `OpenSpacePreStopDecider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
