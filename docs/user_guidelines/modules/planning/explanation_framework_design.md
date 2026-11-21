# Framework Design

![image (7).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%287%29_c99a812.png)

Starting from Apollo 3.5, the Planning module uses a two-layer state machine scenario mechanism. Compared with versions before Apollo 3.0, each driving use case is treated as a different driving scenario, so a problem reported in a specific scenario can now be fixed without affecting the work of other scenarios; fixing the problem does not affect other driving use cases.

In the two-layer state machine, the Top Layer is the Scenario state machine and the Bottom Layer is the Stage state machine.

Scenario switching takes place in the Top Layer. Based on the surrounding environment and map information, the ScenarioManager decides which scenario to switch to, such as the LaneFollow scenario of driving along the road or the PullOver scenario of pulling over to the roadside.

Stage switching takes place in the Bottom Layer. If the previous Stage has finished, the next Stage continues to execute. If all Stages have finished, the current Scenario is considered finished as well.

![image (8).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%288%29_ff1f212.png)

## Input and Output

### Input

The Planning module needs external environment information and the vehicle's own information to plan trajectories. The following is the external input information of planning:

<table><thead><tr><th>Information Category</th><th>Information</th><th>Description</th><th>Data Type</th><th>topic name</th></tr></thead><tbody><tr><td rowspan="2">External environment information</td><td>Obstacle prediction information</td><td>Obstacle information output by the perception module; after processing by prediction, information such as velocity, acceleration, and predicted trajectories is added.</td><td>prediction::PredictionObstacles</td><td>/apollo/prediction</td></tr><tr><td>Traffic light perception information</td><td>Traffic light perception information output by the perception module, including the color of the lit traffic light, id, and other information.</td><td>perception::TrafficLight</td><td>/apollo/perception/traffic_light</td></tr><tr><td rowspan="2">Vehicle information</td><td>Vehicle localization</td><td>Vehicle pose information output by localization.</td><td>localization::LocalizationEstimate</td><td>/apollo/localization/pose</td></tr><tr><td>Chassis status</td><td>Vehicle chassis information output by the canbus module, including chassis speed, throttle, brake, gear, lights, and other states.</td><td>canbus::Chassis</td><td>/apollo/canbus/chassis</td></tr></tbody></table>

In addition, the planning module needs navigation command information from external input. The user first sends a navigation command request to external_command, which then processes these commands and forwards them to the planning module. The navigation commands that users can send are described below:

<table><thead><tr><th>Information Category</th><th>Information</th><th>Description</th><th>Data Type</th><th>topic name</th></tr></thead><tbody><tr><td rowspan="3"><br>External commands</td><td>Lane follow command</td><td>A command for navigation based on the HD map: given the position or heading of the destination, navigate from the current vehicle position to the target destination position.</td><td>external_command::LaneFollowCommand</td><td>/apollo/external_command/lane_follow</td></tr><tr><td>Parking command</td><td>Navigate from the current position and park in a parking space.</td><td>external_command::ValetParkingCommand</td><td>/apollo/external_command/valet_parking</td></tr><tr><td>Process operation command</td><td>Process operation commands sent by the HMI, including emergency pull over (PULL_OVER), emergency stop (STOP), and continue driving (CRUISE); currently only the PULL_OVER and STOP commands are responded to.</td><td>planning::ActionCommand</td><td>/apollo/external_command/action</td></tr></tbody></table>

### Output

<table><thead><tr><th>Information</th><th>Description</th><th>Data Type</th><th>topic name</th></tr></thead><tbody><tr><td>Planned trajectory</td><td>Outputs the planned trajectory, including trajectory points, speed, time, and other information.</td><td>planning::ADCTrajectory</td><td>/apollo/planning</td></tr><tr><td>Navigation status</td><td>Execution status of the navigation command.</td><td>external_command::CommandStatus</td><td>/apollo/planning/command_status</td></tr><tr><td>Re-routing request</td><td>Sends a re-routing request when the road is blocked or lane changing fails by timeout.</td><td>external_command::LaneFollowCommand</td><td>/apollo/external_command/lane_follow</td></tr></tbody></table>

## Design Overview

### Planning Module Entry

#### Module Registration

