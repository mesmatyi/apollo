planning-task-path-decider
==============

## Introduction
`PathDecider` decides on and generates the path decisions for obstacles.

### MakeObjectDecision
Calls MakeStaticObstacleDecision to make obstacle decisions, then determines whether backward obstacles need to be ignored. If the backward-obstacle ignore parameter in the configuration file is true, backward obstacles are ignored.

### MakeStaticObstacleDecision
This method is used for static obstacle decisions.
First, it computes the path in the Frenet coordinate system for later use, and computes the vehicle half width half_width and the lateral radius.

Then, the method traverses all obstacles.
1) For non-static and virtual obstacles, the method skips them directly;
2) For obstacles whose longitudinal decision is ignore, and obstacles whose lateral decision is ignore, the method also skips them directly;
3) For obstacles whose decision is stop, the method skips them directly;
4) For the obstacle whose id is blocking_obstacle_id, the method adds a stop decision to the obstacle;
5) For obstacles whose boundary_type is KEEP_CLEAR, the method skips them directly;
6) For obstacles whose end_s is less than the front_s of the Frenet path, or whose start_s is less than the back_s of the Frenet path, the method adds a not-in-s decision (equivalent to ignore);

Next, the method determines whether static obstacles can be nudged.
1) For obstacles that are too far laterally, the method ignores them and adds a not-in-l decision
2) For obstacles that are too close laterally to nudge, a stop decision is added as appropriate
3) In other cases, a left/right decision is added respectively

### GenerateObjectStopDecision
The function is used to generate a stop decision for an obstacle and returns the ObjectStop object_stop.
It first uses the MinRadiusStopDistance method to compute the shortest braking distance stop_distance for the obstacle, then sets the reason code and braking distance for object_stop (the braking distance is set to -1 * stop_distance), then computes the reference station corresponding to the braking distance as stop_ref_s, and finally generates the reference shortest stopping point according to stop_ref_s.

### IgnoreBackwardObstacle
Used to generate ignore results for obstacles.
It first obtains the start_s of the SL boundary of the reference line, that is, the start station of the SL boundary; then it traverses all obstacles, skipping static and virtual obstacles without judging them; next it determines the end station of the obstacle in the ST graph, that is, end_s. If end_s is less than start_s, the obstacle is considered a backward obstacle, and a longitudinal ignore-backward-obstacle decision result is made for it


## Directory Structure 
```shell
modules/planning/tasks/path_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── path_decider.cc
├── path_decider.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── path_decider.proto
└── README.md

```

## Module

### PathDeciderh plugin
apollo::planning::PathDecider

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/path_decider/conf/default_conf.pb.txt` | apollo::planning::PathDeciderConfig | Configuration file of PathDecider |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the PathDecider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `PathDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `PathDecider`.
```
task {
  name: "PATH_DECIDER"
  type: "PathDecider"
}
```
##### Configuring PathDecider parameters
In `modules/planning/tasks/path_decider/conf/default_conf.pb.txt`, configure the parameters of the `PathDecider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `PathDecider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
