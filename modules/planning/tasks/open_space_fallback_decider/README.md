planning-task-open-space-fallback-decider
==============

## Introduction
`OpenSpaceFallbackDecider` detects whether the trajectory in open space collides with obstacles and takes the corresponding strategy.


### BuildPredictedEnvironment
Based on the obstacle information input from prediction, it builds obstacle boundaries at different time intervals and puts the obstacle information into `predicted_bounding_rectangles`.

### IsCollisionFreeTrajectory
Determines whether the ego vehicle trajectory is collision-free with the obstacle trajectories. When it returns false, a uniform-deceleration stopping trajectory is generated according to the position of the collision point, so that the vehicle stops.

### IsCollisionFreeEgoBox
Determines whether the expanded vehicle box collides with static obstacles. When it returns false, the vehicle brakes hard and sets fallback_flag to true, so that the trajectory is replanned in the task `OpenSpaceTrajectoryProvider`.

## Directory Structure 
```shell
modules/planning/tasks/open_space_fallback_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── open_space_fallback_decider.cc
├── open_space_fallback_decider.h
├── open_space_fallback_decider_test.cc
├── plugins.xml
├── proto
│   ├── BUILD
│   └── open_space_fallback_decider.proto
└── README.md
```

## Module

### OpenSpaceFallbackDecider plugin
apollo::planning::OpenSpaceFallbackDecider

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/open_space_fallback_decider/conf/default_conf.pb.txt` | apollo::planning::OpenSpaceFallBackDeciderConfig | Configuration file of OpenSpaceFallbackDecider |
|`modules/common/data/vehicle_param.pb.txt`|`apollo::common::VehicleConfig`|Vehicle chassis configuration file|

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the OpenSpaceFallbackDecider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `OpenSpaceFallbackDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `OpenSpaceFallbackDecider`.
```
task {
  name: "OPEN_SPACE_FALLBACK_DECIDER"
  type: "OpenSpaceFallbackDecider"
}
```
##### Configuring OpenSpaceFallbackDecider parameters
In `modules/planning/tasks/open_space_fallback_decider/conf/default_conf.pb.txt`, configure the parameters of the `OpenSpaceFallbackDecider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `OpenSpaceFallbackDecider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
