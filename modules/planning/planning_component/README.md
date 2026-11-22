# planning

## Introduction

`planning` is the main flow and entry package of the planning module, containing the overall architecture and flow of the planning module. Based on the perceived surrounding environment information, the map, localization and navigation information, and the global path information input from upstream modules, the planning module plans a motion trajectory (containing coordinates, speed, acceleration, jerk, time, and other information) for the autonomous vehicle and then passes this information to the control module.

![](./docs/images/architecture_all.png)

### Introduction to the planning packages

The planning module consists of the following directories:

- **planning_component**: Contains the planning component class, the planning program startup, and the configuration files.
- **planning_interface_base**: Contains the parent-class interface files of several important plugin types in planning (scenario, task, traffic rule, and planner).
- **planning_base**: Contains the basic data structures and algorithm library of planning.
- **planner**: Contains the several planner subclass plugins of the planning module.
- **pnc_map**: The pnc_map class on which reference line generation is based. According to the input planning navigation commands, maps, and other information, it generates reference line data that serves as the route reference for planning's local path planning.
- **scenarios**: The scenario plugins supported by the planning module. Each directory contains an independent scenario plugin package with the definitions of the scenario and stage classes.
- **tasks**: The task plugins supported by the planning module. Each directory contains an independent task plugin package with the definition of the task class.
- **traffic_rules**: The common traffic rule plugins supported by the planning module. Each directory contains an independent traffic rule plugin package, and traffic rules act on all running scenarios.

### Introduction to the planning framework

#### Scenario Mechanism

Starting with Apollo 3.5, the planning module uses a two-layer state machine scenario mechanism. Compared with versions before Apollo 3.0, where every driving use case was treated as a different driving scenario, issues reported in a specific scenario can now be fixed without affecting the work of other scenarios, and the fix does not affect other driving use cases.

![](./docs/images/state_machine.png)

The Top Layer of the two-layer state machine is the Scenario state machine, and the Bottom Layer is the Stage state machine. Scenario switching happens in the Top Layer: the ScenarioManager decides which scenario to switch to based on the surrounding environment and map information, such as the LaneFollow scenario for driving along the road and the PullOver scenario for pulling over. Stage switching happens in the Bottom Layer: if the previous Stage has finished, the next Stage is executed. When all Stages have finished, the current Scenario is considered finished.

Planning currently supports the following scenarios:

- **apollo::planning::LaneFollowScenario**: The lane-following scenario, which is the default autonomous driving scenario. In this scenario the vehicle drives along the routing path. When it encounters obstacles, it, depending on the situation, bypasses them within the current lane or borrows the adjacent lane to bypass them, and changes lanes according to the route information in routing. For traffic signs on the road, such as stop signs, yield signs, crosswalks, and speed bumps, it slows down or stops to yield according to traffic rules.

![](./docs/images/lane_follow_scenario.png)

- **apollo::planning::PullOverScenario**: The pull-over scenario. If the parameter `enable_pull_over_at_destination` is set to `true`, the vehicle automatically switches into `PullOverScenario` when it approaches the destination and completes the pull-over.

![](./docs/images/pull_over_scenario.png)

- **apollo::planning::BareIntersectionUnprotectedScenario**: The unprotected bare intersection scenario. At such an intersection there is neither a stop sign nor a traffic light, and the vehicle switches to this scenario within a certain distance before the intersection.

![](./docs/images/bare_intersection_scenario.png)

- **apollo::planning::TrafficLightProtectedScenario**: The protected traffic light intersection scenario. At such intersections there are explicit traffic light indications for going straight, turning left, and turning right.

![](./docs/images/traffic_light_protected_scenario.png)

- **apollo::planning::TrafficLightUnprotectedLeftTurnScenario**: The unprotected traffic light left turn scenario. In this scenario, when the vehicle turns left at a traffic light intersection, oncoming vehicles may still pass through the intersection, and the vehicle must yield to them. The design of TrafficLightUnprotectedLeftTurnScenario therefore differs from that of the protected traffic light scenario in that, when passing through the intersection, a slow-moving stage (Creep) is added so that the vehicle can observe oncoming vehicles and yield as appropriate.

