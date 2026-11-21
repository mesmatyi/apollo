planning-scenaios-lane-follow
==============

## Introduction
`LANE FOLLOW`

1) lane_follow_scenario
This function is used to determine whether other_scenario can be transferred into the current LaneFollowScenario.

First, check whether the frame's planning command contains a lane follow command. If not, the function returns false and the transfer is not possible.
Then, check whether the frame's reference line information is empty. If it is empty, return false; the transfer is not possible.
Finally, check whether other_scenario is empty. If it is empty, return true; the transfer is possible.

2) lane_follow_stage

### process
Look for drivable lines in reference_line_info.

This method first checks whether the frame contains any reference line information. If not, it returns a finished StageResult.

Then it traverses all reference lines. For each reference line, it calls the PlanOnReferenceLine method to plan.

If the planning result has no errors, it checks whether this reference line requires a lane change (IsChangeLanePath).
If not, it marks this reference line as drivable and continues with the next reference line.
If so, it checks whether the cost (Cost) of this reference line is less than the cost of not changing lanes (kStraightForwardLineCost). If it is, it marks this reference line as drivable. Otherwise, it marks this reference line as not drivable.

If an error occurs in planning, this reference line is marked as not drivable.

After traversing all reference lines, if a drivable line has been found, it returns a RUNNING StageResult. Otherwise, it returns an ERROR StageResult.

### PlanOnReferenceLine
Generates the cost corresponding to each reference line, for use in later judgments.

Whether reference_line_info changes lanes. If so, add a path cost kStraightForwardLineCost.
Traverse each task in the task list. For each task, print the time difference of executing the task.

If an error occurs during task execution (ret.IsTaskError()), the error information is printed and execution stops.

Then the path and speed information are combined into one trajectory. If this fails, an error status is returned and the error information is printed.

Next, it checks whether there is a target point (destination) on the reference line. If so, it records the s value of this target point (its position on the reference line).

Then, for each obstacle on the reference line, if the obstacle is static and has a stop decision, it checks whether the obstacle's stop point is before the target point. If so, an obstacle cost kReferenceLineStaticObsCost is added.
If trajectory checking is enabled (through the FLAGS variable enable_trajectory_check), the trajectory is checked for validity. If it is invalid, an error status is returned and the error information is printed.

Finally, it sets the trajectory and drivable state of the reference line, and then returns a running status.


## Directory Structure 
```shell
modules/planning/scenarios/lane_follow/
├── BUILD
├── conf
│   └── pipeline.pb.txt
├── cyberfile.xml
├── lane_follow_scenario_test.cc
├── lane_follow_scenario.cc
├── lane_follow_scenario.h
├── lane_follow_stage.cc
├── lane_follow_stage.h
├── plugins.xml
└── README.md

```

## Module

### LaneFollowScenario plugin

apollo::planning::LaneFollowScenario

#### Configuration

| File path                                                          |Type/Structure                            | Description          |
| --------------------------------------------------------------- |----------------------------------- |---------------|
| `modules/planning/scenarios/lane_follow/conf/pipeline.pb.txt`   | apollo::planning::ScenarioPipeline |Scenario pipeline file |

#### Usage
##### Configuring the LaneFollowScenario plugin to load
- Add the `standard_planning_config` configuration to the `modules/planning/planning_component/conf/planning_config.pb.txt` file to use the current scenario plugin.
  ``` shell
  # modules/planning/planning_component/conf/planning_config.pb.txt

  standard_planning_config {
    planner_type: ......
    planner_public_road_config {
      ......
      scenario {
        name: "LANE_FOLLOW"
        type: "LanefollowScenario"
      }
      ......
    }
  }
  ```
- Edit all `Stage`s of this scenario and the `Task` list corresponding to each `Stage` in `modules/planning/scenarios/lane_follow/conf/pipeline.pb.txt`.
- Custom development is also supported. A newly added `Stage` plugin must be added to `modules/planning/scenarios/lane_follow/plugins.xml`.
    ```shell
    # modules/planning/scenarios/lane_follow/plugins.xml
    <library path="modules/planning/scenarios/lane_follow/liblane_follow_scenario.so">
        <class type="apollo::planning::LaneFollowScenario" base_class="apollo::planning::Scenario"></class>
        <class type="apollo::planning::LaneFollowStageApproach" base_class="apollo::planning::Stage"></class>
        <class type="apollo::planning::LaneFollowStageStandby" base_class="apollo::planning::Stage"></class>
    </library>
    ```

##### Configuring LaneFollowScenario parameters
- `proto` file configuration: `modules/planning/scenarios/lane_follow/proto/lane_follow.proto`
- LaneFollowScenario parameters are configured in: `modules/planning/scenarios/lane_follow/conf/scenario_conf.pb.txt`