The entry of the Planning module consists of the two files `planning_component.h` and `planning_component.cc`, which implement the following:

modules/planning/planning_base/planning_component.h:

```bash
// Subscribe to and publish messages
  std::shared_ptr<cyber::Reader<perception::TrafficLightDetection>>
      traffic_light_reader_;
  std::shared_ptr<
      apollo::cyber::Client<apollo::external_command::LaneFollowCommand,
                            apollo::external_command::CommandStatus>>
      rerouting_client_;
  std::shared_ptr<cyber::Reader<planning::PadMessage>> pad_msg_reader_;
  std::shared_ptr<cyber::Reader<relative_map::MapMsg>> relative_map_reader_;
  std::shared_ptr<cyber::Reader<storytelling::Stories>> story_telling_reader_;
  std::shared_ptr<cyber::Reader<PlanningCommand>> planning_command_reader_;

  std::shared_ptr<cyber::Writer<ADCTrajectory>> planning_writer_;
  std::shared_ptr<cyber::Writer<routing::RoutingRequest>> rerouting_writer_;
  std::shared_ptr<cyber::Writer<PlanningLearningData>>
      planning_learning_data_writer_;
  std::shared_ptr<cyber::Writer<external_command::CommandStatus>>
      command_status_writer_;

// Register the module in Cyber
CYBER_REGISTER_COMPONENT(PlanningComponent)
```

#### Module Initialization

Besides registering the module and subscribing to and publishing messages, the planning module implements 2 main functions, `init` and `proc`.
Init implements the initialization of the module:

modules/planning/planning_base/planning_component.cc:

```bash
if (FLAGS_use_navigation_mode) {
    planning_base_ = std::make_unique<NaviPlanning>(injector_);
} else {
    planning_base_ = std::make_unique<OnLanePlanning>(injector_);
}
```

The code above registers 2 kinds of Planning, and the planning module chooses a different Planning implementation according to the configuration. `FLAGS_use_navigation_mode` is in the `conf` directory of the Planning module; since it defaults to false, the default Planning implementation is `OnLanePlanning`. The difference between the 2 kinds of Planning is described below.

- NaviPlanning - mainly used for navigation planning on highways,
- OnLanePlanning - the main application scenario is autonomous driving on open roads.

![image (12).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2812%29_5477fb4.png)

As can be seen, `NaviPlanning` and `OnLanePlanning` both inherit from the same base class, and in PlanningComponent a specific implementation is selected through configuration and registered.

Init then implements the concrete message publishing and subscription. Let us just look at one example:

modules/planning/planning_base/planning_component.cc:

```bash
// Subscribe to the navigation command messages input to planning
planning_command_reader_ = node_->CreateReader<PlanningCommand>(
      config_.topic_config().planning_command_topic(),
      [this](const std::shared_ptr<PlanningCommand>& planning_command) {
        AINFO << "Received planning data: run planning callback."
              << planning_command->header().DebugString();
        std::lock_guard<std::mutex> lock(mutex_);
        planning_command_.CopyFrom(*planning_command);
      });
// Subscribe to traffic light messages
traffic_light_reader_ = node_->CreateReader<TrafficLightDetection>(
      config_.topic_config().traffic_light_detection_topic(),
      [this](const std::shared_ptr<TrafficLightDetection>& traffic_light) {
        ADEBUG << "Received traffic light data: run traffic light callback.";
        std::lock_guard<std::mutex> lock(mutex_);
        traffic_light_.CopyFrom(*traffic_light);
  });
// Subscribe to planning process intervention messages
pad_msg_reader_ = node_->CreateReader<PadMessage>(
      config_.topic_config().planning_pad_topic(),
      [this](const std::shared_ptr<PadMessage>& pad_msg) {
        ADEBUG << "Received pad data: run pad callback.";
        std::lock_guard<std::mutex> lock(mutex_);
        pad_msg_.CopyFrom(*pad_msg);
  });
// Create the publisher for the trajectory output by planning
planning_writer_ = node_->CreateWriter<ADCTrajectory>(
      config_.topic_config().planning_trajectory_topic());
// Client for re-planning when the road is blocked
rerouting_client_ =
      node_->CreateClient<apollo::external_command::LaneFollowCommand,
                      external_command::CommandStatus>(
      config_.topic_config().routing_request_topic());
// Create the publisher for the real-time execution status of planning navigation commands
command_status_writer_ = node_->CreateWriter<external_command::CommandStatus>(
      FLAGS_planning_command_status);
```