![](./docs/images/traffic_light_unprotected_left_scenario.png)

- **apollo::planning::TrafficLightUnprotectedRightTurnScenario**: The unprotected traffic light right turn scenario. In this scenario, when the vehicle turns right at a traffic light, vehicles may pass from other directions, so the vehicle must slow down, observe the traffic light status, and turn right only when it is safe.

![](./docs/images/traffic_light_unprotected_right_scenario.png)

- **apollo::planning::StopSignUnprotectedScenario**: The unprotected stop sign intersection scenario. At such an intersection there is only a two-way stop sign, and before passing through the intersection the vehicle must observe the vehicles coming and going and proceed only when the traffic density at the intersection is low.

![](./docs/images/stop_sign_unprotected_scenario.png)

![](./docs/images/stop_sign_unprotected_1.png)

<center>Two-way stop sign intersection</center>

![](./docs/images/stop_sign_unprotected_2.png)

<center>Four-way stop sign intersection</center>

- **apollo::planning::YieldSignScenario**: The scenario of an intersection with a yield sign. When the vehicle encounters a yield sign and there are oncoming vehicles, it must let the oncoming vehicles go first.

![](./docs/images/yield_sign_scenario.png)

- **apollo::planning::ValetParkingScenario**: The valet parking scenario. When the planning input command RoutingResponse contains parking_id information, the vehicle is to be parked in the parking spot corresponding to that parking_id in the map.

![](./docs/images/valet_parking_scenario.png)

- **apollo::planning::EmergencyPullOverScenario**: The emergency pull-over scenario. If the vehicle receives the PadMessage command "PULL_OVER" while driving, it finds a suitable position nearby and stops within the current lane. Compared with stopping directly, this ensures stopping safety during driving.

![](./docs/images/emergency_pull_over_scenario.png)

- **apollo::planning::ParkAndGoScenario**: The command for the vehicle, after pulling over, to start from its current position and drive toward the specified next target. This scenario is suitable for shuttle-bus situations. The scenario uses the Open Space algorithm to ensure that the vehicle can safely pull out of the specified position.

![](./docs/images/park_and_go_scenario.png)

#### Planning module running flow

The running flow of the planning module is shown in the figure below. The entry of the module is PlanningComponent. When prediction information PredictionObstacles is input, its Proc function is triggered to perform trajectory planning.

![](./docs/images/planning_flow.png)

Planning supports two planning modes: OnLanePlanning and NaviPlanning. The former is trajectory planning based on the HD map and is the default planning mode; the latter is relative-map navigation planning, mainly used on highways with simple traffic rules.

Each planning mode can select the Planner to use through the PlannerDispatcher. Currently there are 4 Planners in the planning module:

- **apollo::planning::PublicRoadPlanner**: planner based on the HD map;
- **apollo::planning::NaviPlanner**: planner based on the real-time relative map;
- **apollo::planning::LatticePlanner**: planner based on the lattice algorithm
- **apollo::planning::RTKReplayPlanner**: planner based on a recorded trajectory

The planning module has two main threads. One is the main flow that plans trajectories according to the input environment and vehicle information; the other generates reference line information according to the map and the input global route. The latter is a periodically running thread, and the main-flow planning is carried out on the reference lines it generates.

In the main planning flow, OnLanePlanning->PublicRoadPlanner is used by default for trajectory planning. In PublicRoadPlanner, according to the surrounding environment information, it switches to different scenarios, plans and generates a trajectory, and sends it to the control module.

#### Planning module entry

PlanningComponent is the entry of the planning module. It is a Component triggered by a topic, and its interface function is:

```C
    bool Proc(
        const std::shared_ptr<prediction::PredictionObstacles>& prediction_obstacles,
        const std::shared_ptr<canbus::Chassis>& chassis,
        const std::shared_ptr<localization::LocalizationEstimate>&
            localization_estimate) override;
```

When new PredictionObstacles data is received, the Proc function is triggered. It obtains the latest Chassis vehicle information and LocalizationEstimate vehicle localization data for processing and computes the planning trajectory.

