planning-scenario-pull-over
============

## Introduction

`PullOverScenario`: The pull-over scenario. If the parameter `enable_pull_over_at_destination` is set to `true`, the vehicle automatically switches into `PullOverScenario` when it approaches the destination and completes the pull-over.

![](./images/pull_over_scenario.png)


### IsTransferable: scenario entry conditions
  1. The current command is `lane_follow_command`
  2. The reference line information is not empty
  3. The `FLAGS_enable_pull_over_at_destination` parameter configuration allows the pull-over scenario
  4. The ego vehicle is not changing lanes
  5. The distance from the ego vehicle to the target point satisfies the pull-over distance threshold
  6. Not in an overlap
  7. The rightmost lane allows pulling over

### Stages

| Stage name                                        | Type                                                   | Description                                                                                                           |
| -------------------------------------------- | ------------------------------------------------------| ---------------------------------------------------------------------------------------------------------------|
| `PULL_OVER_APPROACH`                         | `apollo::planning::PullOverStageApproach`             | The ego vehicle approaches the pull-over point  |
| `PULL_OVER_RETRY_APPROACH_PARKING`           | `apollo::planning::PullOverStageRetryApproachParking` | Approaches the Parking position point; after the ego vehicle speed and distance error reach thresholds, the next stage is entered                                                        |
| `PULL_OVER_RETRY_PARKING`                    | `apollo::planning::PullOverStageRetryParking`         | Executes openspace trajectory planning; the stage exits after the ego vehicle position and heading reach thresholds         |

#### 1. PULL_OVER_APPROACH
`PullOverStageApproach` : This stage is used for the ego vehicle to plan its approach to the pull-over point, obtain the pull-over trajectory, and check whether the pull-over is complete.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- CheckADCPullOver: checks the ego vehicle's PullOver state. Given the ego vehicle state, reference line information, scenario information, and planning context information, it determines the relationship to the pull-over point from the ego vehicle's current position and speed and decides the ego vehicle's `PullOverState`. The returned state is one of: `UNKNOWN`, `PASS_DESTINATION`, `APPROACHING`, `PARK_COMPLETE`, and `PARK_FAIL`. If the pull-over is complete, that is, the state is `PASS_DESTINATION` or `PARK_COMPLETE`, enter `FinishStage`, end the current Stage, and exit the current `PullOverScenario`; if the pull-over fails, that is, the state is `PARK_FAIL`, enter `FinishStage`, end the current Stage, and enter the `PULL_OVER_RETRY_APPROACH_PARKING` stage.
- CheckADCPullOverPathPoint: if the vehicle is still in the pull-over stage, check the key path_points and, based on the position and heading deviation between the path_point and the pull-over point, determine whether path_fail occurs. If `path_fail==true`, a `STOP` virtual obstacle is set before reaching the pull-over point. After the ego vehicle reaches the virtual obstacle, enter `FinishStage`, end the current Stage, and enter the `PULL_OVER_RETRY_APPROACH_PARKING` stage. If `path_fail==false`, it remains in the `PULL_OVER_APPROACH` stage.


**FinishStage**: The exit function of this stage. The input is `bool success`, that is, whether the pull-over succeeded in this stage.
- If `success==true`, exit the `PULL_OVER_APPROACH` stage and exit `PullOverScenario`.
- If `success==false`, exit the `PULL_OVER_APPROACH` stage and enter the `PULL_OVER_RETRY_APPROACH_PARKING` stage.


#### 2. PULL_OVER_RETRY_APPROACH_PARKING
`PullOverStageRetryApproachParking`: The previous stage failed to pull over directly, so this stage is entered to retry approaching the pull-over point.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- CheckADCStop: checks whether the ego vehicle is close to the stopping point; the input is the `Frame` information. When the ego vehicle speed is below a threshold and its position is within a threshold of the `open_space_pre_stop_fence_s` set by planning, enter `FinishStage` and end the current Stage.

**FinishStage**: The exit function of this stage.
- Exit the `PULL_OVER_RETRY_APPROACH_PARKING` stage and enter the `PULL_OVER_RETRY_PARKING` stage.

#### 3. PULL_OVER_RETRY_PARKING
`PullOverStageRetryParking`: After the previous stage brings the vehicle close to the stopping point, this stage is entered to perform the parking.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage. This stage mainly calls the Openspace trajectory planning method to plan the pull-over.
- CheckADCPullOverOpenSpace: checks whether the ego vehicle has stopped; the input is the `Frame` information. When the position and heading deviations between the ego vehicle and the target point are below thresholds, enter `FinishStage` and end the current Stage.

**FinishStage**: The exit function of this stage.
- Exit the `PULL_OVER_RETRY_PARKING` stage and exit the current `PullOverScenario`.

## Directory Structure

