planning-scenario-emergency-pull-over
============

## Introduction

`EmergencyPullOverScenario`: The emergency pull-over scenario. If the vehicle receives the PadMessage command "PULL_OVER" while driving, it finds a suitable position nearby and stops within the current lane. Compared with stopping directly, this ensures stopping safety and comfort during driving.

![](./images/emergency_pull_over_scenario.png)


### IsTransferable: scenario entry conditions
  1. The reference line of the current frame is not empty
  2. The received PadMessage command is `PadMessage::PULL_OVER`

### Stages

| Stage name                                        | Type                                                   | Description                                                                            |
| -------------------------------------------- | ------------------------------------------------------| -------------------------------------------------------------------------------|
| `EMERGENCY_PULL_OVER_SLOW_DOWN`              | `apollo::planning::EmergencyPullOverStageSlowDown`    | Slow-down stage before stopping: modifies the ego vehicle speed limit of the reference line, and exits once the ego vehicle speed reaches a threshold                                  |
| `EMERGENCY_PULL_OVER_APPROACH`               | `apollo::planning::EmergencyPullOverStageApproach`    | After slowing down, the `PULL_OVER_PATH` task computes the stopping point and the ego vehicle approaches the stopping end point; the stage exits once the ego vehicle speed and distance reach thresholds   |
| `EMERGENCY_PULL_OVER_STANDBY`                | `apollo::planning::EmergencyPullOverStageStandby`     | The ego vehicle remains stopped at the stopping point; the stage exits once `PadMessage` switches from `PULL_OVER` to another state                 |


#### 1. EMERGENCY_PULL_OVER_SLOW_DOWN
`EmergencyPullOverStageSlowDown` : This stage is used for the ego vehicle to slow down before stopping. It modifies the ego vehicle speed limit of the reference line and exits once the ego vehicle speed reaches a threshold.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- Ego vehicle speed limit adjustment: obtain `target_slow_down_speed` from the scenario configuration file and call the `reference_line_info.LimitCruiseSpeed()` function to modify the speed limit, thereby slowing the ego vehicle down.
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- Check the ego vehicle state: check whether the ego vehicle has slowed down to the target speed. If so, enter `FinishStage`, end the current Stage, and enter the `EMERGENCY_PULL_OVER_APPROACH` stage. If the target speed has not been reached, it remains in the `EMERGENCY_PULL_OVER_SLOW_DOWN` stage and returns the status value `StageStatusType::RUNNING`.

**FinishStage**: The exit function of this stage.
- Exit the `EMERGENCY_PULL_OVER_SLOW_DOWN` stage, set `pull_over_status->set_plan_pull_over_path(true)` for the ego vehicle, and enter the `EMERGENCY_PULL_OVER_APPROACH` stage.

#### 2. EMERGENCY_PULL_OVER_APPROACH
`EmergencyPullOverStageApproach` : This stage is used for the ego vehicle to approach the stopping target point after slowing down.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- Set the ego vehicle speed limit with `reference_line_info.LimitCruiseSpeed`; turn on the turn signal to indicate a temporary pull-over stop with `reference_line_info.SetTurnSignal(VehicleSignal::TURN_RIGHT)`.
- Build a virtual obstacle: according to the stopping point computed by planning, build a virtual obstacle with the id `EMERGENCY_PULL_OVER`.
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- Check the ego vehicle state: check whether the ego vehicle speed and distance satisfy the thresholds. If so, enter `FinishStage`, end the current Stage, and enter the `EMERGENCY_PULL_OVER_STANDBY` stage. If not, it remains in the `EMERGENCY_PULL_OVER_APPROACH` stage and returns the status value `StageStatusType::RUNNING`.

**FinishStage**: The exit function of this stage.
- Exit the `EMERGENCY_PULL_OVER_APPROACH` stage and enter the `EMERGENCY_PULL_OVER_STANDBY` stage.

#### 3. EMERGENCY_PULL_OVER_STANDBY
`EmergencyPullOverStageStandby` : This stage is used for the ego vehicle to maintain the emergency pull-over stopped state.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- Set the ego vehicle speed limit with `reference_line_info.LimitCruiseSpeed`; turn on the hazard warning flashers with `reference_line_info.SetEmergencyLight()`; turn off the turn signal with `reference_line_info.SetTurnSignal(VehicleSignal::TURN_NONE)`.
- Build a virtual obstacle: according to the stopping point computed by planning, build a virtual obstacle with the id `EMERGENCY_PULL_OVER`.
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- Check PadMessage: after `PadMessage` switches from `PULL_OVER` to another state, enter `FinishStage` and end the current Stage; otherwise it remains in the `EMERGENCY_PULL_OVER_STANDBY` stage and returns the status value `StageStatusType::RUNNING`.