#### Planning initialization

Planning initialization is performed in the PlanningComponent::Init function. Here the PlanningBase object (OnLanePlanning by default), which is the main body of trajectory planning, is created. In addition, subscriber objects for the other input messages of planning and publisher objects for the output messages must be created:

| Member object                      | Type                                                                                                                                | Description                                     |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------- |
| **planning_command_reader\_** | `std::shared_ptr< apollo::cyber::Reader < apollo::planning::PlanningCommand >>`                                                     | Subscription to input navigation commands                         |
| **traffic_light_reader\_**    | `std::shared_ptr< apollo::cyber::Reader < apollo::perception::TrafficLightDetection >>`                                             | Subscription to traffic light messages                           |
| **pad_msg_reader\_**          | `std::shared_ptr< apollo::cyber::Reader < apollo::planning::PadMessage >>`                                                          | Subscription to planning operation command (start, stop) messages  |
| **story_telling_reader\_**    | `std::shared_ptr< apollo::cyber::Reader < apollo::storytelling::Stories >>`                                                         | Subscription to storytelling messages                     |
| **relative_map_reader\_**     | `std::shared_ptr< apollo::cyber::Reader < apollo::relative_map::MapMsg >>`                                                          | Subscription to real-time relative map messages (used by NaviPlanning) |
| **planning_writer\_**         | `std::shared_ptr< apollo::cyber::Writer < apollo::planning::ADCTrajectory >>`                                                       | Publisher of planning output trajectory messages                 |
| **rerouting_client\_**        | `std::shared_ptr< apollo::cyber::Client < apollo::external_command::LaneFollowCommand , apollo::external_command::CommandStatus >>` | Request for rerouting when planning is blocked         |
| **command_status_writer\_**   | `std::shared_ptr< apollo::cyber::Writer < apollo::external_command::CommandStatus >>`                                               | Publisher of planning real-time task status messages             |

#### Scenario configuration

Planning currently mainly uses `OnLanePlanning->PublicRoadPlanner`, and the following introduction also focuses on PublicRoadPlanner. PublicRoadPlanner uses the two-layer state machine scenario mechanism. To specify which scenarios are to be handled at runtime, users can set the `standard_planning_config` field in the configuration file `modules/planning/planning_component/conf/planning_config.pb.txt`. For example, the following configuration supports the two scenarios `LaneFollowScenario` and `ValetParkingScenario`:

```text
standard_planning_config {
  planner_type: PUBLIC_ROAD
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
}
```

The scenarios in the configuration list are ordered by priority from high to low. If it is determined that an earlier scenario can be entered, the later scenarios are no longer checked.

#### Scenario switching

Each time the `apollo::planning::PlanningComponent::Proc` function is called in the main-flow thread, which scenario to enter must be re-determined according to the current context.

Scenario update and switching are performed in `apollo::planning::ScenarioManager::Update`. It traverses all scenarios in the scenario list and calls the overridden function `IsTransferable` of each scenario subclass to determine whether the current environment allows entering that scenario subclass. Because the scenario list is ordered by priority from high to low, when the first scenario that can be entered is found during traversal, the later scenarios need not be checked, and this scenario is used directly as the current scenario for this running cycle.

![](./docs/images/scenario_switch.png)

#### Scenario execution

Scenario execution is also invoked in the `apollo::planning::PlanningComponent::Proc` function. Because each scenario contains multiple `Stage`s and each `Stage` contains multiple `Task`s, executing a scenario means executing the different tasks of the different stages in sequence.

<center><img src="./docs/images/scenario_execute_1.png" width = "50%" height = "50%" alt="Image name" /></center>
<center>Main planning flow</center>

<img src="./docs/images/scenario_execute_2.png" width = "100%" height = "100%" alt="Image name" align=center />
<center>Scenario execution flow</center>

#### traffic rules

Traffic rules are applied by planning before it runs a scenario: according to the different traffic rules, they decide whether the vehicle needs to stop, slow down, or take other actions. Because they take place before the scenario runs, they take effect for all scenarios.

