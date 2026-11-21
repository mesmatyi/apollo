planning-task-open-space-trajectory-partition
==============

## Introduction
`OpenSpaceTrajectoryPartition` first linearly interpolates the trajectory generated in open space, then splits the trajectory into different segments according to whether the path moves forward or backward. Finally, it finds the point on the current trajectory closest to the ego vehicle, uses it as the trajectory start point, and plans the corresponding trajectory.

### InterpolateTrajectory
Linearly interpolates the generated trajectory `stitched_trajectory_result` by time interval, and then stores the interpolated trajectory in `interpolated_trajectory`.

### PartitionTrajectory
This function traverses all trajectory points and, according to the heading of the current trajectory point and the next trajectory point, determines whether the vehicle shifts gears. If it does, the trajectory is split at the current trajectory point, so that the vehicle heading is the same within each trajectory segment.

### AdjustRelativeTimeAndS
This function takes the vehicle's currently tracked trajectory point as the start point and adjusts the relative time and longitudinal distance of all trajectory points, so that the relative time and longitudinal distance at the currently tracked trajectory point are 0.

### InsertGearShiftTrajectory
When the vehicle reaches the end of a trajectory and is about to follow the next trajectory segment, a stopping trajectory is inserted so that the vehicle can adjust its wheel steering angle in place.


## Directory Structure 
```shell
modules/planning/tasks/open_space_trajectory_partition/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── open_space_trajectory_partition.cc
├── open_space_trajectory_partition.h
├── open_space_trajectory_partition_test.cc
├── plugins.xml
├── proto
│   ├── BUILD
│   └── open_space_trajectory_partition.proto
└── README.md
```

## Module Input, Output, and Configuration

### planning-task-open-space-trajectory-partition plugin

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/open_space_trajectory_partition/conf/default_conf.pb.txt` | apollo::planning::OpenSpaceTrajectoryPartitionConfig | Configuration file of OpenSpaceTrajectoryPartition |

#### Usage
##### Configuring the OpenSpaceTrajectoryPartition Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `OpenSpaceTrajectoryPartition` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `OpenSpaceTrajectoryPartition`.
```
task {
  name: "OPEN_SPACE_TRAJECTORY_PARTITION"
  type: "OpenSpaceTrajectoryPartition"
}
```
##### Configuring OpenSpaceTrajectoryPartition parameters
In `modules/planning/tasks/open_space_trajectory_partition/conf/default_conf.pb.txt`, configure the parameters of the `OpenSpaceTrajectoryPartition` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `OpenSpaceTrajectoryPartition` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