With this, the initialization of the Planning module is complete.

#### Module Execution

Proc mainly checks the data, executes the registered Planning, and generates and publishes the route.

modules/planning/planning_base/planning_component.cc:

```bash
bool PlanningComponent::Proc(...) {
  // 1. Check whether the route needs to be re-planned.
  CheckRerouting();
﻿
  // 2. Put the data into local_view_ and check the input data.
  ...

  // 3. Execute the registered Planning and generate the route.
  planning_base_->RunOnce(local_view_, &adc_trajectory_pb);
﻿
  // 4. Publish the message
  planning_writer_->Write(std::make_shared<ADCTrajectory>(adc_trajectory_pb));
}
```

That completes the analysis of `PlanningComponent`. Since the default Planning is OnLanePlanning for open roads, we mainly analyze this Planning next.

### OnLanePlanning

Each time, Planning executes with the following 2 pieces of information as input:

- Planning context information,
- The Frame struct (vehicle information, position information, and all other information needed for planning, in `/planning/common/frame.h`).

modules/planning/planning_base/common/frame.h:

```bash
uint32_t sequence_num_ = 0;
LocalView local_view_;
const hdmap::HDMap *hdmap_ = nullptr;
common::TrajectoryPoint planning_start_point_;
common::VehicleState vehicle_state_;
std::list<ReferenceLineInfo> reference_line_info_;

bool is_near_destination_ = false;

// Information about the reference line finally chosen for the vehicle to drive on
const ReferenceLineInfo *drive_reference_line_info_ = nullptr;

ThreadSafeIndexedObstacles obstacles_;

std::unordered_map<std::string, const perception::TrafficLight *> traffic_lights_;

// Trajectory planned and published in the current frame
ADCTrajectory current_frame_planned_trajectory_;
// Path planned in the current frame
DiscretizedPath current_frame_planned_path_;

const ReferenceLineProvider *reference_line_provider_ = nullptr;

OpenSpaceInfo open_space_info_;

std::vector<routing::LaneWaypoint> future_route_waypoints_;

common::monitor::MonitorLogBuffer monitor_logger_buffer_;
```

#### Initialization

The initialization logic of OnLanePlanning is in Init. It mainly allocates the specific Planner and starts the reference line provider (`reference_line_provider_`). The code analysis is as follows:

modules/planning/planning_base/on_lane_planning.cc:

```bash
Status OnLanePlanning::Init(const PlanningConfig& config) {
  ...

  // Start the reference line provider. It starts another thread that runs a periodic task, providing a reference line every 50 ms.
  reference_line_provider_ = std::make_unique<ReferenceLineProvider>(
      injector_->vehicle_state(), reference_line_config);
  reference_line_provider_->Start();

  // Allocate the specific Planner for Planning.
  planner_ = planner_dispatcher_->DispatchPlanner(config_, injector_);
  ...
}
```

As can be seen, `DispatchPlanner` is specified when `OnLanePlanning` is instantiated.

modules/planning/planning_base/on_lane_planning.h:

```bash
class OnLanePlanning : public PlanningBase {
 public:
  explicit OnLanePlanning(const std::shared_ptr<DependencyInjector>& injector)
      : PlanningBase(injector) {
    planner_dispatcher_ = std::make_unique<OnLanePlannerDispatcher>();
  }
```

Looking at the concrete implementation of `OnLanePlannerDispatcher`, it also selects the specific `Planner` according to the configuration, with the `PUBLIC_ROAD` planner as the default:

modules/planning/planning_base/planner/on_lane_planner_dispatcher.cc:

```bash
std::unique_ptr<Planner> OnLanePlannerDispatcher::DispatchPlanner(
    const PlanningConfig& planning_config,
    const std::shared_ptr<DependencyInjector>& injector) {
  return planner_factory_.CreateObject(
      planning_config.standard_planning_config().planner_type(0), injector);
}
```

#### Event Triggering

