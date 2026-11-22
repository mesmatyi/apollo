
planning-scenario-emergency-stop
============

## Introduction

`EmergencyStopScenario`: The emergency stop scenario. If the vehicle receives the PadMessage command "PadMessage::STOP" while driving, the ego vehicle computes the stopping distance and stops directly.


### IsTransferable: scenario entry conditions
  1. The received PadMessage command is `PadMessage::STOP`

### Stages

| Stage name                                                     | Type                                                                        | Description                     |
| ---------------------------------------------------------- | --------------------------------------------------------------------------- | ------------------------ |
| `EMERGENCY_STOP_APPROACH`                | `apollo::planning::EmergencyStopStageApproach`               | Computes the braking distance from the maximum deceleration; the ego vehicle decelerates and brakes, and the stage exits once the deceleration reaches a threshold |
| `EMERGENCY_STOP_STANDBY`                 | `apollo::planning::EmergencyStopStageStandby`              | The ego vehicle remains in the emergency stop state; the stage exits once `PadMessage` switches from `STOP` to another state   |


#### 1. EMERGENCY_STOP_APPROACH
`EmergencyStopStageApproach` : This stage is used for the ego vehicle to slow down before the emergency stop, and it exits once the ego vehicle speed reaches a threshold.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- Ego vehicle light settings: turn on the hazard warning flashers with `frame->mutable_reference_line_info()->front().SetEmergencyLight()`.
- Build a virtual obstacle: according to the stopping point computed by planning, build a virtual obstacle with the id `EMERGENCY_STOP`.
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- Check the ego vehicle state: check whether the ego vehicle has slowed down to the target speed. If so, enter `FinishStage`, end the current Stage, and enter the `EMERGENCY_STOP_STANDBY` stage. If the target speed has not been reached, it remains in the `EMERGENCY_STOP_APPROACH` stage and returns the status value `StageStatusType::RUNNING`.

**FinishStage**: The exit function of this stage.
- Exit the `EMERGENCY_STOP_APPROACH` stage and enter the `EMERGENCY_STOP_STANDBY` stage.

#### 2. EMERGENCY_STOP_STANDBY
`EmergencyStopStageStandby` : This stage is used for the ego vehicle to maintain the emergency stop state.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- Build a virtual obstacle: according to the stopping point computed by planning, build a virtual obstacle with the id `EMERGENCY_PULL_OVER`.
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- Check PadMessage: after `PadMessage` switches from `STOP` to another state, enter `FinishStage` and end the current Stage; otherwise it remains in the `EMERGENCY_STOP_STANDBY` stage and returns the status value `StageStatusType::RUNNING`.

**FinishStage**: The exit function of this stage.
- Exit the `EMERGENCY_STOP_STANDBY` stage and exit the `EmergencyStopScenario` scenario.

## Directory Structure

```shell
modules/planning/scenarios/emergency_stop/
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   └── scenario_conf.pb.txt
├── cyberfile.xml
├── emergency_stop_scenario.cc
├── emergency_stop_scenario.h
├── emergency_stop_scenario_test.cc
├── plugins.xml
├── proto
│   ├── BUILD
│   └── emergency_stop.proto
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_standby.cc
├── stage_standby.h
└── stage_standby_test.cc
```

## Module

### EmergencyStopScenario plugin

apollo::planning::EmergencyStopScenario


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| --------------------------------------------------------------------- | ---------------- | ---------------- |
| `modules/planning/scenarios/emergency_stop/conf/scenario_conf.pb.txt`| `apollo::planning::ScenarioEmergencyStopConfig` | Scenario configuration file   |
| `modules/planning/scenarios/emergency_stop/conf/pipeline.pb.txt`     | `apollo::planning::ScenarioPipeline` | Scenario pipeline file |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |

#### Usage
##### Configuring the EmergencyStopScenario plugin to load
- Add the `standard_planning_config` configuration to the `modules/planning/planning_component/conf/planning_config.pb.txt` file to use the current scenario plugin.
  ``` shell
  # modules/planning/planning_component/conf/planning_config.pb.txt

  standard_planning_config {
    planner_type: ......
    planner_public_road_config {
      ......
      scenario {
        name: "EMERGENCY_STOP"
        type: "EmergencyStopScenario"
      }
      ......
    }
  }
  ```
- Edit all `Stage`s of this scenario and the `Task` list corresponding to each `Stage` in `modules/planning/scenarios/emergency_stop/conf/pipeline.pb.txt`.
- Custom development is also supported. A newly added `Stage` plugin must be added to `modules/planning/scenarios/emergency_stop/plugins.xml`.
    ```shell
    # modules/planning/scenarios/emergency_stop/plugins.xml
    <library path="modules/planning/scenarios/emergency_stop/libemergency_stop_scenario.so">
        <class type="apollo::planning::EmergencyStopScenario" base_class="apollo::planning::Scenario"></class>
        <class type="apollo::planning::EmergencyStopStageApproach" base_class="apollo::planning::Stage"></class>
        <class type="apollo::planning::EmergencyStopStageStandby" base_class="apollo::planning::Stage"></class>
    </library>
    ```

##### Configuring EmergencyStopScenario parameters
- `proto` file configuration: `modules/planning/scenarios/emergency_stop/proto/emergency_stop.proto`
- EmergencyStopScenario parameters are configured in: `modules/planning/scenarios/emergency_stop/conf/scenario_conf.pb.txt`
