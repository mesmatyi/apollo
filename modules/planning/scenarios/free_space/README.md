planning-scenario-free-space
============

## Introduction

`FreeSpaceScenario` is used to generate the corresponding trajectory with the openspace algorithm, given a specified drivable area boundary and a target pose.

### Scenario entry conditions
  1. `planning_command` has the corresponding custom command.
  2. The ego vehicle is stationary.
  3. `planning_command` contains the drivable area boundary, whose boundary points are published in counterclockwise order, and the drivable area boundary contains the ego vehicle's start and end points.
  4. The boundary points of the non-drivable area boundaries are published in clockwise order, and the non-drivable area boundaries must not contain the ego vehicle's start and end points.

### Stages.

| Stage name             | Type                               | Description                               |
| ------------------ | ---------------------------------- | ---------------------------------- |
| `STAGE_FREE_SPACE` | `apollo::planning::StageFreeSpace` | Generates a trajectory that drives to the specified pose within the specified area |

`STAGE_FREE_SPACE` generates a feasible path to the target point. This stage requires that the non-drivable areas lie within the drivable area boundary, and that the area boundary constraints can be converted into the form `Ax>b`.

## Directory Structure

```shell

modules/planning/scenarios/free_space/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── scenario_conf.pb.txt
│   └── stage_free_space
│       ├── open_space_trajectory_partition.pb.txt
│       └── open_space_trajectory_provider.pb.txt
├── cyberfile.xml
├── free_space_scenario.cc
├── free_space_scenario.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── free_space_scenario.proto
├── README.md
├── stage_free_space.cc
└── stage_free_space.h

```

## Module

### FreeSpaceScenario plugin

apollo::planning::FreeSpaceScenario

#### Configuration

| File path                                                                                             | Type/Structure                                              | <div style="width: 300pt">Description</div>                                        |
| ---------------------------------------------------------------------------------------------------- | ------------------------------------------------------ | --------------------------------------------------------------------------- |
| `modules/planning/scenarios/free_space/conf/scenario_conf.pb.txt`                                    | `apollo::planning::ScenarioFreeSpaceConfig`            | Scenario configuration file                                                              |
| `modules/planning/scenarios/free_space/conf/pipeline.pb.txt`                                         | `apollo::planning::ScenarioPipeline`                   | Scenario pipeline file                                                            |
| `modules/planning/scenarios/free_space/conf/stage_free_space/open_space_trajectory_partition.pb.txt` | `apollo::planning::OpenSpaceTrajectoryPartitionConfig` | Configuration file of the `open_space_trajectory_partition` task in the `stage_free_space` stage |
| `modules/planning/scenarios/free_space/conf/stage_free_space/open_space_trajectory_provider.pb.txt`  | `apollo::planning::OpenSpaceTrajectoryProviderConfig`  | Configuration file of the `open_space_trajectory_provider` task in the `stage_free_space` stage   |