The main logic of OnLanePlanning is in `RunOnce()`. Before Apollo 3.5 it was triggered by a timer; since 3.5 it is event-triggered, that is, execution is triggered once the corresponding message is received. The benefit of this is improved real-time responsiveness.

modules/planning/planning_base/on_lane_planning.cc:

```bash
void OnLanePlanning::RunOnce(const LocalView& local_view,
                             ADCTrajectory* const ptr_trajectory_pb) {

  // Initialize the Frame
  status = InitFrame(frame_num, stitching_trajectory.back(), vehicle_state);
  ...

  // Check compliance with traffic rules
  for (auto& ref_line_info : *frame_->mutable_reference_line_info()) {
    auto traffic_status =
        traffic_decider_.Execute(frame_.get(), &ref_line_info);
    if (!traffic_status.ok() || !ref_line_info.IsDrivable()) {
      ref_line_info.SetDrivable(false);
      AWARN << "Reference line " << ref_line_info.Lanes().Id()
            << " traffic decider failed";
    }
  }

  // Execute the plan
  status = Plan(start_timestamp, stitching_trajectory, ptr_trajectory_pb);

  ...
}


Status OnLanePlanning::Plan(
    const double current_time_stamp,
    const std::vector<TrajectoryPoint>& stitching_trajectory,
    ADCTrajectory* const ptr_trajectory_pb) {

  ...

  // Call the specific (PUBLIC_ROAD) Planner to execute
  auto status = planner_->Plan(stitching_trajectory.back(), frame_.get(),
                               ptr_trajectory_pb);
  ...
}

```

The above is the execution process of `OnLanePlanning`: first, the Planner dispatcher selects the specific planner according to the configuration, then the Frame is initialized, and the (`PUBLIC_ROAD`) planner executes the `Plan` method according to the input frame.

### Planner

Let us first look at the Planner directory structure. A total of 4 kinds of Planner are implemented:

```bash
.
├── BUILD
├── navi_planner_dispatcher.cc
├── navi_planner_dispatcher.h
├── navi_planner_dispatcher_test.cc
├── on_lane_planner_dispatcher.cc
├── on_lane_planner_dispatcher.h
├── on_lane_planner_dispatcher_test.cc
├── planner_dispatcher.cc
├── planner_dispatcher.h
├── planner.h
├── lattice           // lattice planner
├── navi              // navi planner
├── public_road       // public road planner
└── rtk               // rtk planner
```

As can be seen, the Planner directory implements the Planner dispatchers and the specific Planners. We will introduce the dispatchers later with a flow chart; here we first introduce the 4 different Planners.

- rtk - plans the driving route according to a recorded trajectory,
- public_road - the trajectory planner for open roads,
- lattice - a trajectory planner based on the lattice algorithm,
- navi - a planner based on a real-time relative map.

#### Planner Scenario Registration

The overall flow of the Planning module is as follows:

![image (13).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2813%29_3b05ca5.png)

1. PlanningComponent is registered in cyber,
2. Planning is selected,
3. The Planner is dispatched according to the different Dispatcher.

Below we mainly introduce `PublicRoadPlanner`, whose main implementation is again in Init and Plan. Init mainly registers the scenarios supported by the planner.

modules/planning/planning_base/planner/public_road/public_road_planner.cc:

```bash
Status PublicRoadPlanner::Init(const PlanningConfig& config) {
  config_ = config;
  // Register different scenarios according to the configuration
  scenario_manager_.Init(config);
  return Status::OK();
}

```

The scenario configuration supported by `PublicRoadPlanner` is as follows:

modules/planning/planning_base/conf/planning_config.pb.txt:

