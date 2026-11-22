# Development by Configuration Parameters

## Global Parameters

Global parameters are defined based on gflag, in the file `modules/planning/planning_base/common/planning_gflags.cc`.
All global parameters of the planning module can be configured in `modules/planning/planning_base/conf/planning.conf` in the apollo directory. For example, to set the cruise speed to 10 m/s, modify the parameter in `planning.conf` as follows:

```bash
--default_cruise_speed=10
```

Note that there should be no extra spaces in each parameter line. In code, the value of the corresponding parameter can be obtained in the form `FLAGS_XXX`; for example, to get the cruise speed in code, use the `FLAGS_default_cruise_speed` variable.
`gflag` parameters not defined in `planning.conf` use the default values defined in `planning_gflags.cc`. If you need to modify these parameters, simply add them to the end of `planning.conf`.

## Scenario Plugin Configuration

Scenario plugin configuration defines the planning scenario plugins you want to enable. It is configured in `planner_public_road_config` of `modules/planning/planning_base/conf/planning_config.pb.txt`, and the configured scenario plugins are dynamically loaded into the planning module process when the planning module runs. The following example enables only the valet parking and lane follow scenarios:

```bash
planner_public_road_config {
    scenario {
      name: "VALET_PARKING"
      type: "ValetParkingScenario"
    }
    scenario {
      name: "LANE_FOLLOW"
      type: "LaneFollowScenario"
    }
 }
```

Here, name is the name of the scenario, usually in uppercase with underscores, and type is the class name of the scenario; both are strings. All scenario plugins included in Apollo can be viewed in the `modules/planning/scenarios` directory.

## Scenario Configuration

### Scenario Pipeline Configuration

Each scenario has a scenario pipeline configuration file, which configures the stage plugins loaded by the scenario and the task plugins that each stage needs to load.
The scenario pipeline configuration file is `pipeline.pb.txt` in the scenario's conf directory; for example, the pipeline configuration file of the unprotected left turn scenario is at `modules/planning/scenarios/traffic_light_unprotected_left_turn/pipeline.pb.txt`:

```bash
stage: {
  name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_APPROACH"
  type: "TrafficLightUnprotectedLeftTurnStageApproach"
  enabled: true
  task {
    name: "LANE_FOLLOW_PATH"
    type: "LaneFollowPath"
  }
  task {
    name: "LANE_BORROW_PATH"
    type: "LaneBorrowPath"
  }

}
stage: {
  name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_CREEP"
  type: "TrafficLightUnprotectedLeftTurnStageCreep"
  enabled: true
  task {
    name: "LANE_FOLLOW_PATH"
    type: "LaneFollowPath"
  }
  ...
}
stage: {
  name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_INTERSECTION_CRUISE"
  type: "TrafficLightUnprotectedLeftTurnStageIntersectionCruise"
  enabled: true
    task {
    name: "LANE_FOLLOW_PATH"
    type: "LaneFollowPath"
  }

  ...
}
```

As shown above, 3 stages are configured for the unprotected left turn scenario: approaching the intersection, creeping to observe the intersection, and crossing the intersection. Each stage is configured with the tasks it needs to execute. You can add or remove scenario functions by configuring the task pipeline; for example, if you do not want lane borrowing while approaching the intersection, you can delete the configuration of the `LANE_BORROW_PATH` task.

### Scenario Parameter Configuration

The scenario parameter configuration file is `scenario_conf.pb.txt` in the scenario directory; for example, the configuration file of the unprotected left turn scenario is `modules/planning/scenarios/traffic_light_unprotected_left_turn/conf/scenario_conf.pb.txt`.

The configuration parameters in a scenario apply to the scenario and the stages it contains. For example, the unprotected left turn scenario parameters:

```bash
start_traffic_light_scenario_distance: 30.0
approach_cruise_speed: 2.78
max_valid_stop_distance: 2.0
creep_timeout_sec: 10.0
max_adc_speed_before_creep: 5.56
creep_stage_config {
    min_boundary_t: 6.0
    ignore_max_st_min_t: 0.1
    ignore_min_st_min_s: 15.0
}
```

Adjusting `start_traffic_light_scenario_distance` changes the distance at which this scenario is entered, and adjusting `approach_cruise_speed` changes the cruise speed in the intersection approach stage.

### Scenario Task Configuration

A scenario can reuse all tasks in the task library. If the task parameters in this scenario conflict with the default task parameters, you can set task parameters specifically for this scenario. Scenario task parameters are saved in the stage subdirectory of the scenario's conf configuration directory. If the default task parameters are fully reused, no scenario task configuration file needs to be added.

For example, in the emergency pull over scenario `emergency_pull_over`, the pull over stage `EMERGENCY_PULL_OVER_APPROACH` needs to use `pull_over_path` to plan the pull over path, but its parameters differ from the default pull over task parameters. You need to add the scenario task configuration parameters `pull_over_path.pb.txt` for `pull_over_path` and place it in the configuration directory of the scenario stage: `modules/planning/scenarios/emergency_pull_over/conf/emergency_pull_over_approach/pull_over_path.pb.txt`.

### Task Default Configuration Parameters

Scenario task configuration parameters take effect only within the corresponding scenario, while modifying the task's default parameters takes effect for all scenarios. The configuration parameters of each task are saved in `default_conf.pb.txt` in the task's `conf` directory. For example, the default configuration parameters of the pull over path planning task are at:
`modules/planning/tasks/pull_over_path/conf/default_conf.pb.txt`.

## Traffic Rule Plugin Configuration

Traffic rule plugin configuration configures the traffic rule plugins enabled by planning. All business logic of traffic rule plugins takes effect in all scenarios, and you can enable or disable the corresponding function by adding or removing plugin configurations. Its configuration file is located at `modules/planning/planning_base/conf/traffic_rule_config.pb.txt`.

```bash
rule {
  name: "BACKSIDE_VEHICLE"
  type: "BacksideVehicle"
}
rule {
  name: "CROSSWALK"
  type: "Crosswalk"
}
...
```

Here, name is the name of the traffic rule plugin and type is the class name of the traffic rule plugin. All traffic rule plugins can be viewed in the `modules/planning/traffic_rules` directory.

Each traffic rule also has its own traffic rule parameters, located in the `conf` directory of the traffic rule plugin. For example, the crosswalk rule parameters are located at `modules/planning/traffic_rules/crosswalk/conf/default_conf.pb.txt`.

```bash
stop_distance: 1.0
max_stop_deceleration: 4.0
min_pass_s_distance: 1.0
expand_s_distance: 2.0
stop_strict_l_distance: 4.0
stop_loose_l_distance: 5.0
start_watch_timer_distance:10
stop_timeout: 10.0
```

If you want to change the distance at which the vehicle stops before a crosswalk, modify the `stop_distance` parameter.