![](./docs/images/traffic_rules_flow.png)

The traffic rules currently supported are:

- **backside_vehicle**: Handling of vehicles behind, deciding whether to ignore rear vehicles
- **crosswalk**: Crosswalk traffic rule, deciding whether to stop according to obstacles near the crosswalk
- **destination**: Handling of approaching the destination, setting a stop wall when approaching the destination
- **keepclear**: Handling of no-stopping zones. The vehicle cannot stop in a no-stopping zone; if there is a blocking obstacle in the zone, the vehicle must stop outside it
- **reference_line_end**: Handling of approaching the end of the reference line, setting a stop wall at the end of the reference line
- **rerouting**: Handling of rerouting when planning is blocked, sending a rerouting request
- **stop_sign**: Handling of the stop sign traffic rule, stopping before the stop sign
- **traffic_light**: Handling of traffic lights: pass when the light is green, otherwise stop
- **yield_sign**: Handling of yield signs: if there is a conflicting vehicle near the sign, the ego vehicle stops to yield

#### reference line

The reference line is the foundation of planning's planning algorithms. According to the vehicle's real-time position, ReferenceLineProvider computes the reference line information within a certain range (several hundred meters) ahead of and behind the vehicle. Compared with the global routing path, the reference line is local route information, but it also carries dynamic information around the vehicle, such as obstacles and traffic lights.

Two important pieces of data related to the reference line:

- **ReferenceLine**: The original path information generated from the global routing path, containing the static relationships between roads on the map, after smoothing.
- **ReferenceLineInfo**: Dynamic information, such as decision information and the ST graph, added on top of `ReferenceLine`. Planning's planning operations are basically carried out on this data structure. It can be understood as ReferenceLine providing trajectory information, while ReferenceLineInfo newly adds decision information on top of ReferenceLine.

The reference line generation flow is shown in the figure below:

<center><img src="./docs/images/reference_line_flow.png" width = "50%" height = "50%" alt="Image name" /></center>

The `CreateRouteSegments` function converts the global routing path within the range near the vehicle into the reference line format; the SmoothRouteSegment function smooths the original reference line.

There are three smoothing methods for the reference line: discrete-point smoothing (default), spiral smoothing, and spline smoothing.

![](./docs/images/reference_line_smoother.png)

- @subpage md_modules_2planning_2planning\_\_base_2docs_2reference\_\_line\_\_smoother\_\_cn "Reference line smoothing algorithms"

### How to extend planning

The planning module framework is shown in the figure below. planning-base contains the main flow and the related external interface classes. The planning module lets users flexibly extend or modify the functionality they need according to their own scenarios, mainly in the following ways: ![](./docs/images/planning_frame.png)

- **Modify configuration parameters**: If the functionality you need is already implemented but the performance in certain scenarios needs adjusting, you can modify the corresponding parameters.
- **Call external interface commands**: If your scenario requires multiple planning tasks to complete one job, or the task status needs to be changed dynamically while a planning task is running, you can orchestrate planning tasks yourself at the business layer according to your business needs and send commands to the planning module. The external interfaces currently supported are:

  | Command name                                                        | Description             |
  | --------------------------------------------------------------- | ---------------- |
  | `apollo::external_command::LaneFollowCommand`                   | Point-to-point driving along the road |
  | `apollo::external_command::ValetParkingCommand`                 | Parking             |
  | `apollo::external_command::ActionCommandType::PULL_OVER`        | Emergency pull-over     |
  | `apollo::external_command::ActionCommandType::STOP`             | Emergency stop         |
  | `apollo::external_command::ActionCommandType::START`            | Resume driving         |
  | `apollo::external_command::ActionCommandType::SWITCH_TO_MANUAL` | Switch to manual mode   |
  | `apollo::external_command::ActionCommandType::SWITCH_TO_AUTO`   | Switch to autonomous mode   |
  | `apollo::external_command::ActionCommandType::VIN_REQ`          | VIN code verification     |