```bash

standard_planning_config {
  planner_type: PUBLIC_ROAD
  planner_public_road_config {
    scenario {
      name: "EMERGENCY_PULL_OVER"
      type: "EmergencyPullOverScenario"
    }
    scenario {
      name: "EMERGENCY_STOP"
      type: "EmergencyStopScenario"
    }
    scenario {
      name: "VALET_PARKING"
      type: "ValetParkingScenario"
    }
    scenario {
      name: "BARE_INTERSECTION_UNPROTECTED"
      type: "BareIntersectionUnprotectedScenario"
    }
    scenario {
      name: "STOP_SIGN_UNPROTECTED"
      type: "StopSignUnprotectedScenario"
    }
    scenario {
      name: "YIELD_SIGN"
      type: "YieldSignScenario"
    }
    scenario {
      name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN"
      type: "TrafficLightUnprotectedLeftTurnScenario"
    }
    scenario {
      name: "TRAFFIC_LIGHT_UNPROTECTED_RIGHT_TURN"
      type: "TrafficLightUnprotectedRightTurnScenario"
    }
    scenario {
      name: "TRAFFIC_LIGHT_PROTECTED"
      type: "TrafficLightProtectedScenario"
    }
    scenario {
      name: "PULL_OVER"
      type: "PullOverScenario"
    }
    scenario {
      name: "PARK_AND_GO"
      type: "ParkAndGoScenario"
    }
    scenario {
      name: "LANE_FOLLOW"
      type: "LaneFollowScenario"
    }
  }
}
```

#### Running Scenarios

Running scenarios takes place in the Plan function:

modules/planning/planning_base/planner/public_road/public_road_planner.cc:

```bash

Status PublicRoadPlanner::Plan(const TrajectoryPoint& planning_start_point,
                               Frame* frame,
                               ADCTrajectory* ptr_computed_trajectory) {
  scenario_manager_.Update(planning_start_point, frame);
  scenario_ = scenario_manager_.mutable_scenario();
  if (!scenario_) {
    return Status(apollo::common::ErrorCode::PLANNING_ERROR,
                  "Unknown Scenario");
  }
  auto result = scenario_->Process(planning_start_point, frame);

  if (FLAGS_enable_record_debug) {
    auto scenario_debug = ptr_computed_trajectory->mutable_debug()
                              ->mutable_planning_data()
                              ->mutable_scenario();
    scenario_debug->set_scenario_type(scenario_->Name());
    scenario_debug->set_stage_type(scenario_->GetStage());
    scenario_debug->set_msg(scenario_->GetMsg());
  }

  if (result.GetScenarioStatus() == ScenarioStatusType::STATUS_DONE) {
    // only updates scenario manager when previous scenario's status is
    // STATUS_DONE
    scenario_manager_.Update(planning_start_point, frame);
  } else if (result.GetScenarioStatus() == ScenarioStatusType::STATUS_UNKNOWN) {
    return Status(common::PLANNING_ERROR,
                  result.GetTaskStatus().error_message());
  }
  return Status(common::OK, result.GetTaskStatus().error_message());
}
```

As can be seen, the `Planner` module converts the concrete planning into a series of scenarios. Before each planning execution, it first determines and updates the current scenario, and then executes for that specific scenario.

### Scenario

The parent class of scenario is declared in `modules/planning/planning_base/scenario_base/scenario.h`. Each concrete scenario inherits from the parent class Scenario and is a separate plugin package. The plugin packages of scenario subclasses are under the `modules/planning/scenarios` directory, and the scenarios currently supported are as follows:

modules/planning/scenarios:

```bash
.
├── bare_intersection_unprotected          // unprotected bare intersection scenario
├── emergency_pull_over                    // emergency pull over scenario
├── emergency_stop                         // emergency stop scenario
├── lane_follow                            // lane follow scenario
├── park_and_go                            // scenario of driving along the lane after pulling over
├── pull_over                              // destination pull over scenario
├── stop_sign_unprotected                  // unprotected stop sign scenario
├── traffic_light_protected                // protected traffic light scenario
├── traffic_light_unprotected_left_turn    // unprotected left turn scenario
├── traffic_light_unprotected_right_turn   // unprotected right turn scenario
├── valet_parking                          // valet parking scenario
├── yield_sign                             // yield sign scenario
```

What needs to be understood is how scenarios are switched and how each scenario is executed. The commonly used scenario is `lane_follow`.

#### Scenario Switching

Scenario switching is implemented in `scenario_manager.cc`, which implements scenario registration, scenario creation, and scenario update.

modules/planning/planning_base/scenario_base/scenario_manager.cc:

