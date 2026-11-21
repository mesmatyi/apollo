planning-task-lane-borrow-path
==============

## Introduction

`LaneBorrowPath` is used to decide on and generate a lane-borrow path. If an obstacle stays ahead on the road for a long time and blocks it, and the vehicle cannot bypass it within the current lane, the vehicle needs to borrow the adjacent lane to bypass the obstacle. After the vehicle passes the obstacle, it immediately returns to its original lane. This task obtains the ego vehicle position through `Frame`, obtains the lane information, reference line information, and static obstacle information through `ReferenceLineInfo`, and finally outputs the computed path to `ReferenceLineInfo`.
![](./docs/images/1.png)

### IsNecessaryToBorrowLane()

The function first checks whether the vehicle is currently in a lane-borrow scenario. If it is, and the ego lane has been used for a while, the function switches to the non-lane-borrow scenario and clears the decided side-pass direction.

If the path planning state was not in a lane-borrow scenario to begin with, the function prints the ID of the blocking obstacle and checks whether the ADC requirements for lane borrowing are met. These requirements include whether there is a single reference line, whether the speed is within the side-pass speed range, whether the blocking obstacle is far from the intersection, whether it is a long-term blocking obstacle, whether there is a blocking obstacle before the target position, and whether the obstacle is movable.

If all conditions are true, the function checks whether the lane can be borrowed and, if so, updates the decided side-pass direction. If neither the left nor the right lane can be borrowed, the lane borrow task is exited.

### DecidePathBounds()
This function decides the path boundaries (`candidate_path_boundaries`) within which the vehicle drives, by analyzing the surrounding lanes, the vehicle position, and static obstacles.

It traverses the decided side-pass directions (`decided_side_pass_direction_`) and tries to find path boundaries for each direction.
It initializes the path boundary with `PathBoundsDeciderUtil::InitPathBoundary`. The path boundary is updated according to the lane information and the ego position. It is then updated again according to the projection of static obstacles onto the reference line.

### OptimizePath()
This function is used to optimize the path. It takes two parameters: `candidate_path_boundaries` (the path boundaries) and `candidate_path_data` (which stores the candidate path data).

The function computes the path curvature constraints (`ddl_bounds`), the curvature rate (`jerk_bound`), and the reference path (`ref_l` and `weight_ref_l`).
It calls `PathOptimizerUtil::OptimizePath` to optimize the path. If the optimization succeeds, the computed `path_data` object is added to `candidate_path_data`.

### AssessPath()
This function is used to evaluate the candidate path data and select a final path.

The code traverses the candidate path data. For each path data, it uses the static function `IsValidRegularPath` of the `PathAssessmentDeciderUtil` class to check whether the path is valid. This determines whether the path is empty, whether the path deviates from the reference line, whether the path deviates from the road, whether the path collides with static obstacles, and whether the end of the path is on an adjacent lane in the opposite direction. `TrimTailingOutLanePoints` removes path points at the end of the road that are not in the ego lane. `ComparePathData` selects a suitable path based on factors such as road length, whether an opposite-direction lane is borrowed, the ego vehicle's lateral position, and returning to the ego lane sooner.

## Directory Structure 
```shell
modules/planning/tasks/lane_borrow_path/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── lane_borrow_path.cc
├── lane_borrow_path.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── lane_borrow_path.proto
└── README.md

```

## Module Input, Output, and Configuration

### planning-task-lane-borrow-path plugin

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/lane_borrow_path/conf/default_conf.pb.txt` | apollo::planning::LaneBorrowPathConfig | Configuration file of LaneBorrowPath |


#### Usage

##### Configuring the LaneBorrowPath Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `LaneBorrowPath` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `LaneBorrowPath`.
```
task {
  name: "LANE_BORROW_PATH"
  type: "LaneBorrowPath"
}
```
##### Configuring LaneBorrowPath parameters
In `modules/planning/tasks/lane_borrow_path/conf/default_conf.pb.txt`, configure the parameters of the `LaneBorrowPath` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `LaneBorrowPath` plugin
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
