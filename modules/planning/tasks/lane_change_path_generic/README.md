planning-task-lane-change-path
==============

## Introduction
`LaneChangePath` decides on and generates a lane change path. While the vehicle drives along the routing path in RoutingResponse, it switches from one lane to an adjacent lane. The reason for changing lanes is that it is specified in RoutingResponse, rather than the vehicle actively deciding to change lanes, which differs somewhat from lane changes in human driving. Because RoutingResponse is a lane-level navigation route, a lane change is needed between two consecutive passages in the routing path. This task obtains the ego vehicle position through `Frame`, obtains the lane information, reference line information, and static obstacle information through `ReferenceLineInfo`, obtains the previous frame's lane change status through injector_, and finally outputs the computed path to `ReferenceLineInfo`.
![](./docs/images/1.png)

### UpdateLaneChangeStatus
Determines whether to change lanes according to the vehicle state, the number of reference lines, and whether the vehicle was changing lanes in the previous frame. `UpdateLaneChangeStatus` traverses all dynamic obstacles, determines the target driving direction, sets the corresponding safety distance according to the speed between the ego vehicle and the target, and uses `HysteresisFilter` to determine whether the lane change is safe.

### DecidePathBounds
This function decides the path boundaries (`candidate_path_boundaries`) within which the vehicle drives, by analyzing the surrounding lanes, the vehicle position, and static obstacles.

It initializes the path boundary with `PathBoundsDeciderUtil::InitPathBoundary`. The path boundary is updated according to the ego lane information and then extended according to the ego vehicle position. It calls `GetBoundaryFromLaneChangeForbiddenZone` to update the path before the lane change point according to the ego position and the current path boundary. It calls `GetBoundaryFromStaticObstacles` to update the path boundary again according to the projection of static obstacles onto the reference line.

### OptimizePath
This function is used to optimize the path. It takes two parameters: `candidate_path_boundaries` (the path boundaries) and `candidate_path_data` (which stores the candidate path data).

The function computes the path curvature constraints (`ddl_bounds`), the curvature rate (`jerk_bound`), and the reference path (`ref_l` and `weight_ref_l`).
It calls `PathOptimizerUtil::OptimizePath` to optimize the path. If the optimization succeeds, the computed `PathData` object is added to `candidate_path_data`.

### AssessPath
This function is used to evaluate the candidate path data and select a final path.

The code traverses the candidate path data. For each path data, it uses the static function `IsValidRegularPath` of the `PathAssessmentDeciderUtil` class to check whether the path is valid. This determines whether the path is empty, whether the path deviates from the reference line, whether the path deviates from the road, whether the path collides with static obstacles, and whether the end of the path is on an adjacent lane in the opposite direction. `TrimTailingOutLanePoints` removes path points at the end of the road that are not in the ego lane.

## Directory Structure 
```shell
modules/planning/tasks/lane_change_path/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── lane_change_path.cc
├── lane_change_path.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── lane_change_path.proto
└── README.md

```

## Module

### LaneChangePath plugin
apollo::planning::LaneChangePath
#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/lane_change_path/conf/default_conf.pb.txt` | apollo::planning::LaneChangePathConfig | Configuration file of LaneChangePath |

#### Usage
##### Configuring the LaneChangePath Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `LaneChangePath` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, LaneChangePath.
```
task {
  name: "LANE_CHANGE_PATH"
  type: "LaneChangePath"
}
```
##### Configuring LaneChangePath parameters
In `modules/planning/tasks/lane_change_path/conf/default_conf.pb.txt`, configure the parameters of the `LaneChangePath` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `LaneChangePath` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