- **Extend plugins**: Including scenario, task, or traffic rule

  Secondary development and extension of planning is provided in the form of plugin development. Before developing planning plugins, you should first understand [plugin-related knowledge and concepts](todo:插件的readme).

  - **Develop a scenario plugin**

    Scenarios can be divided by geographic location. When the ego vehicle's planning actions in a particular area differ from those of the default scenario (Lane Follow), a new scenario can be developed for it to avoid affecting the operation of the default scenario. For example, a traffic light scenario is needed for traffic lights ahead, and a stop sign scenario for a stop sign ahead.

    Scenarios can also be divided by command: when an emergency pull-over command is received, the emergency pull-over scenario is entered; when a parking command is received, the parking scenario is entered. Developing a new Scenario requires inheriting the `apollo::planning::Scenario` base class and implementing the following functions:

    - **Initialization function**, which loads the scenario's pipeline and configuration parameters from the scenario plugin: `apollo::planning::Scenario::Init`.
    - **Switching function**, called by `apollo::planning::ScenarioManager` to determine whether the scenario needs to be entered: `apollo::planning::Scenario::IsTransferable`.
    - **Function for operations on entering the scenario**, called before the scenario is entered for the first time to do some preprocessing and reset variables within the scenario; it need not be overridden if no operation is needed: `apollo::planning::Scenario::Enter`
    - **Function for operations on exiting the scenario**, called when the scenario is switched out, and can be used to clear some global variables; it need not be overridden if no operation is needed: `apollo::planning::Scenario::Exit`

  - **Develop a task plugin**

    When the tasks (Task) in Apollo cannot meet the scenario's needs, a new task plugin must be developed. There are several types of `apollo::planning::Task` base classes in Apollo:

    - apollo::planning::PathGeneration: mainly used to generate paths on the main road, such as the lane borrow path LaneBorrowPath, the pull-over path PullOverPath, and the lane follow path LaneFollowPath.
    - apollo::planning::SpeedOptimizer: mainly used to plan speed profiles on the main road, such as speed planning based on quadratic programming and speed planning based on nonlinear programming.
    - apollo::planning::TrajectoryOptimizer: mainly used to generate trajectories, such as open-space planning `apollo::planning::OpenSpaceTrajectoryProvider`.

    If the above tasks cannot meet the scenario requirements, you can inherit the Task base class to implement the requirement. Developing a new Task subclass requires implementing the following functions:

    - **Initialization function**: before running a task for the first time, `apollo::planning::Stage` calls the task's `Init` function to initialize it. The initialization function mainly initializes the task's member variables and loads configuration parameters: `apollo::planning::Task::Init`
    - **Execution function**: the execution function contains the main running logic of the task: `apollo::planning::Task::Execute`

  - **Develop a traffic rule plugin**

    The traffic rule plugin mainly processes traffic rules before the planning module executes a Scenario. When a new decision logic that takes effect in all scenarios needs to be added, a new traffic rule plugin can be developed.

    A traffic rule plugin inherits from the traffic rule base class, and the traffic_decider in planning_base then generates and calls each plugin. Each time planning performs a planning task, it calls each traffic rule through the traffic_decider, so that the traffic rule plugins take effect.

    Developing a new traffic rule plugin subclass requires inheriting from apollo::planning::TrafficRule and implementing the following functions:

    - **Initialization function**, in which the plugin's configuration parameters are loaded: `apollo::planning::TrafficRule::Init`
    - **Execution function**, which contains the main logic by which the traffic rule takes effect: `apollo::planning::TrafficRule::ApplyRule`

## Directory Structure

```shell

modules/planning/planning_component
├── planning_component
    ├── conf                    // Common (global) parameter configuration files
    ├── dag                     // Module startup files (mainboard)
    ├── docs                    // Documentation
    ├── integration_tests       // Integration unit tests
    ├── launch                  // Module startup files (cyber_launch)
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── navi_planning.cc        // Source code of NaviPlanning, the relative-map navigation mode
    ├── navi_planning.h         // Header file of NaviPlanning, the relative-map navigation mode
    ├── on_lane_planning.cc     // Source code of OnLanePlanning, the HD-map-based navigation mode
    ├── on_lane_planning.h      // Header file of OnLanePlanning, the HD-map-based navigation mode
    ├── planning_base.cc        // Source code of the planning mode parent class
    ├── planning_base.h         // Header file of the planning mode parent class
    ├── planning_component.cc   // Source code of the planning component external interface class
    ├── planning_component.h    // Header file of the planning component external interface class
    └── README.md               // Documentation
```

