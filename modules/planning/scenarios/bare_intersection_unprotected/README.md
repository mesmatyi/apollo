planning-scenario-bare-intersection-unprotected
============

## Introduction

`BareIntersectionUnprotectedScenario`: The unprotected bare intersection scenario. At such an intersection there is neither a stop sign nor a traffic light, and the vehicle switches to this scenario within a certain distance before the intersection.

![](./images/bare_intersection_scenario.png)

### IsTransferable: scenario entry conditions
  1. The current command is `lane_follow_command`
  2. The reference line is not empty
  3. The sorted encountered overlaps are not empty
- Obtaining overlap information: the first-encountered `traffic_sign_overlap` and `pnc_junction_overlap` are obtained from `first_encounter_overlaps_` of the reference line information.
- Considering the nearest `pnc_junction_overlap`, if the ego vehicle is less than `start_bare_intersection_scenario_distance` from the intersection and more than 0 meters away, it means the unprotected bare intersection scenario is entered.



### Stages

| Stage name                                                     | Type                                                                        | Description                     |
| ---------------------------------------------------------- | --------------------------------------------------------------------------- | ------------------------ |
| `BARE_INTERSECTION_UNPROTECTED_APPROACH`                | `apollo::planning::BareIntersectionUnprotectedStageApproach`               | The ego vehicle slows down and approaches the unprotected intersection area |
| `BARE_INTERSECTION_UNPROTECTED_INTERSECTION_CRUISE`                 | `apollo::planning::BareIntersectionUnprotectedStageIntersectionCruise`              | The ego vehicle passes through the intersection   |


#### 1. BARE_INTERSECTION_UNPROTECTED_APPROACH
`BareIntersectionUnprotectedStageApproach` : This stage is used for the ego vehicle to slow down and approach the unprotected intersection area, and it exits once the ego vehicle's distance reaches a threshold.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- Check whether the intersection exists through the overlaps in reference_line_info. If it does not exist, call `FinishScenario()` and exit the `BareIntersectionUnprotectedScenario` scenario directly.
- Check the distance from the ego vehicle to the intersection. If `distance_adc_to_pnc_junction < -kPassStopLineBuffer` is satisfied, enter `FinishStage`, end the current Stage, and enter the `BARE_INTERSECTION_UNPROTECTED_INTERSECTION_CRUISE` stage.
- Set the ego vehicle speed limit so that the ego vehicle slows down as it approaches the intersection.
- Set the right of way of the intersection, `reference_line_info.SetJunctionRightOfWay`; the ego vehicle is in the unprotected state.
- Execute Task planning again, `ExecuteTaskOnReferenceLine`.
- CheckClear: collect the sequence of obstacles `wait_for_obstacle_ids` that the ego vehicle needs to wait for.
- Parameter configuration policy: enable_explicit_stop. If True, when the ego vehicle needs to wait, set `stop = true` and build a STOP virtual wall for the `wait_for_obstacle_ids` obstacles. After 5 consecutive frames without a need to wait, `stop = false`.

**CheckClear**: Checks whether the vehicle is currently blocked by obstacles.
- The inputs are `const ReferenceLineInfo& reference_line_info, std::vector<std::string>* wait_for_obstacle_ids`.
- `wait_for_obstacle_ids` is passed by reference, and after the function executes it returns the sequence of blocking obstacles to wait for.


**FinishStage**: The exit function of this stage.
- Exit the `BARE_INTERSECTION_UNPROTECTED_APPROACH` stage and enter the `BARE_INTERSECTION_UNPROTECTED_INTERSECTION_CRUISE` stage.
- Reset the cruise speed limit to `FLAGS_default_cruise_speed`.


#### 2. BARE_INTERSECTION_UNPROTECTED_INTERSECTION_CRUISE
`BareIntersectionUnprotectedStageApproach` : This stage is used for the ego vehicle to pass through the intersection.

**Process**: The main function of this stage. The inputs are the planning initial point `planning_init_point` and `Frame`; the output is the current stage processing status `StageResult`
- ExecuteTaskOnReferenceLine: the inputs are the planning initial point `planning_init_point` and `Frame` information; planning is performed in turn according to the task list configured for this stage.
- CheckDone: checks whether the ego vehicle has passed through the intersection. If so, enter `FinishStage` and end the current Stage. Otherwise, return the status value `StageStatusType::RUNNING`.

**CheckDone**: A member function of the base class `BaseStageCruise`.
- The inputs are: `const Frame& frame, const PlanningContext* context, const bool right_of_way_status`.
- At an unprotected intersection, `right_of_way_status = false`. It checks whether the ego vehicle is still in the intersection.

**FinishStage**: The exit function of this stage.
- Exit the `BARE_INTERSECTION_UNPROTECTED_INTERSECTION_CRUISE` stage and exit the `BareIntersectionUnprotectedScenario` scenario.

## Directory Structure

```shell
modules/planning/scenarios/bare_intersection_unprotected/
├── bare_intersection_unprotected_scenario.cc
├── bare_intersection_unprotected_scenario.h
├── bare_intersection_unprotected_scenario_test.cc
├── BUILD
├── conf
│   ├── pipeline.pb.txt
│   └── scenario_conf.pb.txt
├── cyberfile.xml
├── images
│   └── bare_intersection_scenario.png
├── plugins.xml
├── proto
│   ├── bare_intersection_unprotected_scenario.proto
│   └── BUILD
├── README.md
├── stage_approach.cc
├── stage_approach.h
├── stage_approach_test.cc
├── stage_intersection_cruise.cc
└── stage_intersection_cruise.h
```

## Module

### BareIntersectionUnprotectedScenario plugin

apollo::planning::BareIntersectionUnprotectedScenario


#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| --------------------------------------------------------------------- | ---------------- | ---------------- |
| `modules/planning/scenarios/bare_intersection_unprotected/conf/scenario_conf.pb.txt`| `apollo::planning::ScenarioBareIntersectionUnprotectedConfig` | Scenario configuration file   |
| `modules/planning/scenarios/bare_intersection_unprotected/conf/pipeline.pb.txt`     | `apollo::planning::ScenarioPipeline` | Scenario pipeline file |
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
        name: "BARE_INTERSECTION_UNPROTECTED"
        type: "BareIntersectionUnprotectedScenario"
      }
      ......
    }
  }
  ```
- Edit all `Stage`s of this scenario and the `Task` list corresponding to each `Stage` in `modules/planning/scenarios/bare_intersection_unprotected/conf/pipeline.pb.txt`.
- Custom development is also supported. A newly added `Stage` plugin must be added to `modules/planning/scenarios/bare_intersection_unprotected/plugins.xml`.
  ```shell
  # modules/planning/scenarios/bare_intersection_unprotected/plugins.xml 
  <library path="modules/planning/scenarios/bare_intersection_unprotected/libbare_intersection_unprotected_scenario.so">
      <class type="apollo::planning::BareIntersectionUnprotectedScenario" base_class="apollo::planning::Scenario"></class>
      <class type="apollo::planning::BareIntersectionUnprotectedStageApproach" base_class="apollo::planning::Stage"></class>
      <class type="apollo::planning::BareIntersectionUnprotectedStageIntersectionCruise" base_class="apollo::planning::Stage"></class>
  </library>
  ```

##### Configuring EmergencyStopScenario parameters
- `proto` file configuration: `modules/planning/scenarios/bare_intersection_unprotected/proto/bare_intersection_unprotected_scenario.proto`
- EmergencyStopScenario parameters are configured in: `modules/planning/scenarios/bare_intersection_unprotected/conf/scenario_conf.pb.txt`