**FinishStage**: The exit function of this stage.
- Exit the `EMERGENCY_PULL_OVER_STANDBY` stage and exit the `EmergencyPullOverScenario` scenario.

## Directory Structure

```shell
modules/planning/scenarios/emergency_pull_over/
├── BUILD
├── conf
│   ├── emergency_pull_over_approach
│   │   └── pull_over_path.pb.txt
│   ├── pipeline.pb.txt
│   └── scenario_conf.pb.txt
├── cyberfile.xml
├── emergency_pull_over_scenario.cc
├── emergency_pull_over_scenario.h
├── emergency_pull_over_scenario_test.cc
├── images
│   └── emergency_pull_over_scenario.png
├── plugins.xml
├── proto
│   ├── BUILD
│   └── emergency_pull_over.proto
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_slow_down.cc
├── stage_slow_down.h
├── stage_slow_down_test.cc
├── stage_standby.cc
├── stage_standby.h
└── stage_standby_test.cc
```

## Module

### EmergencyPullOverScenario plugin

apollo::planning::EmergencyPullOverScenario


#### Configuration files

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| --------------------------------------------------------------------- | ---------------- | ---------------- |
| `modules/planning/scenarios/emergency_pull_over/conf/scenario_conf.pb.txt`| `apollo::planning::ScenarioEmergencyPullOverConfig` | Scenario configuration file   |
| `modules/planning/scenarios/emergency_pull_over/conf/pipeline.pb.txt`     | `apollo::planning::ScenarioPipeline` | Scenario pipeline file |
| `modules/planning/scenarios/emergency_pull_over/conf/emergency_pull_over_approach/pull_over_path.pb.txt`                 | `apollo::planning::PullOverPathConfig`              | Task configuration file               |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |

#### Usage
##### Configuring the EmergencyPullOverScenario plugin to load
- Add the `standard_planning_config` configuration to the `modules/planning/planning_component/conf/planning_config.pb.txt` file to use the current scenario plugin.
  ``` shell
  # modules/planning/planning_component/conf/planning_config.pb.txt

  standard_planning_config {
    planner_type: ......
    planner_public_road_config {
      scenario {
        name: "EMERGENCY_PULL_OVER"
        type: "EmergencyPullOverScenario"
      }
      ......
    }
  }
  ```
- Edit all `Stage`s of this scenario and the `Task` list corresponding to each `Stage` in `modules/planning/scenarios/emergency_pull_over/conf/pipeline.pb.txt`.
- Custom development is also supported. A newly added `Stage` plugin must be added to `modules/planning/scenarios/emergency_pull_over/plugins.xml`.
  ```shell
  # modules/planning/scenarios/emergency_pull_over/plugins.xml
  <library path="modules/planning/scenarios/emergency_pull_over/libemergency_pull_over_scenario.so">
      <class type="apollo::planning::EmergencyPullOverScenario" base_class="apollo::planning::Scenario"></class>
      <class type="apollo::planning::EmergencyPullOverStageApproach" base_class="apollo::planning::Stage"></class>
      <class type="apollo::planning::EmergencyPullOverStageSlowDown" base_class="apollo::planning::Stage"></class>
      <class type="apollo::planning::EmergencyPullOverStageStandby" base_class="apollo::planning::Stage"></class>
  </library>
  ```


##### Configuring EmergencyPullOverScenario parameters
- `proto` file configuration: `modules/planning/scenarios/emergency_pull_over/proto/emergency_pull_over.proto`
- EmergencyPullOverScenario parameters are configured in: `modules/planning/scenarios/emergency_pull_over/conf/scenario_conf.pb.txt`
- - The parameter configuration of each `Stage` is located in the `modules/planning/scenarios/emergency_pull_over/conf/<stage_name>/` folder

  ```shell
  # modules/planning/scenarios/emergency_pull_over/conf/
  ├── emergency_pull_over_approach/
  │   └── pull_over_path.pb.txt
  ├── pipeline.pb.txt
  └── scenario_conf.pb.txt
  ```