## Module

### PlanningComponent

apollo::planning::PlanningComponent

#### Input

The Planning module needs external environment information and the vehicle's own information for trajectory planning. The following are planning's external inputs:

| Channel name                         | Type                                         | <div style="width: 300pt">Description</div>                                                                                                               |
| ---------------------------------- | -------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/prediction`               | `apollo::prediction::PredictionObstacles`    | Obstacle prediction information; the channel name can be modified in the `modules/planning/planning_component/dag/planning.dag` startup file                                                     |
| `/apollo/perception/traffic_light` | `apollo::perception::TrafficLight`           | Traffic light perception information output by the perception module, including the lit color and id of the traffic light                                                                                 |
| `/apollo/localization/pose`        | `apollo::localization::LocalizationEstimate` | Localization information; the channel name can be modified in the `modules/planning/planning_component/dag/planning.dag` configuration file                                                           |
| `/apollo/canbus/chassis`           | `apollo::canbus::Chassis`                    | Vehicle chassis information output by the canbus module, including chassis speed, throttle, brake, gear, lights, and other states; the channel name can be modified in the `modules/planning/planning_component/dag/planning.dag` configuration file |

In addition, the planning module needs externally input navigation command information. Users first send navigation command requests to external_command, which processes these commands and forwards them to the planning module. The navigation commands that users can send are introduced below:

| Channel name                               | Type                                            | <div style="width: 300pt">Description</div>                                           |
| ---------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------------------------ |
| `/apollo/external_command/lane_follow`   | `apollo::external_command::LaneFollowCommand`   | HD-map-based navigation command: given the position or heading of the end point, navigate from the current vehicle position to the target end position |
| `/apollo/external_command/valet_parking` | `apollo::external_command::ValetParkingCommand` | Navigate from the current position to a parking spot and park                                                   |
| `/apollo/external_command/action`        | `apollo::external_command::ActionCommand`       | Process operation commands sent by the HMI                                                          |

#### Output

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning`                     | `apollo::planning::ADCTrajectory`             | Outputs the planned trajectory, including trajectory points, speed, time, and other information       |
| `/apollo/planning/command_status`      | `apollo::external_command::CommandStatus`     | Execution status of navigation commands                               |
| `/apollo/external_command/lane_follow` | `apollo::external_command::LaneFollowCommand` | Sends a rerouting request when the road is blocked and a lane change fails by timeout |

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |
| `modules/planning/planning_component/conf/public_road_planner_config.pb.txt`       | `apollo::planning::PlannerOpenSpaceConfig`      | Configuration file of PublicRoadPlanner           |
| `modules/planning/planning_component/conf/traffic_rule_config.pb.txt`             | `apollo::planning::TrafficRulesPipeline`        | Configuration file of the list of supported traffic rules    |
| `modules/planning/planning_component/conf/discrete_points_smoother_config.pb.txt` | `apollo::planning::ReferenceLineSmootherConfig` | Configuration file for when the reference line uses discrete-point smoothing     |
| `modules/planning/planning_component/conf/qp_spline_smoother_config.pb.txt`       | `apollo::planning::ReferenceLineSmootherConfig` | Configuration file for when the reference line uses quintic polynomial smoothing |
| `modules/planning/planning_component/conf/spiral_smoother_config.pb.txt`          | `apollo::planning::ReferenceLineSmootherConfig` | Configuration file for when the reference line uses quintic spiral smoothing |
| `modules/planning/planning_component/conf/planner_open_space_config.pb.txt`       | `apollo::planning::PlannerOpenSpaceConfig`      | Configuration file of the open-space planning algorithm           |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage

##### Starting with mainboard

```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```

##### Starting with cyber_launch

```shell
cyber_launch start modules/planning/planning_component/launch/planning.launch
```