```bash
bool ScenarioManager::Init(const PlanningConfig& planning_config) {
  if (init_) {
    return true;
  }
  if (!planning_config.has_standard_planning_config()) {
    AERROR << "lost standard_planning_config" << planning_config.DebugString();
    return false;
  }
  if (!planning_config.standard_planning_config()
           .has_planner_public_road_config()) {
    AERROR << "lost planner_public_road_config"
           << planning_config.DebugString();
    return false;
  }
  const auto& public_road_config =
      planning_config.standard_planning_config().planner_public_road_config();
  // Read the list of supported scenarios from the configuration file and create the corresponding scenarios
  for (int i = 0; i < public_road_config.scenario_size(); i++) {
    auto scenario = PluginManager::Instance()->CreateInstance<Scenario>(
        ConfigUtil::GetFullPlanningClassName(
            public_road_config.scenario(i).type()));
    ACHECK(scenario->Init(injector_, public_road_config.scenario(i).name()))
        << "Can not init scenario" << public_road_config.scenario(i).name();
    scenario_list_.push_back(scenario);
    if (public_road_config.scenario(i).name() == "LANE_FOLLOW") {
      default_scenario_type_ = scenario;
    }
  }
  AINFO << "Load scenario list:" << public_road_config.DebugString();
  // Set the current scenario
  current_scenario_ = default_scenario_type_;
  init_ = true;
  return true;
}

```

In the `PublicRoadPlanner::Plan` function, scenario switching is performed by calling the `ScenarioManager::Update` function:

modules/planning/planning_base/scenario_base/scenario_manager.cc:

```bash
void ScenarioManager::Update(const common::TrajectoryPoint& ego_point,
                             Frame* frame) {
  CHECK_NOTNULL(frame);
  for (auto scenario : scenario_list_) {
    if (current_scenario_.get() == scenario.get() &&
        current_scenario_->GetStatus() ==
            ScenarioStatusType::STATUS_PROCESSING) {
      // The previous scenario has higher priority
      return;
    }
    // Each concrete scenario subclass implements IsTransferable, which decides based on the current environment whether it is possible to switch to its own scenario;
    // if it can switch, the remaining scenarios are no longer traversed
    if (scenario->IsTransferable(current_scenario_.get(), *frame)) {
      current_scenario_->Exit(frame);
      AINFO << "switch scenario from" << current_scenario_->Name() << " to "
            << scenario->Name();
      current_scenario_ = scenario;
      current_scenario_->Reset();
      current_scenario_->Enter(frame);
      return;
    }
  }
}
```

For the scenario state transitions, refer to the figure below:

![image (14).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2814%29_818c1ef.png)

#### Scenario Execution

Scenario execution is in `scenario.cc` and the corresponding scenario directory. In fact, each scenario is divided into one or more stages, and each stage is composed of different tasks. Executing a scenario means sequentially executing the different tasks of the different stages.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_d7dc794.png)

Taking TrafficLightProtectedScenario as an example, its pipeline is set in the configuration file `modules/planning/scenarios/traffic_light_protected/conf/pipeline.pb.txt` under the plugin package:

modules/planning/scenarios/traffic_light_protected/conf/pipeline.pb.txt:

```bash
stage: {
  name: "TRAFFIC_LIGHT_PROTECTED_APPROACH"
  type: "TrafficLightProtectedStageApproach"
  enabled: true
  task {
    name: "LANE_FOLLOW_PATH"
    type: "LaneFollowPath"
  }
  task {
    name: "LANE_BORROW_PATH"
    type: "LaneBorrowPath"
  }
  task {
    name: "FALLBACK_PATH"
    type: "FallbackPath"
  }
  task {
    name: "PATH_DECIDER"
    type: "PathDecider"
  }
  task {
    name: "RULE_BASED_STOP_DECIDER"
    type: "RuleBasedStopDecider"
  }
  task {
    name: "ST_BOUNDS_DECIDER"
    type: "STBoundsDecider"
  }
  task {
    name: "SPEED_BOUNDS_PRIORI_DECIDER"
    type: "SpeedBoundsDecider"
  }
  task {
    name: "SPEED_HEURISTIC_OPTIMIZER"
    type: "PathTimeHeuristicOptimizer"
  }
  task {
    name: "SPEED_DECIDER"
    type: "SpeedDecider"
  }
  task {
    name: "SPEED_BOUNDS_FINAL_DECIDER"
    type: "SpeedBoundsDecider"
  }
  task {
    name: "PIECEWISE_JERK_SPEED"
    type: "PiecewiseJerkSpeedOptimizer"
  }
}
stage: {
  name: "TRAFFIC_LIGHT_PROTECTED_INTERSECTION_CRUISE"
  type: "TrafficLightProtectedStageIntersectionCruise"
  enabled: true
  task {
    name: "LANE_FOLLOW_PATH"
    type: "LaneFollowPath"
  }
  task {
    name: "LANE_BORROW_PATH"
    type: "LaneBorrowPath"
  }
  task {
    name: "FALLBACK_PATH"
    type: "FallbackPath"
  }
  task {
    name: "PATH_DECIDER"
    type: "PathDecider"
  }
  task {
    name: "RULE_BASED_STOP_DECIDER"
    type: "RuleBasedStopDecider"
  }
  task {
    name: "ST_BOUNDS_DECIDER"
    type: "STBoundsDecider"
  }
  task {
    name: "SPEED_BOUNDS_PRIORI_DECIDER"
    type: "SpeedBoundsDecider"
  }
  task {
    name: "SPEED_HEURISTIC_OPTIMIZER"
    type: "PathTimeHeuristicOptimizer"
  }
  task {
    name: "SPEED_DECIDER"
    type: "SpeedDecider"
  }
  task {
    name: "SPEED_BOUNDS_FINAL_DECIDER"
    type: "SpeedBoundsDecider"
  }
  task {
    name: "PIECEWISE_JERK_SPEED"
    type: "PiecewiseJerkSpeedOptimizer"
  }
}
```

Since Stages are executed sequentially, if the current Stage has not finished, it continues to execute in the next cycle; if it has finished, execution switches directly to the next Stage:

modules/planning/planning_base/scenario_base/scenario.cc:

```bash
ScenarioResult Scenario::Process(
    const common::TrajectoryPoint& planning_init_point, Frame* frame) {
  if (current_stage_ == nullptr) {
    current_stage_ = CreateStage(
        *stage_pipeline_map_[scenario_pipeline_config_.stage(0).name()]);
    AINFO << "Create stage " << current_stage_->Name();
  }
  if (current_stage_->Name().empty()) {
    scenario_result_.SetScenarioStatus(ScenarioStatusType::STATUS_DONE);
    return scenario_result_;
  }
  auto ret = current_stage_->Process(planning_init_point, frame);
  scenario_result_.SetStageResult(ret);
  switch (ret.GetStageStatus()) {
    case StageStatusType::ERROR: {
      AERROR << "Stage '" << current_stage_->Name() << "' returns error";
      scenario_result_.SetScenarioStatus(ScenarioStatusType::STATUS_UNKNOWN);
      break;
    }
    // Continue executing the current Stage in the next cycle
    case StageStatusType::RUNNING: {
      scenario_result_.SetScenarioStatus(ScenarioStatusType::STATUS_PROCESSING);
      break;
    }
    // Switch to the next Stage
    case StageStatusType::FINISHED: {
      auto next_stage = current_stage_->NextStage();
      ... ...
    }
    default: {
      AWARN << "Unexpected Stage return value: "
            << static_cast<int>(ret.GetStageStatus());
      scenario_result_.SetScenarioStatus(ScenarioStatusType::STATUS_UNKNOWN);
    }
  }
  return scenario_result_;
}

```

Taking `TrafficLightProtectedStageApproach` as an example, its `Process` function executes as follows:

modules/planning/scenarios/traffic_light_protected/stage_approach.cc:

```bash
StageResult TrafficLightProtectedStageApproach::Process(
    const TrajectoryPoint& planning_init_point, Frame* frame) {
  ADEBUG << "stage: Approach";
  CHECK_NOTNULL(frame);
  CHECK_NOTNULL(context_);

  auto context = GetContextAs<TrafficLightProtectedContext>();
  const ScenarioTrafficLightProtectedConfig& scenario_config =
      context->scenario_config;
  // Plan along the reference line
  StageResult result = ExecuteTaskOnReferenceLine(planning_init_point, frame);
  if (result.HasError()) {
    AERROR << "TrafficLightProtectedStageApproach planning error";
  }

  if (context->current_traffic_light_overlap_ids.empty()) {
    return FinishScenario();
  }

  const auto& reference_line_info = frame->reference_line_info().front();

  ... ...

  if (traffic_light_all_done) {
    return FinishStage();
  }

  return result.SetStageStatus(StageStatusType::RUNNING);
}

```