```shell
modules/planning/scenarios/pull_over/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   ├── pull_over_approach
│   │   └── pull_over_path.pb.txt
│   ├── pull_over_retry_approach_parking
│   │   └── open_space_pre_stop_decider.pb.txt
│   ├── pull_over_retry_parking
│   │   ├── open_space_roi_decider.pb.txt
│   │   └── open_space_trajectory_partition.pb.txt
│   └── scenario_conf.pb.txt
├── context.h
├── cyberfile.xml
├── images
│   └── pull_over_scenario.png
├── plugins.xml
├── proto
│   ├── BUILD
│   └── pull_over.proto
├── pull_over_scenario.cc
├── pull_over_scenario.h
├── pull_over_scenario_test.cc
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_retry_approach_parking.cc
├── stage_retry_approach_parking.h
├── stage_retry_approach_parking_test.cc
├── stage_retry_parking.cc
├── stage_retry_parking.h
├── stage_retry_parking_test.cc
├── util.cc
└── util.h
```

## Module

### PullOverScenario plugin

apollo::planning::PullOverScenario

#### Configuration files

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| --------------------------------------------------------------------- | ---------------- | ---------------- |
| `modules/planning/scenarios/pull_over/conf/scenario_conf.pb.txt`                                                 | `apollo::planning::ScenarioPullOverConfig` |Scenario configuration file                                     |
| `modules/planning/scenarios/pull_over/conf/pipeline.pb.txt`                                                      | Scenario pipeline file                                   |
| `modules/planning/scenarios/pull_over/conf/pull_over_approach/pull_over_path.pb.txt`                             | `apollo::planning::ScenarioPipeline` |Configuration file of the `pull_over_approach` stage                |
| `modules/planning/scenarios/pull_over/conf/pull_over_retry_approach_parking/open_space_pre_stop_decider.pb.txt`  | `apollo::planning::OpenSpacePreStopDeciderConfig` |Configuration file of the `pull_over_retry_approach_parking` stage  |
| `modules/planning/scenarios/pull_over/conf/pull_over_retry_parking/open_space_roi_decider.pb.txt`                | `apollo::planning::OpenSpaceRoiDeciderConfig` |Configuration file of the `pull_over_retry_parking` stage           |
| `modules/planning/scenarios/pull_over/conf/pull_over_retry_parking/open_space_trajectory_partition.pb.txt`       | `apollo::planning::OpenSpaceTrajectoryPartitionConfig` |Configuration file of the `pull_over_retry_parking` stage           |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the PullOverScenario plugin to load
- Add the `standard_planning_config` configuration to the `modules/planning/planning_component/conf/planning_config.pb.txt` file to use the current scenario plugin.
  ``` shell
  # modules/planning/planning_component/conf/planning_config.pb.txt

  standard_planning_config {
    planner_type: ......
    planner_public_road_config {
      ......
      scenario {
        name: "PULL_OVER"
        type: "PullOverScenario"
      }
      ......
    }
  }
  ```
- Edit all `Stage`s of this scenario and the `Task` list corresponding to each `Stage` in `modules/planning/scenarios/pull_over/conf/pipeline.pb.txt`.
- Custom development is also supported. A newly added `Stage` plugin must be added to `modules/planning/scenarios/pull_over/plugins.xml`.

    ```shell
    # modules/planning/scenarios/pull_over/plugins.xml
    <library path="modules/planning/scenarios/pull_over/libpull_over_scenario.so">
        <class type="apollo::planning::PullOverScenario" base_class="apollo::planning::Scenario"></class>
        <class type="apollo::planning::PullOverStageApproach" base_class="apollo::planning::Stage"></class>
        <class type="apollo::planning::PullOverStageRetryApproachParking" base_class="apollo::planning::Stage"></class>
        <class type="apollo::planning::PullOverStageRetryParking" base_class="apollo::planning::Stage"></class>
    </library>
    ```


##### Configuring PullOverScenario parameters
- `proto` file configuration: `modules/planning/scenarios/pull_over/proto/pull_over.proto`
- PullOverScenario parameters are configured in: `modules/planning/scenarios/pull_over/conf/scenario_conf.pb.txt`
- The parameter configuration of each `Stage` is located in the `modules/planning/scenarios/pull_over/conf/<stage_name>/` folder

  ```shell
  # modules/planning/scenarios/pull_over/conf/
  ├── pipeline.pb.txt
  ├── pull_over_approach
  │   └── pull_over_path.pb.txt
  ├── pull_over_retry_approach_parking
  │   └── open_space_pre_stop_decider.pb.txt
  ├── pull_over_retry_parking
  │   ├── open_space_roi_decider.pb.txt
  │   └── open_space_trajectory_partition.pb.txt
  └── scenario_conf.pb.txt
  ```
