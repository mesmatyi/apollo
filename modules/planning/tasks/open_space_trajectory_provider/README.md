planning-task-open-space-trajectory-provider
==============

## Introduction
`OpenSpaceTrajectoryProvider` is used to call the open-space algorithm to generate the corresponding trajectory, given the specified planning boundary and target point pose.

### GenerateStopTrajectory
When the vehicle reaches the planning end point, while the open-space trajectory is being computed, or in the `PARK_AND_GO_CHECK` stage, this function is called to generate a stopping trajectory.

### GenerateTrajectoryThread
Generates the trajectory by calling the function `OpenSpaceTrajectoryOptimizer::Plan`: the `HybridAStar::Plan` function uses Hybrid A* and Reeds Shepp curves to compute a coarse path solution; the `OpenSpaceTrajectoryOptimizer::GenerateDecoupledTraj` function performs path optimization and speed optimization on the trajectory (see: `JINYUN Z, RUNXIN H, WANG Y, et al. DL-IAPS and PJSO: A Path/Speed Decoupled Trajectory Optimization and its Application in Autonomous Driving[J]. arXiv: Robotics,arXiv: Robotics, 2020.`); the `OpenSpaceTrajectoryOptimizer::GenerateDistanceApproachTraj` function generates the trajectory (see: `RUNXIN H, JINYUN Z, SHU J, et al. TDR-OBCA: A Reliable Planner for Autonomous Driving in Free-Space Environment[J]. Cornell University - arXiv,Cornell University - arXiv, 2020.`)

### IsVehicleNearDestination
Determines whether the vehicle has reached the destination by comparing the angle and distance between the vehicle and the end point and the vehicle speed.

## Directory Structure 
```shell
modules/planning/tasks/open_space_trajectory_provider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── open_space_trajectory_optimizer.cc
├── open_space_trajectory_optimizer.h
├── open_space_trajectory_provider.cc
├── open_space_trajectory_provider.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── open_space_trajectory_provider.proto
└── README.md
```

## Module Input, Output, and Configuration

### planning-task-open-space-trajectory-provider plugin

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/open_space_trajectory_provider/conf/default_conf.pb.txt` | apollo::planning::OpenSpaceTrajectoryProviderConfig | Configuration file of OpenSpaceTrajectoryProvider |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the OpenSpaceTrajectoryProvider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `OpenSpaceTrajectoryProvider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `OpenSpaceTrajectoryProvider`.
```
task {
  name: "OPEN_SPACE_TRAJECTORY_PROVIDER"
  type: "OpenSpaceTrajectoryProvider"
}
```
##### Configuring OpenSpaceTrajectoryProvider parameters
In `modules/planning/tasks/open_space_trajectory_provider/conf/default_conf.pb.txt`, configure the parameters of the `OpenSpaceTrajectoryProvider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `OpenSpaceTrajectoryProvider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