Here `ExecuteTaskOnReferenceLine` plans along the reference line and executes the Tasks contained in the current Stage.

modules/planning/planning_base/scenario_base/stage.cc:

```bash
StageResult Stage::ExecuteTaskOnReferenceLine(
    const common::TrajectoryPoint& planning_start_point, Frame* frame) {
  StageResult stage_result;
  if (frame->reference_line_info().empty()) {
    AERROR << "referenceline is empty in stage" << name_;
    return stage_result.SetStageStatus(StageStatusType::ERROR);
  }
  for (auto& reference_line_info : *frame->mutable_reference_line_info()) {
    if (!reference_line_info.IsDrivable()) {
      AERROR << "The generated path is not drivable";
      return stage_result.SetStageStatus(StageStatusType::ERROR);
    }
    // Execute the Tasks contained in the Stage
    for (auto task : task_list_) {
      const double start_timestamp = Clock::NowInSeconds();

      const auto ret = task->Execute(frame, &reference_line_info);

      const double end_timestamp = Clock::NowInSeconds();
      const double time_diff_ms = (end_timestamp - start_timestamp) * 1000;
      ADEBUG << "after task[" << task->Name()
             << "]: " << reference_line_info.PathSpeedDebugString();
      ADEBUG << task->Name() << " time spend: " << time_diff_ms << " ms.";
      RecordDebugInfo(&reference_line_info, task->Name(), time_diff_ms);

      if (!ret.ok()) {
        stage_result.SetTaskStatus(ret);
        AERROR << "Failed to run tasks[" << task->Name()
               << "], Error message: " << ret.error_message();
        break;
      }
    }
    ... ...
    return stage_result;
  }
  return stage_result;
}

```

The above uses `Process`, which calls `ExecuteTaskOnReferenceLine`, as an example; the implementation of `Process` may differ in different scenarios. The benefit of this design is that when a problem is found in one scenario, modifying it does not affect other scenarios. It also allows optimization for different scenarios, which suits individual scenarios better than generic planning. Each scenario has a dedicated directory for optimization.

### Task

The parent class of Task is defined in `modules/planning/planning_base/task_base/task.h`. Each concrete task inherits from the Task parent class and is an independent plugin package. The concrete task subclasses are under the `modules/planning/tasks` directory:

modules/planning/tasks:

```bash
.
├── fallback_path                            // fallback path generation
├── lane_borrow_path                         // lane borrow path generation
├── lane_change_path                         // lane change path generation
├── lane_follow_path                         // path generation for going straight along the current lane
├── open_space_fallback_decider              // lane change path generation
├── open_space_pre_stop_decider              // Before starting open space parking, computes the position near the parking space where the vehicle stops to prepare for parking
├── open_space_roi_decider                   // computation of the feasible region during parking
├── open_space_trajectory_partition          // parking trajectory partition: splits the trajectory by forward and reverse driving
├── open_space_trajectory_provider           // parking trajectory generation
├── path_decider                             // makes decisions on obstacles along the path: whether to stop longitudinally or ignore laterally
├── path_reference_decider                   //
├── path_time_heuristic                      // speed dynamic programming
├── piecewise_jerk_speed                     // speed planning based on quadratic programming
├── piecewise_jerk_speed_nonlinear           // speed planning based on nonlinear programming
├── pull_over_path                           // pull over path generation
├── reuse_path                               // handles whether the path is reused
├── rss_decider                              // reports RSS information
├── rule_based_stop_decider                  // sets the vehicle stop wall according to rules
├── speed_bounds_decider                     // generates speed bounds
├── speed_decider                            // sets obstacle labels according to the speed_profile from speed dynamic programming
├── st_bounds_decider                        // generates the ST graph, providing a searchable space for speed dynamic programming

```
