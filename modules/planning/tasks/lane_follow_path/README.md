planning-task-lane-follow-path
==============

## Introduction
`LaneFollowPath` decides on and generates a lane-following path.
As shown in the figure below:
![](./docs/images/1.png)

### GetStartPointSLState
This function obtains the Cartesian coordinates of the planning start point and then converts them to Frenet coordinates through the reference line.

### DecidePathBounds
This function decides the path boundaries (`candidate_path_boundaries`) within which the vehicle drives, by analyzing the surrounding lanes, the vehicle position, and static obstacles. This task obtains the ego vehicle position through `Frame`, obtains the lane information, reference line information, and static obstacle information through `ReferenceLineInfo`, and finally outputs the computed path to `ReferenceLineInfo`.

It initializes the path boundary with `PathBoundsDeciderUtil::InitPathBoundary`. The path boundary is updated according to the ego lane information and then extended according to the ego vehicle position. It calls `GetBoundaryFromStaticObstacles` to update the path boundary again according to the projection of static obstacles onto the reference line.

### OptimizePath
This function is used to optimize the path. It takes two parameters: `candidate_path_boundaries` (the path boundaries) and `candidate_path_data` (which stores the candidate path data).

The function computes the path curvature constraints (`ddl_bounds`), the curvature rate (`jerk_bound`), and the reference path (`ref_l` and `weight_ref_l`).
It calls `PathOptimizerUtil::OptimizePath` to optimize the path. If the optimization succeeds, the computed `path_data` object is added to `candidate_path_data`.

### AssessPath
This function is used to evaluate the candidate path data and select a final path.

The code traverses the candidate path data. For each path data, it uses the static function `IsValidRegularPath` of the `PathAssessmentDeciderUtil` class to check whether the path is valid. This determines whether the path is empty, whether the path deviates from the reference line, whether the path deviates from the road, whether the path collides with static obstacles, and whether the end of the path is on an adjacent lane in the opposite direction.


## Directory Structure 
```shell
modules/planning/tasks/lane_follow_path/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── lane_follow_path.cc
├── lane_follow_path.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── lane_follow_path.proto
└── README.md

```

## Module

### LaneChangePath plugin
apollo::planning::LaneFollowPath

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/lane_follow_path/conf/default_conf.pb.txt` | apollo::planning::LaneFollowPathConfig | Configuration file of LaneFollowPath |

#### Usage
##### Configuring the LaneFollowPath Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `LaneFollowPath` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `LaneFollowPath`.
```
task {
  name: "LANE_FOLLOW_PATH"
  type: "LaneFollowPath"
}
```
##### Configuring LaneFollowPath parameters
In `modules/planning/tasks/lane_follow_path/conf/default_conf.pb.txt`, configure the parameters of the `LaneFollowPath` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `LaneFollowPath` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
