# Left-Turn Waiting Scenario Simulation Debugging

## Cloud experiment

This practice is now available in the Apollo Cloud Lab. Click to quick start and try it out: [Apollo Planning: Traffic Light Scenario Simulation and Debugging](https://apollo.baidu.com/community/course/25).

## Review of the Planning runtime mechanism

Apollo uses a dual state machine framework. A scenario (Scenario) is the state machine of the planning module and is switched and scheduled by the scenario manager; a stage (Stage) is the state machine of a scenario and is switched and scheduled by the scenario; a task (Task) is a concrete planning computation and is orchestrated and scheduled by a stage. Tasks can reuse the existing tasks in the task library, or new tasks can be developed and added to the task library for reuse by the stages of various scenarios.

![image (15).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2815%29_3eb0d1e.png)

### Simulation scenario requirements

<!--
## Scenario parameter configuration

### Enabling scenario plugins
The scenario plugin configuration file defines the planning scenario plugins you need to enable. It is configured in `planner_public_road_config` in `modules/planning/planning_base/conf/planning_config.pb.txt`, and the configured scenario plugins are dynamically loaded into the planning module process when the planning module runs. The following example indicates that only the parking and lane-following scenarios are started:

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

Here, name is the name of the scenario, usually written in uppercase with underscores, and type is the class name of the scenario; both are strings. You can see all the scenario plugins included in apollo in the `modules/planning/scenarios` directory.


### Scenario pipeline configuration
Each scenario has a scenario pipeline configuration file, which configures the stage plugins loaded by the scenario and the task plugins that each stage needs to load.

The scenario pipeline configuration file is `pipeline.pb.txt` in the scenario's `conf` directory. For example, the pipeline configuration file of the unprotected left turn scenario is at `modules/planning/scenarios/traffic_light_unprotected_left_turn/conf/pipeline.pb.txt`:

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
  ...
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

As shown above, 3 stages are configured for the unprotected left turn scenario: approaching the intersection, creeping to observe the intersection, and crossing the intersection. Each stage is configured with the tasks it needs to execute. You can add or remove scenario functionality by configuring the task pipeline. For example, if you do not want to borrow a lane to bypass while approaching the intersection, you can delete the configuration of the LANE_BORROW_PATH task.

### Scenario parameter configuration
The scenario parameter configuration file is `scenario_conf.pb.txt` in the scenario directory. For example, the configuration file of the unprotected left turn scenario is `modules/planning/scenarios/traffic_light_unprotected_left_turn/conf/scenario_conf.pb.txt`.

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

By adjusting `start_traffic_light_scenario_distance` you can adjust the distance at which the scenario is entered, and by adjusting `approach_cruise_speed` you can adjust the cruise speed of the approaching-intersection stage.


### Scenario task configuration

A scenario can reuse all the tasks in the task library. If the task parameters under the scenario conflict with the default task parameters, the task parameters can be set separately for that scenario. Scenario task parameters are saved in the stage subdirectory of the scenario's conf configuration directory. If the default task parameters are fully reused, no scenario task configuration file needs to be added. For example, in the emergency pull over scenario emergency_pull_over, in the pull over stage EMERGENCY_PULL_OVER_APPROACH, pull_over_path is needed to plan the pull over path, but its parameters differ from the default pull over task parameters, so the scenario task configuration parameters pull_over_path.pb.txt for pull_over_path need to be added and placed in the configuration directory of the scenario stage:

```bash
modules/planning/scenarios/emergency_pull_over/conf/emergency_pull_over_approach/pull_over_path.pb.txt
```
#### Task configuration parameters

Scenario task configuration parameters only take effect within the corresponding scenario, while modifying a task's default parameters takes effect for all scenarios. The configuration parameters of each task are saved in `default_conf.pb.txt` in the task's `conf` directory. For example, the default configuration parameters of the planning pull over task are in:
`modules/planning/tasks/pull_over_path/conf/default_conf.pb.txt`.

## Secondary development of scenarios
- A Scenario can be divided by geographic location. When the planning actions of the ego vehicle in a specific area of the scene differ from those of the default scenario (Lane Follow), a new scenario can be developed for it to avoid affecting the operation of the default scenario. For example, a traffic light scenario needs to be developed when there is a traffic light ahead, and a stop sign scenario when there is a stop sign ahead;

- A Scenario can also be divided by command. When an emergency pull over command is received, the emergency pull over scenario is entered. For example, when a parking command is received, the parking scenario is entered.

To develop a new Scenario, you need to inherit from the Scenario base class:


```bash
class Scenario {
 public:
  Scenario();

  virtual ~Scenario() = default;

  virtual bool Init(std::shared_ptr<DependencyInjector> injector,
                    const std::string& name);

  /**
   * @brief Get the scenario context.
   */
  virtual ScenarioContext* GetContext() = 0;

  /**
   * Each scenario should define its own transfer condition, i.e., when it
   * should allow to transfer from other scenario to itself.
   */
  virtual bool IsTransferable(const Scenario* other_scenario,
                              const Frame& frame) {
    return false;
  }

  virtual ScenarioResult Process(
      const common::TrajectoryPoint& planning_init_point, Frame* frame);

  virtual bool Exit(Frame* frame) { return true; }

  virtual bool Enter(Frame* frame) { return true; }

  /**
   * Each scenario should define its own stages object's creation
   * scenario will call stage's Stage::Process function following a configured
   * order, The return value of Stage::Process function determines the
   * transition from one stage to another.
   */
  std::shared_ptr<Stage> CreateStage(const StagePipeline& stage_pipeline);

  const ScenarioStatusType& GetStatus() const {
    return scenario_result_.GetScenarioStatus();
  }

  const std::string GetStage() const;

  const std::string& GetMsg() const { return msg_; }

  const std::string& Name() const { return name_; }

  /**
   * @brief Reset the scenario, used before entering the scenario.
   */
  void Reset();

 protected:
  template <typename T>
  bool LoadConfig(T* config);

  ScenarioResult scenario_result_;
  std::shared_ptr<Stage> current_stage_;
  std::unordered_map<std::string, const StagePipeline*> stage_pipeline_map_;
  std::string msg_;  // debug msg
  std::shared_ptr<DependencyInjector> injector_;

  std::string config_path_;
  std::string config_dir_;
  std::string name_;
  ScenarioPipeline scenario_pipeline_config_;
};

template <typename T>
bool Scenario::LoadConfig(T* config) {
  return apollo::cyber::common::GetProtoFromFile(config_path_, config);
}
```

You need to implement several functions here.

### Scenario initialization
Scenario initialization needs to override the `Init()` function of Scenario. The Init function of the scenario base class mainly loads the scenario's pipeline from the scenario plugin and saves the loaded Stage instances into `stage_pipeline_map_`. If the scenario has its own configuration file, you can call the `Scenario::LoadConfig<T>()` function to load the scenario's own configuration file and save it in the scenario instance context variable `context_`.

The following is an example of the initialization of a pull over Scenario. First call the Init function of the base class to load the Stages contained in the Scenario, then call `Scenario::LoadConfig<ScenarioPullOverConfig>()` to load the configuration file of the pull over scenario and save it in the pull over scenario context variable `context_`. Context variables can pass configuration and data between the Scenario and its Stages.


```bash
bool PullOverScenario::Init(std::shared_ptr<DependencyInjector> injector,
                            const std::string& name) {
  if (init_) {
    return true;
  }

  if (!Scenario::Init(injector, name)) {
    AERROR << "failed to init scenario" << Name();
    return false;
  }

  if (!Scenario::LoadConfig<ScenarioPullOverConfig>(
          &context_.scenario_config)) {
    AERROR << "fail to get config of scenario" << Name();
    return false;
  }

  init_ = true;
  return true;
}

struct PullOverContext : public ScenarioContext {
  ScenarioPullOverConfig scenario_config;
};
```

### Scenario transfer function

The scenario transfer function is mainly called by the scenario manager to determine whether to switch into this scenario. The scenario transfer function overrides `IsTransferable()` of the Scenario base class and determines whether the current frame can switch into this scenario. If it returns true, the scenario is switched into and the judgment of subsequent scenarios is no longer performed. Scenario entry includes a geographic-location-triggered method, such as the stop sign scenario: obtain from the reference line whether the first overlap is a stop sign; if so, and the distance between the ego vehicle and the stop sign meets the set distance, the stop sign scenario is entered.

```bash
bool StopSignUnprotectedScenario::IsTransferable(
    const Scenario* const other_scenario, const Frame& frame) {
  if (!frame.local_view().planning_command->has_lane_follow_command()) {
    return false;
  }
  if (other_scenario == nullptr || frame.reference_line_info().empty()) {
    return false;
  }
  const auto& reference_line_info = frame.reference_line_info().front();
  const auto& first_encountered_overlaps =
      reference_line_info.FirstEncounteredOverlaps();
  // note: first_encountered_overlaps already sorted
  hdmap::PathOverlap* stop_sign_overlap = nullptr;
  for (const auto& overlap : first_encountered_overlaps) {
    if (overlap.first == ReferenceLineInfo::SIGNAL ||
        overlap.first == ReferenceLineInfo::YIELD_SIGN) {
      return false;
    } else if (overlap.first == ReferenceLineInfo::STOP_SIGN) {
      stop_sign_overlap = const_cast<hdmap::PathOverlap*>(&overlap.second);
      break;
    }
  }
  if (stop_sign_overlap == nullptr) {
    return false;
  }
  const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();
  const double adc_distance_to_stop_sign =
      stop_sign_overlap->start_s - adc_front_edge_s;
  ADEBUG << "adc_distance_to_stop_sign[" << adc_distance_to_stop_sign
         << "] stop_sign[" << stop_sign_overlap->object_id
         << "] stop_sign_overlap_start_s[" << stop_sign_overlap->start_s << "]";
  const bool stop_sign_scenario =
      (adc_distance_to_stop_sign > 0.0 &&
       adc_distance_to_stop_sign <=
           context_.scenario_config.start_stop_sign_scenario_distance());

  return stop_sign_scenario;
}
```
Another way to enter a scenario is command-triggered. For example, for the emergency pull over scenario, the entry condition is that the `pad_msg` command is received, which enters the emergency pull over scenario:


```bash
bool EmergencyPullOverScenario::IsTransferable(
    const Scenario* const other_scenario, const Frame& frame) {
  if (frame.reference_line_info().empty()) {
    return false;
  }
  const auto& pad_msg_driving_action = frame.GetPadMsgDrivingAction();
  if (pad_msg_driving_action == PadMessage::PULL_OVER) {
    return true;
  }
  return false;
}
```

### Scenario enter and exit functions
The scenario enter function overrides the `Enter()` function of the base class. It is called before the scenario is entered for the first time to do some preprocessing and reset variables in the scenario.
For example, in the `Enter()` function of the stop sign scenario, the stop sign id of the reference line is first found and saved in the context variable, and then the global variables of the stop sign are reset.


```bash
bool StopSignUnprotectedScenario::Enter(Frame* frame) {
  const auto& reference_line_info = frame->reference_line_info().front();
  std::string current_stop_sign_overlap_id;
  const auto& overlaps = reference_line_info.FirstEncounteredOverlaps();
  for (auto overlap : overlaps) {
    if (overlap.first == ReferenceLineInfo::STOP_SIGN) {
      current_stop_sign_overlap_id = overlap.second.object_id;
      break;
    }
  }

  if (current_stop_sign_overlap_id.empty()) {
    injector_->planning_context()
        ->mutable_planning_status()
        ->mutable_stop_sign()
        ->Clear();
    AERROR << "Can not find stop sign overlap in refline";
    return false;
  }

  const std::vector<hdmap::PathOverlap>& stop_sign_overlaps =
      reference_line_info.reference_line().map_path().stop_sign_overlaps();
  auto stop_sign_overlap_itr = std::find_if(
      stop_sign_overlaps.begin(), stop_sign_overlaps.end(),
      [&current_stop_sign_overlap_id](const hdmap::PathOverlap& overlap) {
        return overlap.object_id == current_stop_sign_overlap_id;
      });

  if (stop_sign_overlap_itr != stop_sign_overlaps.end()) {
    injector_->planning_context()
        ->mutable_planning_status()
        ->mutable_stop_sign()
        ->set_current_stop_sign_overlap_id(current_stop_sign_overlap_id);
    ADEBUG << "Update PlanningContext with first_encountered stop sign["
           << current_stop_sign_overlap_id << "] start_s["
           << stop_sign_overlap_itr->start_s << "]";
  } else {
    AERROR << "Can not find stop sign overlap " << current_stop_sign_overlap_id;
    return false;
  }

  hdmap::StopSignInfoConstPtr stop_sign = HDMapUtil::BaseMap().GetStopSignById(
      hdmap::MakeMapId(current_stop_sign_overlap_id));
  if (!stop_sign) {
    AERROR << "Could not find stop sign: " << current_stop_sign_overlap_id;
    return false;
  }
  context_.current_stop_sign_overlap_id = current_stop_sign_overlap_id;
  context_.watch_vehicles.clear();

  GetAssociatedLanes(*stop_sign);
  return true;
}
```
The scenario exit function overrides the `Exit()` function of the base class. It is called when the scenario is switched out and can be used to clear some global variables, such as the exit function of the stop sign scenario.


```bash
bool StopSignUnprotectedScenario::Exit(Frame* frame) {
  injector_->planning_context()
      ->mutable_planning_status()
      ->mutable_stop_sign()
      ->Clear();
  return true;
}
```

### Scenario run function

The scenario run function overrides the `Process()` function of the base class and is called every frame when the scenario runs. The base class Process is mainly used to create Stages, run the Process function of the Stage, and schedule the switching between different Stages.

Scenarios in apollo generally use the base class `Process()` function by default. If you have more customization needs, you can override the Process function to rewrite the business strategy.

### Scenario context variables

Scenario context variables are member variables of the scenario that pass data across frames and between scenario and stage, for example the context variables of the stop sign scenario:


```bash
// stage context
struct StopSignUnprotectedContext : public ScenarioContext {
  ScenarioStopSignUnprotectedConfig scenario_config;
  std::string current_stop_sign_overlap_id;
  double stop_start_time = 0.0;
  double creep_start_time = 0.0;
  // watch_vehicle: <lane_id, perception_obstacle_ids>
  std::unordered_map<std::string, std::vector<std::string>> watch_vehicles;
  std::vector<std::pair<hdmap::LaneInfoConstPtr, hdmap::OverlapInfoConstPtr>>
      associated_lanes;
};
```

## Secondary development of stages

A Stage in Apollo is the second-level state machine under a Scenario and can be divided by time. When a scenario has business logic with a sequential order, it can be divided into multiple Stages. For example, the traffic light unprotected left turn scenario can be divided into three stages:
- The first stage is the process of approaching and driving to the traffic light stop line,
- The second is the process of creeping slowly and observing when the traffic light is green,
- The third is the process of quickly passing when the oncoming straight lanes are clear.


```bash
stage: {
  name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_APPROACH"
  type: "TrafficLightUnprotectedLeftTurnStageApproach"
  enabled: true
  }
}
stage: {
  name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_CREEP"
  type: "TrafficLightUnprotectedLeftTurnStageCreep"
  enabled: true
}
stage: {
  name: "TRAFFIC_LIGHT_UNPROTECTED_LEFT_TURN_INTERSECTION_CRUISE"
  type: "TrafficLightUnprotectedLeftTurnStageIntersectionCruise"
  enabled: true
}
```
After the stages are divided, tasks (Task) need to be configured for the stages. In each frame of planning, only one stage runs, and within a stage each task in the stage is executed in order. A task is the smallest computing unit for path planning or speed planning. You can reuse the existing tasks in Apollo for configuration, or develop new planning tasks as needed.

### Stage initialization
When a scenario runs, the stage instances contained in the scenario are created and the stages are initialized. If a stage needs initialization, you can override the stage's `Init()` function. The base class `Init()` function is mainly used to load the task pipeline and initialize the tasks. Apollo generally uses the base class Init function.


```bash
bool Stage::Init(const StagePipeline& config,
                 const std::shared_ptr<DependencyInjector>& injector,
                 const std::string& config_dir, void* context) {
  pipeline_config_ = config;
  next_stage_ = config.name();
  injector_ = injector;
  name_ = config.name();
  context_ = context;
  injector_->planning_context()
      ->mutable_planning_status()
      ->mutable_scenario()
      ->set_stage_type(name_);
  std::string path_name = ConfigUtil::TransformToPathName(name_);
  // Load task plugin
  for (int i = 0; i < pipeline_config_.task_size(); ++i) {
    auto task = pipeline_config_.task(i);
    auto task_type = task.type();
    auto task_ptr = apollo::cyber::plugin_manager::PluginManager::Instance()
                        ->CreateInstance<Task>(
                            ConfigUtil::GetFullPlanningClassName(task_type));
    if (nullptr == task_ptr) {
      AERROR << "Create task " << task.name() << " of " << name_ << " failed!";
      return false;
    }
    std::string task_config_dir = config_dir + "/" + path_name;
    task_ptr->Init(task_config_dir, task.name(), injector);
    task_list_.push_back(task_ptr);
    tasks_[task.name()] = task_ptr;
  }
  return true;
}
```

### Stage run function

The stage run function `Process()` is mainly used to maintain the state of the current stage. A stage has three states: RUNNING, FINISHED and ERROR. Among them:
- RUNNING indicates that the current stage is running, and the Scenario continues to keep the current stage running;
- FINISHED indicates that the current stage is complete, and the Scenario switches into the next stage to run;
- ERROR indicates that the current planning has a serious fault. The Scenario reports it, and the ego vehicle brakes to a stop.

If the current stage is complete, in addition to returning FINISHED for the current stage, the name of the next stage `next_stage_` also needs to be specified, and the Scenario switches into the next stage according to the next\_stage of the current stage. If the specified `next_stage_` is an empty string, the Scenario considers that all Stages have finished running, and the Scenario also returns the finished state.

There are two base class functions that can be reused to call tasks in turn within a Stage: `ExecuteTaskOnReferenceLine`, mainly used to run a series of tasks on the main road, and `ExecuteTaskOnOpenSpace`, mainly used to run a series of tasks in open space.


```bash
  StageResult ExecuteTaskOnReferenceLine(
      const common::TrajectoryPoint& planning_start_point, Frame* frame);

  StageResult ExecuteTaskOnOpenSpace(Frame* frame);
```

The context data in the scenario is saved in the Stage's `context_` through a void\* pointer. To read the scenario's context data, the context variable can be parsed through the template function `GetContextAs()`.

```bash
  template <typename T>
  T* GetContextAs() const {
    return static_cast<T*>(context_);
  }

```

## Secondary development of tasks

When the tasks (Task) in Apollo cannot meet your scenario needs, you need to develop a brand-new task plugin. There are several types of Task base classes in Apollo:

- PathGeneration: mainly used to generate paths on the main road, such as the lane borrow path LaneBorrowPath, the pull over path PullOverPath, the lane following path LaneFollowPath, etc.,
- SpeedOptimizer: mainly used to plan speed profiles on the main road, such as speed planning based on quadratic programming and speed planning based on nonlinear programming,
- TrajectoryOptimizer: mainly used to generate trajectories, such as open space planning OpenSpaceTrajectoryProvider.

You can also inherit from the Task base class to implement your own task.

### Task initialization

Before running a task for the first time, the Stage calls the task's Init function to initialize it. The Init function of the base class is mainly used to obtain the task's default parameter path `default_config_path_` and the scenario task parameter path `config_path_`.

```bash
bool Task::Init(const std::string& config_dir, const std::string& name,
                const std::shared_ptr<DependencyInjector>& injector) {
  injector_ = injector;
  name_ = name;
  config_path_ =
      config_dir + "/" + ConfigUtil::TransformToPathName(name) + ".pb.txt";

  // Get the name of this class.
  int status;
  std::string class_name =
      abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
  // Generate the default task config path from PluginManager.
  default_config_path_ =
      apollo::cyber::plugin_manager::PluginManager::Instance()
          ->GetPluginClassHomePath<Task>(class_name) +
      "/conf/" + "default_conf.pb.txt";
  return true;
}
```
Then you can call the template function `Task::LoadConfig<T>(&config_)` to load the task parameters. For example, the task LaneBorrowPath that generates a lane borrow path: first call `Task::Init` to get the task parameter path, and then call `Task::LoadConfig<LaneBorrowPathConfig>()` to load the parameters of the lane borrow task.

```bash
bool LaneBorrowPath::Init(const std::string& config_dir,
                          const std::string& name,
                          const std::shared_ptr<DependencyInjector>& injector) {
  if (!Task::Init(config_dir, name, injector)) {
    return false;
  }
  // Load the config this task.
  return Task::LoadConfig<LaneBorrowPathConfig>(&config_);
}
```

The default parameters of a task are saved in `default_conf.pb.txt` in the task's `conf` directory. For example, the parameters of the lane borrow path task are in `modules/planning/tasks/lane_borrow_path/conf/default_conf.pb.txt`. The configuration parameters of a task are defined in proto format and saved in the task's `proto` directory. For example, the proto of the lane borrow path task is in `modules/planning/tasks/lane_borrow_path/proto`.


### Task execution


The task run function mainly overrides the Process function or Execute function of the base class. The input of the run function is usually `frame` and `reference_line_info`, and the output is also saved into `frame` and `reference_line_info`. For example, the lane borrow path task saves the generated path into `reference_line_info->mutable_path_data()`.

```bash
apollo::common::Status LaneBorrowPath::Process(
    Frame* frame, ReferenceLineInfo* reference_line_info) {
  if (!config_.is_allow_lane_borrowing() ||
      reference_line_info->path_reusable()) {
    ADEBUG << "path reusable" << reference_line_info->path_reusable()
           << ",skip";
    return Status::OK();
  }
  if (!IsNecessaryToBorrowLane()) {
    ADEBUG << "No need to borrow lane";
    return Status::OK();
  }
  std::vector<PathBoundary> candidate_path_boundaries;
  std::vector<PathData> candidate_path_data;

  GetStartPointSLState();
  if (!DecidePathBounds(&candidate_path_boundaries)) {
    return Status::OK();
  }
  if (!OptimizePath(candidate_path_boundaries, &candidate_path_data)) {
    return Status::OK();
  }
  if (AssessPath(&candidate_path_data,
                 reference_line_info->mutable_path_data())) {
    ADEBUG << "lane borrow path success";
  }

  return Status::OK();
}
```

-->

Here, taking `apollo_virtual_map` as the map, we develop a left-turn waiting scenario. The left-turn waiting scenario can be divided into two zones: the straight-through zone and the left-turn waiting zone.

- Ego vehicle in the straight-through zone:

  If the left-turn light is red and the straight light is green, the vehicle can enter the left-turn waiting zone:

  ![image (34).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2834%29_a02ad93.png)

  If the left-turn light is green, the vehicle can pass through the intersection regardless of whether the straight light is red or green:

  ![image (35).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2835%29_7839cce.png)

  If the left-turn light is red and the straight light is red, the vehicle waits at the stop line of the straight-through zone:

  ![image (36).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2836%29_0cdfea7.png)

- Ego vehicle in the left-turn zone:

  If the left-turn light is red and the straight light is green, the vehicle waits in front of the stop line of the left-turn zone:

  ![image (37).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2837%29_1668784.png)

  If the left-turn light is green, the vehicle can pass through the intersection:

  ![image (38).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2838%29_41ff878.png)

  > Note: if the left-turn light changes from green to red while in the waiting zone, the ego vehicle does not need to wait in the waiting zone and may pass through the intersection.
  > Additional requirement: the cruise speed differs in each zone: 5 m/s in the straight-through zone, 3 m/s in the left-turn zone, and 8 m/s when passing through the intersection.

  ![image (39).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2839%29_ab30a31.png)

  In the map file `base_map.txt`, you can observe the elements of the left-turn waiting zone:

  In the left-turn waiting scenario, the left-turn light has two stop lines on the left-turn lane, one in the straight-through zone and one in the left-turn zone, and the subsignal type of the left-turn light is `ARROW_LEFT`; the straight light has only one stop line on the left-turn lane, located in the straight-through zone, and the subsignal of the straight light is of type CIRCLE. This can be used to determine whether to enter the left-turn waiting scenario.

- Left-turn light element:

  ```bash
  signal {
  id {
    id: "451089192"
  }
  boundary {
    point {
      x: 750969.52326592465
      y: 2563961.3250804096
      z: 26.59372596
    }
    point {
      x: 750968.32370583259
      y: 2563961.3780084951
      z: 26.59372596
    }
    point {
      x: 750968.32370583259
      y: 2563961.3780084951
      z: 26.99372597
    }
    point {
      x: 750969.52326592465
      y: 2563961.3250804096
      z: 26.99372597
    }
  }
  subsignal {
    id {
      id: "0"
    }
    type: ARROW_LEFT
    location {
      x: 750969.3284603036
      y: 2563961.3339879983
      z: 26.79372597
    }
  }
  subsignal {
    id {
      id: "1"
    }
    type: ARROW_LEFT
    location {
      x: 750968.92860693939
      y: 2563961.3516306877
      z: 26.79372597
    }
  }
  subsignal {
    id {
      id: "2"
    }
    type: ARROW_LEFT
    location {
      x: 750968.5287535761
      y: 2563961.3692733883
      z: 26.79372597
    }
  }
  overlap_id {
    id: "overlap_263"
  }
  overlap_id {
    id: "overlap_306"
  }
  overlap_id {
    id: "overlap_319"
  }
  overlap_id {
    id: "overlap_324"
  }
  overlap_id {
    id: "overlap_908"
  }
  type: MIX_3_HORIZONTAL
  stop_line {
    segment {
      line_segment {
        point {
          x: 750970.81973172375
          y: 2564012.0906806863
        }
        point {
          x: 750968.8422244587
          y: 2564012.1039131097
        }
        point {
          x: 750966.8749406623
          y: 2564012.1184260151
        }
      }
    }
  }
  stop_line {
    segment {
      line_segment {
        point {
          x: 750972.57867658127
          y: 2563990.9899958805
        }
        point {
          x: 750970.86382346111
          y: 2563990.6154060815
        }
        point {
          x: 750969.1592123817
          y: 2563990.2409889824
        }
      }
    }
  }
  }
  ```

- Right-turn light element:

  ```bash
  signal {
  id {
    id: "451089193"
  }
  boundary {
    point {
      x: 750963.29795619729
      y: 2563961.7155308384
      z: 26.72153167
    }
    point {
      x: 750962.09768731019
      y: 2563961.8105529686
      z: 26.72153167
    }
    point {
      x: 750962.09768731019
      y: 2563961.8105529686
      z: 27.12153168
    }
    point {
      x: 750963.29795619729
      y: 2563961.7155308384
      z: 27.12153168
    }
  }
  subsignal {
    id {
      id: "0"
    }
    type: CIRCLE
    location {
      x: 750963.09279653709
      y: 2563961.7309123669
      z: 26.92153168
    }
  }
  subsignal {
    id {
      id: "1"
    }
    type: CIRCLE
    location {
      x: 750962.69270068756
      y: 2563961.762955647
      z: 26.92153168
    }
  }
  subsignal {
    id {
      id: "2"
    }
    type: CIRCLE
    location {
      x: 750962.29260484129
      y: 2563961.7949989373
      z: 26.92153168
    }
  }
  overlap_id {
    id: "overlap_253"
  }
  overlap_id {
    id: "overlap_257"
  }
  overlap_id {
    id: "overlap_264"
  }
  overlap_id {
    id: "overlap_909"
  }
  type: MIX_3_HORIZONTAL
  stop_line {
    segment {
      line_segment {
        point {
          x: 750970.81973172375
          y: 2564012.0906806863
        }
        point {
          x: 750968.8422244587
          y: 2564012.1039131097
        }
        point {
          x: 750966.8749406623
          y: 2564012.1184260151
        }
      }
    }
  }
  stop_line {
    segment {
      line_segment {
        point {
          x: 750966.87382129545
          y: 2564012.1848895182
        }
        point {
          x: 750964.86549455044
          y: 2564012.2031436064
        }
        point {
          x: 750962.85718649556
          y: 2564012.2202902385
        }
      }
    }
  }
  }
  ```

Stage division:

According to the driving process, we can divide it into:

- The stage of stopping and waiting in the straight-through zone, TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_APPROACH,
- The stage of creeping and waiting in the waiting zone, TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CREEP,
- The stage of passing through the intersection, TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE.

### Practice steps

1. In the package management environment, create a plugin template. The following command creates a template for a left-turn waiting plugin.

   ```bash
    buildtool create --template plugin --namespaces planning --dependencies planning:binary:planning --base_class_name apollo::planning::Scenario --class_name TrafficLightLeftTurnWaitingZone     modules/planning/scenarios/traffic_light_left_turn_waiting_zone
   ```

2. Based on the package management template, implement the left-turn waiting Scenario subclass.

   - a. In the `IsTransferable()` function, add the scenario entry condition: when two traffic light overlaps with the same id are found on the current reference line, they are left-turn signals, and one of the overlaps is in the nearest traffic light group, switch into this scenario.
   - b. In the `Enter()` function, set the parameter initialization before entering the scenario, and save the straight light and left-turn light ids into the `context_` variable.
   - c. In the `Exit()` function, set the reset of member variables and global variables when the scenario is switched out.

   File directory:

   ```bash
   planning/scenarios/traffic_light_left_turn_waiting_zone/traffic_light_left_turn_waiting_zone.cc
   ```

   ```bash
   #include <memory>
    #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/traffic_light_left_turn_waiting_zone.h"

    namespace apollo {
    namespace planning {

    using apollo::hdmap::HDMapUtil;

    bool TrafficLightLeftTurnWaitingZone::Init(std::shared_ptr<DependencyInjector> injector, const std::string& name) {
        if (!Scenario::Init(injector, name)) {
            AERROR << "failed to init scenario" << Name();
            return false;
        }

        if (!Scenario::LoadConfig<TrafficLightLeftTurnWaitingZoneConfig>(&context_.scenario_config)) {
            AERROR << "fail to get specific config of scenario " << Name();
            return false;
        }
        return true;
    }

    bool TrafficLightLeftTurnWaitingZone::IsTransferable(const Scenario* const other_scenario, const Frame& frame) {
        if (other_scenario == nullptr || frame.reference_line_info().empty()) {
            return false;
        }
        const auto& reference_line_info = frame.reference_line_info().front();
        const auto& first_encountered_overlaps = reference_line_info.FirstEncounteredOverlaps();
        const std::vector<hdmap::PathOverlap> traffic_light_overlaps
                = reference_line_info.reference_line().map_path().signal_overlaps();
        if (first_encountered_overlaps.empty()) {
            return false;
        }
        hdmap::PathOverlap* traffic_sign_overlap = nullptr;
        for (const auto& overlap : first_encountered_overlaps) {
            if (overlap.first == ReferenceLineInfo::STOP_SIGN || overlap.first == ReferenceLineInfo::YIELD_SIGN) {
                return false;
            } else if (overlap.first == ReferenceLineInfo::SIGNAL) {
                traffic_sign_overlap = const_cast<hdmap::PathOverlap*>(&overlap.second);
                break;
            }
        }
        if (traffic_sign_overlap == nullptr) {
            return false;
        }
        const double start_check_distance = context_.scenario_config.start_traffic_light_scenario_distance();
        const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();
        if (traffic_sign_overlap->start_s - adc_front_edge_s > start_check_distance) {
            return false;
        }
        const auto& turn_type = reference_line_info.GetPathTurnType(traffic_sign_overlap->start_s);
        if (turn_type != hdmap::Lane::LEFT_TURN) {
            return false;
        }
        auto* base_map = HDMapUtil::BaseMapPtr();

        static constexpr double kTrafficLightGroupingMaxDist = 2.0;  // unit: m
        for (const auto& overlap : traffic_light_overlaps) {
            const double dist = overlap.start_s - traffic_sign_overlap->start_s;
            if (fabs(dist) <= kTrafficLightGroupingMaxDist) {
                const auto& signal = base_map->GetSignalById(hdmap::MakeMapId(overlap.object_id))->signal();
                if (signal.subsignal_size() > 0 && signal.subsignal(0).type() == apollo::hdmap::Subsignal::ARROW_LEFT
                    && signal.stop_line_size() > 1) {
                    return true;
                }
            }
        }

        return false;
    }

    bool TrafficLightLeftTurnWaitingZone::Enter(Frame* frame) {
        const auto& reference_line_info = frame->reference_line_info().front();
        const auto& first_encountered_overlaps = reference_line_info.FirstEncounteredOverlaps();
        const std::vector<hdmap::PathOverlap> traffic_light_overlaps
                = reference_line_info.reference_line().map_path().signal_overlaps();
        hdmap::PathOverlap* nearest_traffic_light_overlap = nullptr;
        for (const auto& overlap : first_encountered_overlaps) {
            if (overlap.first == ReferenceLineInfo::SIGNAL) {
                nearest_traffic_light_overlap = const_cast<hdmap::PathOverlap*>(&overlap.second);
                break;
            }
        }
        std::vector<hdmap::PathOverlap> next_traffic_lights;
        auto* base_map = HDMapUtil::BaseMapPtr();
        std::string left_turn_signal_id;
        static constexpr double kTrafficLightGroupingMaxDist = 2.0;  // unit: m
        for (const auto& overlap : traffic_light_overlaps) {
            const double dist = overlap.start_s - nearest_traffic_light_overlap->start_s;
            if (fabs(dist) <= kTrafficLightGroupingMaxDist) {
                const auto signal = base_map->GetSignalById(hdmap::MakeMapId(overlap.object_id));
                AINFO << signal->id().id() << "," << overlap.object_id;
                if (signal->signal().subsignal(0).type() == apollo::hdmap::Subsignal::ARROW_LEFT) {
                    context_.left_turn_traffic_light_id = overlap.object_id;
                    AINFO << "left_turn_signal_id" << context_.left_turn_traffic_light_id;
                } else {
                    context_.forward_traffic_light_id = overlap.object_id;
                    AINFO << "forward signal id" << context_.forward_traffic_light_id;
                }
            }
        }

        return true;
    }

    bool TrafficLightLeftTurnWaitingZone::Exit(Frame* frame) {
        context_.left_turn_traffic_light_id.clear();
        context_.forward_traffic_light_id.clear();
        auto done_traffic_light = injector_->planning_context()->mutable_planning_status()->mutable_traffic_light();
        done_traffic_light->mutable_done_traffic_light_overlap_id()->Clear();
        return true;
    }

    }  // namespace planning
    }  // namespace apollo
   ```

   Add the header file `modules/planning/scenarios/traffic_light_left_turn_waiting_zone/traffic_light_left_turn_waiting_zone.h`:

   ```bash
   #pragma once

    #include <memory>
    #include "cyber/plugin_manager/plugin_manager.h"
    #include "modules/planning/planning_base/scenario_base/scenario.h"
    #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/proto/traffic_light_left_turn_waiting_zone.pb.h"

    namespace apollo {
    namespace planning {

    struct TrafficLightLeftTurnWaitingZoneContext : public ScenarioContext {
        TrafficLightLeftTurnWaitingZoneConfig scenario_config;
        std::string left_turn_traffic_light_id;
        std::string forward_traffic_light_id;
    };

    class TrafficLightLeftTurnWaitingZone : public apollo::planning::Scenario {
    public:
        bool Init(std::shared_ptr<DependencyInjector> injector, const std::string& name);
        TrafficLightLeftTurnWaitingZoneContext* GetContext() override {
            return &context_;
        }
        bool IsTransferable(const Scenario* other_scenario, const Frame& frame);
        bool Exit(Frame* frame);
        bool Enter(Frame* frame);

    private:
        TrafficLightLeftTurnWaitingZoneContext context_;
    };

    CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::planning::TrafficLightLeftTurnWaitingZone, apollo::planning::Scenario)

    }  // namespace planning
    }  // namespace apollo
   ```

3. Develop the first stage of left-turn waiting, the stage of driving to the stop line of the straight-through zone, TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_APPROACH, implementing the `Process()` function of the Stage:

   - a. Set the speed limit of this stage in `reference_line_info`,
   - b. If the left-turn light is green and the stop line has been passed, switch to the TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE stage, and add the straight light to the global variable `add_done_traffic_light_overlap_id`,
   - c. If the left-turn light is red and the straight light is green, switch to the TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CREEP stage, and add the left-turn light to the global variable `add_done_traffic_light_overlap_id`,
   - d. If the stop line has already been passed, switch into the TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE stage.

   File path:

   ```bash
   planning/scenarios/traffic_light_left_turn_waiting_zone/stage_approach.cc
   ```

   ```bash
   #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/stage_approach.h"
    #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/traffic_light_left_turn_waiting_zone.h"
    #include "modules/common_msgs/perception_msgs/traffic_light_detection.pb.h"
    #include "cyber/common/log.h"
    #include "modules/planning/planning_base/common/frame.h"
    #include "modules/planning/planning_base/common/planning_context.h"

    namespace apollo {
    namespace planning {

    using apollo::common::TrajectoryPoint;
    using apollo::hdmap::PathOverlap;
    using apollo::perception::TrafficLight;

    StageResult TrafficLightLeftTurnWaitingZoneStageApproach::Process(
            const TrajectoryPoint& planning_init_point,
            Frame* frame) {
        ADEBUG << "stage: Approach";
        CHECK_NOTNULL(frame);
        CHECK_NOTNULL(context_);

        auto* context = GetContextAs<TrafficLightLeftTurnWaitingZoneContext>();
        const auto& scenario_config = context->scenario_config;
        auto& reference_line_info = frame->mutable_reference_line_info()->front();
        reference_line_info.LimitCruiseSpeed(scenario_config.approach_speed());
        StageResult result = ExecuteTaskOnReferenceLine(planning_init_point, frame);
        if (result.HasError()) {
            AERROR << "TrafficLightLeftTurnWaitingZoneStageApproach planning error";
        }
        if (context->left_turn_traffic_light_id.empty() || context->forward_traffic_light_id.empty()) {
            return FinishScenario();
        }
        hdmap::PathOverlap forward_traffic_light_overlap;
        hdmap::PathOverlap left_turn_traffic_light_overlap_first;
        hdmap::PathOverlap left_turn_traffic_light_overlap_second;
        const std::vector<hdmap::PathOverlap> traffic_light_overlaps
                = reference_line_info.reference_line().map_path().signal_overlaps();
        for (const auto& overlap : traffic_light_overlaps) {
            if (overlap.object_id == context->forward_traffic_light_id) {
                forward_traffic_light_overlap = overlap;
            } else if (overlap.object_id == context->left_turn_traffic_light_id) {
                if (left_turn_traffic_light_overlap_second.start_s < overlap.start_s) {
                    std ::swap(left_turn_traffic_light_overlap_first, left_turn_traffic_light_overlap_second);
                    left_turn_traffic_light_overlap_second = overlap;
                } else {
                    left_turn_traffic_light_overlap_first = overlap;
                }
            }
        }
        const double adc_back_edge_s = reference_line_info.AdcSlBoundary().end_s();
        bool is_passed_forward_stop_line = adc_back_edge_s > forward_traffic_light_overlap.end_s;
        auto forward_signal_color = frame->GetSignal(context->forward_traffic_light_id).color();
        auto left_signal_color = frame->GetSignal(context->left_turn_traffic_light_id).color();
        auto done_traffic_light = injector_->planning_context()->mutable_planning_status()->mutable_traffic_light();
        done_traffic_light->mutable_done_traffic_light_overlap_id()->Clear();
        if (left_signal_color == TrafficLight::GREEN) {
            if (is_passed_forward_stop_line) {
                return FinishStage("TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE");
            }
            done_traffic_light->add_done_traffic_light_overlap_id(forward_traffic_light_overlap.object_id);
        } else if (forward_signal_color == TrafficLight::GREEN) {
            done_traffic_light->add_done_traffic_light_overlap_id(left_turn_traffic_light_overlap_first.object_id);
        }
        if (is_passed_forward_stop_line) {
            return FinishStage("TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CREEP");
        }

        return result.SetStageStatus(StageStatusType::RUNNING);
    }

    StageResult TrafficLightLeftTurnWaitingZoneStageApproach::FinishStage(std::string next_stage) {
        injector_->planning_context()
                ->mutable_planning_status()
                ->mutable_traffic_light()
                ->mutable_done_traffic_light_overlap_id()
                ->Clear();
        next_stage_ = next_stage;
        return StageResult(StageStatusType::FINISHED);
    }

    }  // namespace planning
    }  // namespace apollo
   ```

   Add the header file `modules/planning/scenarios/traffic_light_left_turn_waiting_zone/stage_approach.h`:

   ```bash
    #pragma once

    #include "cyber/plugin_manager/plugin_manager.h"
    #include "modules/planning/planning_base/scenario_base/stage.h"

    namespace apollo {
    namespace planning {

    class TrafficLightLeftTurnWaitingZoneStageApproach : public Stage {
    public:
        StageResult Process(const common::TrajectoryPoint& planning_init_point, Frame* frame) override;

    private:
        StageResult FinishStage(std::string next_stage);
    };

    CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(
            apollo::planning::TrafficLightLeftTurnWaitingZoneStageApproach,
            apollo::planning::Stage)

    }  // namespace planning
    }  // namespace apollo
   ```

4. Develop the second stage, the stage of driving in the waiting zone, TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CREEP, implementing the `Process()` function of the waiting zone stage:

   - a. Set the speed limit of this stage in `reference_line_info`
   - b. If the left-turn stop line has been passed, or the left-turn light is green, switch into the TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE stage.

   File path:

   ```bash
   planning/scenarios/traffic_light_left_turn_waiting_zone/stage_creep.cc
   ```

   ```bash
   #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/stage_creep.h"
    #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/traffic_light_left_turn_waiting_zone.h"
    #include <string>

    #include "modules/common_msgs/perception_msgs/traffic_light_detection.pb.h"
    #include "cyber/common/log.h"
    #include "modules/planning/planning_base/common/frame.h"
    #include "modules/planning/planning_base/common/planning_context.h"

    namespace apollo {
    namespace planning {

    using apollo::common::TrajectoryPoint;
    using apollo::hdmap::PathOverlap;
    using apollo::perception::TrafficLight;

    bool TrafficLightLeftTurnWaitingZoneStageCreep::Init(
            const StagePipeline& config,
            const std::shared_ptr<DependencyInjector>& injector,
            const std::string& config_dir,
            void* context) {
        CHECK_NOTNULL(context);
        bool ret = Stage::Init(config, injector, config_dir, context);
        if (!ret) {
            AERROR << Name() << "init failed!";
            return false;
        }
        return ret;
    }

    StageResult TrafficLightLeftTurnWaitingZoneStageCreep::Process(
            const TrajectoryPoint& planning_init_point,
            Frame* frame) {
        CHECK_NOTNULL(frame);
        CHECK_NOTNULL(context_);

        auto* context = GetContextAs<TrafficLightLeftTurnWaitingZoneContext>();
        const auto& scenario_config = context->scenario_config;
        auto& reference_line_info = frame->mutable_reference_line_info()->front();
        reference_line_info.LimitCruiseSpeed(scenario_config.creep_speed());
        StageResult result = ExecuteTaskOnReferenceLine(planning_init_point, frame);
        if (result.HasError()) {
            AERROR << "TrafficLightLeftTurnWaitingZoneStageCreep planning error";
        }
        hdmap::PathOverlap forward_traffic_light_overlap;
        hdmap::PathOverlap left_turn_traffic_light_overlap_first;
        hdmap::PathOverlap left_turn_traffic_light_overlap_second;
        const std::vector<hdmap::PathOverlap> traffic_light_overlaps
                = reference_line_info.reference_line().map_path().signal_overlaps();
        for (const auto& overlap : traffic_light_overlaps) {
            if (overlap.object_id == context->forward_traffic_light_id) {
                forward_traffic_light_overlap = overlap;
            } else if (overlap.object_id == context->left_turn_traffic_light_id) {
                if (left_turn_traffic_light_overlap_second.start_s < overlap.start_s) {
                    std ::swap(left_turn_traffic_light_overlap_first, left_turn_traffic_light_overlap_second);
                    left_turn_traffic_light_overlap_second = overlap;
                } else {
                    left_turn_traffic_light_overlap_first = overlap;
                }
            }
        }
        const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();
        bool is_passed_left_turn_stop_line = adc_front_edge_s > left_turn_traffic_light_overlap_second.end_s;
        auto left_signal_color = frame->GetSignal(context->left_turn_traffic_light_id).color();
        auto done_traffic_light = injector_->planning_context()->mutable_planning_status()->mutable_traffic_light();
        done_traffic_light->mutable_done_traffic_light_overlap_id()->Clear();
        if (is_passed_left_turn_stop_line || left_signal_color == TrafficLight::GREEN) {
            reference_line_info.LimitCruiseSpeed(scenario_config.cruise_speed());
            return FinishStage();
        }

        return result.SetStageStatus(StageStatusType::RUNNING);
    }

    StageResult TrafficLightLeftTurnWaitingZoneStageCreep::FinishStage() {
        next_stage_ = "TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE";
        return StageResult(StageStatusType::FINISHED);
    }

    }  // namespace planning
    }  // namespace apollo
   ```

   Add the header file `modules/planning/scenarios/traffic_light_left_turn_waiting_zone/stage_creep.h`:

   ```bash
   #pragma once

    #include <memory>
    #include <string>

    #include "cyber/plugin_manager/plugin_manager.h"
    #include "modules/planning/planning_base/scenario_base/stage.h"

    namespace apollo {
    namespace planning {

    class TrafficLightLeftTurnWaitingZoneStageCreep : public Stage {
    public:
        bool Init(
                const StagePipeline& config,
                const std::shared_ptr<DependencyInjector>& injector,
                const std::string& config_dir,
                void* context) override;

        StageResult Process(const common::TrajectoryPoint& planning_init_point, Frame* frame) override;

    private:
        StageResult FinishStage();
    };

    CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::planning::TrafficLightLeftTurnWaitingZoneStageCreep, Stage)

    }  // namespace planning
    }  // namespace apollo
   ```

5. Develop the third stage, passing through the intersection, TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE.

   - a. This stage can inherit from the existing intersection stage base class BaseStageTrafficLightCruise,
   - b. Set the speed limit of the current stage,
   - c. This stage ignores traffic light information, so both the straight light and the left-turn light are added to `add_done_traffic_light_overlap_id`.

   File path:

   ```bash
   planning/scenarios/traffic_light_left_turn_waiting_zone/stage_intersection_cruise.cc
   ```

   ```bash
       #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/stage_intersection_cruise.h"
    #include "modules/planning/scenarios/traffic_light_left_turn_waiting_zone/traffic_light_left_turn_waiting_zone.h"
    #include "cyber/common/log.h"

    namespace apollo {
    namespace planning {

    StageResult TrafficLightLeftTurnWaitingZoneStageIntersectionCruise::Process(
            const common::TrajectoryPoint& planning_init_point,
            Frame* frame) {
        const auto* context = GetContextAs<TrafficLightLeftTurnWaitingZoneContext>();
        const auto& scenario_config = context->scenario_config;
        auto& reference_line_info = frame->mutable_reference_line_info()->front();
        reference_line_info.LimitCruiseSpeed(scenario_config.cruise_speed());
        StageResult result = ExecuteTaskOnReferenceLine(planning_init_point, frame);
        auto done_traffic_light = injector_->planning_context()->mutable_planning_status()->mutable_traffic_light();
        done_traffic_light->mutable_done_traffic_light_overlap_id()->Clear();
        done_traffic_light->add_done_traffic_light_overlap_id(context->forward_traffic_light_id);
        done_traffic_light->add_done_traffic_light_overlap_id(context->left_turn_traffic_light_id);
        if (result.HasError()) {
            AERROR << "TrafficLightLeftTurnWaitingZoneStageIntersectionCruise "
                   << "plan error";
        }

        bool stage_done = CheckDone(*frame, injector_->planning_context(), true);
        if (stage_done) {
            return FinishStage();
        }
        return result.SetStageStatus(StageStatusType::RUNNING);
    }

    StageResult TrafficLightLeftTurnWaitingZoneStageIntersectionCruise::FinishStage() {
        return FinishScenario();
    }

    }  // namespace planning
    }  // namespace apollo
   ```

   Add the header file `modules/planning/scenarios/traffic_light_left_turn_waiting_zone/stage_intersection_cruise.h`:

   ```bash
   #pragma once

    #include <string>

    #include "cyber/plugin_manager/plugin_manager.h"
    #include "modules/planning/planning_base/scenario_base/traffic_light_base/base_stage_traffic_light_cruise.h"

    namespace apollo {
    namespace planning {

    class TrafficLightLeftTurnWaitingZoneStageIntersectionCruise : public BaseStageTrafficLightCruise {
    public:
        StageResult Process(const common::TrajectoryPoint& planning_init_point, Frame* frame) override;

    private:
        StageResult FinishStage();
    };

    CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::planning::TrafficLightLeftTurnWaitingZoneStageIntersectionCruise, Stage)

    }  // namespace planning
    }  // namespace apollo
   ```

6. Complete the scenario's pipeline configuration `conf/pipeline.pb.txt`, configuring the stages to be started and the tasks used by each stage.

   File path:

   ```bash
   planning/scenarios/traffic_light_left_turn_waiting_zone/conf/pipeline.pb.txt
   ```

   ```bash
   stage: {
      name: "TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_APPROACH"
      type: "TrafficLightLeftTurnWaitingZoneStageApproach"
      enabled: true
      task {
        name: "LANE_FOLLOW_PATH"
        type: "LaneFollowPath"
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
      name: "TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CREEP"
      type: "TrafficLightLeftTurnWaitingZoneStageCreep"
      enabled: true
      task {
        name: "LANE_FOLLOW_PATH"
        type: "LaneFollowPath"
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
      name: "TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE_CRUISE"
      type: "TrafficLightLeftTurnWaitingZoneStageIntersectionCruise"
      enabled: true
        task {
        name: "LANE_FOLLOW_PATH"
        type: "LaneFollowPath"
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

7. Add the scenario's parameter file.

- Configure the parameters used by the scenario into a proto file:

  File path:

  ```bash
  planning/scenarios/traffic_light_left_turn_waiting_zone/proto/traffic_light_left_turn_waiting_zone.proto
  ```

  ```bash
  syntax = "proto2";

  package apollo.planning;

  message TrafficLightLeftTurnWaitingZoneConfig {
    optional double start_traffic_light_scenario_distance = 1;  // meter
    optional double approach_speed = 2;
    optional double creep_speed = 3;
    optional double cruise_speed = 4;
  }
  ```

* Modify the build file BUILD of the configuration to build it into one library.

  File path:

  ```bash
  proto_library(planning/scenarios/traffic_light_left_turn_waiting_zone/proto/BUILD
  ```

  ```bash
  load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
  load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package", "apollo_plugin")
  load("//tools/proto:proto.bzl", "proto_library")
  load("//tools:cpplint.bzl", "cpplint")

  package(default_visibility = ["//visibility:public"])

  proto_library(
      name = "traffic_light_left_turn_waiting_zone_proto",
      srcs = ["traffic_light_left_turn_waiting_zone.proto"],
  )

  apollo_package()

  cpplint()
  ```

* Add the scenario's parameter configuration to `planning/scenarios/traffic_light_left_turn_waiting_zone/conf/scenario_conf.pb.txt`.

  ```bash
  start_traffic_light_scenario_distance: 30.0
  approach_speed:5.0
  creep_speed: 3.0
  cruise_speed:8.0
  ```

8. Register the added scenario and stage plugins in the file `modules/planning/scenarios/traffic_light_left_turn_waiting_zone/plugin_traffic_light_left_turn_waiting_zone_description.xml`.
   ```bash
   <library path="modules/planning/scenarios/traffic_light_left_turn_waiting_zone/libtraffic_light_left_turn_waiting_zone.so">
    <class type="apollo::planning::TrafficLightLeftTurnWaitingZone" base_class="apollo::planning::Scenario"></class>
    <class type="apollo::planning::TrafficLightLeftTurnWaitingZoneStageApproach" base_class="apollo::planning::Stage"></class>
    <class type="apollo::planning::TrafficLightLeftTurnWaitingZoneStageCreep" base_class="apollo::planning::Stage"></class>
    <class type="apollo::planning::TrafficLightLeftTurnWaitingZoneStageIntersectionCruise" base_class="apollo::planning::Stage"></class>
   </library>
   ```
9. Add the left-turn waiting scenario to the planning scenario management pipeline.

   File path:

   ```bash
    profiles/current/modules/planning/planning_base/conf/planning_config.pb.txt
   ```

   ```bash
   topic_config {
      chassis_topic: "/apollo/canbus/chassis"
      hmi_status_topic: "/apollo/hmi/status"
      localization_topic: "/apollo/localization/pose"
      planning_pad_topic: "/apollo/planning/pad"
      planning_trajectory_topic: "/apollo/planning"
      prediction_topic: "/apollo/prediction"
      relative_map_topic: "/apollo/relative_map"
      routing_request_topic: "/apollo/external_command/lane_follow"
      routing_response_topic: "/apollo/routing_response"
      planning_command_topic: "/apollo/planning/command"
      story_telling_topic: "/apollo/storytelling"
      traffic_light_detection_topic: "/apollo/perception/traffic_light"
      planning_learning_data_topic: "/apollo/planning/learning_data"
    }
    # NO_LEARNING / E2E / HYBRID / RL_TEST / E2E_TEST / HYBRID_TEST
    learning_mode: NO_LEARNING
    reference_line_config {
      pnc_map_class: "apollo::planning::LaneFollowMap"
    }
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
          name: "TRAFFIC_LIGHT_LEFT_TURN_WAITING_ZONE"
          type: "TrafficLightLeftTurnWaitingZone"
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

10. Develop a simulation test script that controls the traffic light changes through keyboard input.

    File path:

    ```bash
    modules/planning/scenarios/traffic_light_left_turn_waiting_zone/tools/manual_traffic_light.py
    ```

    ```bash
    from cyber.python.cyber_py3 import cyber
    from cyber.python.cyber_py3 import cyber_time
    import modules.common_msgs.perception_msgs.traffic_light_detection_pb2 as traffic_light_detection_pb2
    import threading
    import time
    ```


    def add_left_light(color, traffic_light_pb):
        light = traffic_light_pb.traffic_light.add()
        light.color = color
        light.id = "451089192"
        light.tracking_time = 10.0


    def add_forward_light(color, traffic_light_pb):
        light = traffic_light_pb.traffic_light.add()
        light.color = color
        light.id = "451089193"
        light.tracking_time = 10.0


    seq_num = 0


    def add_header(msg):
        global seq_num
        msg.header.sequence_num = seq_num
        msg.header.timestamp_sec = cyber_time.Time.now().to_sec()
        msg.header.module_name = "manual_traffic_light"
        seq_num = seq_num + 1


    def pub_func(writer):
        while not cyber.is_shutdown():
            global traffic_light_msg
            add_header(traffic_light_msg)
            writer.write(traffic_light_msg)
            time.sleep(0.1)


    traffic_light_msg = None

    if __name__ == '__main__':
        traffic_light_msg = traffic_light_detection_pb2.TrafficLightDetection()
        cyber.init()
        node = cyber.Node("traffic_light_command")
        writer = node.create_writer(
            "/apollo/perception/traffic_light", traffic_light_detection_pb2.TrafficLightDetection)
        thread = threading.Thread(target=pub_func, args=(writer,))
        thread.start()
        while not cyber.is_shutdown():
            m = input(
                "1: all red 2: forward green  3: left green 4:all green\n")
            traffic_light_msg.ClearField('traffic_light')
            print(m)
            if m == '1':
                add_left_light(
                    traffic_light_detection_pb2.TrafficLight.RED, traffic_light_msg)
                add_forward_light(
                    traffic_light_detection_pb2.TrafficLight.RED, traffic_light_msg)
            elif m == "2":
                add_left_light(
                    traffic_light_detection_pb2.TrafficLight.RED, traffic_light_msg)
                add_forward_light(
                    traffic_light_detection_pb2.TrafficLight.GREEN, traffic_light_msg)
            elif m == "3":
                add_left_light(
                    traffic_light_detection_pb2.TrafficLight.GREEN, traffic_light_msg)
                add_forward_light(
                    traffic_light_detection_pb2.TrafficLight.RED, traffic_light_msg)
            elif m == "4":
                add_left_light(
                    traffic_light_detection_pb2.TrafficLight.GREEN, traffic_light_msg)
                add_forward_light(
                    traffic_light_detection_pb2.TrafficLight.GREEN, traffic_light_msg)

        cyber.shutdown()
       ```


11. Modify the BUILD file to configure build dependencies.

    - Build all the code in the package into an apollo plugin, adding all dependencies on base libraries in the code.

      ```bash
      load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
        load("//tools:apollo.bzl", "cyber_plugin_description")
        load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package", "apollo_plugin")
        load("//tools/proto:proto.bzl", "proto_library")
        load("//tools:cpplint.bzl", "cpplint")

        package(default_visibility = ["//visibility:public"])

        filegroup(
            name = "left_turn_waiting_zone_files",
            srcs = glob([
                "conf/**",
            ]),
        )

        apollo_plugin(
            name = "libtraffic_light_left_turn_waiting_zone.so",
            srcs = [
                "stage_approach.cc",
                "stage_creep.cc",
                "stage_intersection_cruise.cc",
                "traffic_light_left_turn_waiting_zone.cc",
            ],
            hdrs = [
                "stage_approach.h",
                "stage_creep.h",
                "stage_intersection_cruise.h",
                "traffic_light_left_turn_waiting_zone.h",
            ],
            copts = ["-DMODULE_NAME=\\\"planning\\\""],
            description = ":plugin_traffic_light_left_turn_waiting_zone_description.xml",
            deps = [
                "//cyber",
                "//modules/common/util:common_util",
                "//modules/common/util:util_tool",
                "//modules/common_msgs/planning_msgs:planning_cc_proto",
                "//modules/map:apollo_map",
                "//modules/planning/planning_base:apollo_planning_planning_base",
                "//modules/planning/scenarios/traffic_light_left_turn_waiting_zone/proto:traffic_light_left_turn_waiting_zone_cc_proto",
            ],
        )

        apollo_package()

        cpplint()
      ```

12. Build the developed package.

    ```bash
    buildtool build -p modules/planning/scenarios/traffic_light_left_turn_waiting_zone
    ```

13. Verify in Dreamview simulation.

    - a. Select the planning2.0 scenario set, left-turn waiting scenario.
      ![image (40).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2840%29_959df09.png)

      ![image (41).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2841%29_7b2da0d.png)

    - b. Place the vehicle on the left-turn lane at the intersection in the lower left corner of the map.

      ![image (42).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2842%29_13cf95c.png)

    - c. Open the planning module.
    - d. Start the traffic light script in the terminal so that both the straight light and the left-turn light are red.

      ```bash
      python3 modules/planning/scenarios/traffic_light_left_turn_waiting_zone/tools/manual_traffic_light.py
      ```

    - e. Send a routing request to the target lane.

      ![image (43).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2843%29_ee73fc3.png)

    - f. After the vehicle drives to the stop line, change to straight green and left-turn red.

      Type in the terminal: 2
      ![image (44).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2844%29_95ca389.png)
      Phenomenon:
      ![image (45).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2845%29_a02ad93.png)

    - g. After the vehicle drives to the stop line of the left-turn zone, change to left-turn green.

      Type in the terminal: 3
      ![image (46).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2846%29_7e71774.png)
      Phenomenon:
      ![image (47).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2847%29_20c2b5d.png)

    - h. Re-run the simulation test and observe the effect when the left-turn light in the straight-through zone is green
    - i. Re-run the simulation test and observe the effect when the left-turn light changes from green to red in the left-turn zone
