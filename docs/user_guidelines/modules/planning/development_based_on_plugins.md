# Development by Adding Plugins

You can perform secondary development of the planning module by adding plugins. The Apollo planning module uses a two-state-machine architecture to organize decision-making and motion planning tasks. In Apollo alpha, the planning module was pluginized based on the two-state-machine mechanism. At runtime, the planning module dynamically loads and runs plugins based on configuration files, enabling Lego-style development with free combination of scenarios (Scenario) and functional tasks (Task).

![image (10).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2810%29_98484f7.png)

planning base is the foundation of the planning module. It is responsible for receiving and publishing communication information with other modules, generating reference lines, and providing basic data structures and basic libraries. The first layer of the two-layer state machine is the Scenario plugin. Each Scenario plugin usually contains its own Stage plugins, and each Stage plugin orchestrates different tasks (Task) from the task library to execute.

## Secondary Development of Scenario Plugins

Scenarios can be divided by geographic location. When the planning actions of the ego vehicle in a specific area differ from the default scenario (Lane Follow), a new scenario can be developed for it to avoid affecting the operation of the default scenario. For example, a traffic light ahead requires a traffic light scenario, and a stop sign ahead requires a stop sign scenario.

Scenarios can also be divided by command. When an emergency pull over command is received, the emergency pull over scenario is entered. Likewise, when a parking command is received, the valet parking scenario is entered.

Developing a new Scenario requires inheriting from the Scenario base class:

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

### Scenario Initialization

Scenario initialization requires inheriting the `Init()` function of Scenario. The `Init` function of the scenario base class mainly loads the scenario's pipeline from the scenario plugin and saves the loaded Stage instances into `stage_pipeline_map_`. If the scenario itself also has a configuration file, you can call the `Scenario::LoadConfig<T>()` function to load the scenario's own configuration file and save it into the scenario instance context variable `context_`.

The following is an example of initializing a pull over Scenario. First, the base class's Init function is called to load the Stages contained in the Scenario. Then `Scenario::LoadConfig<ScenarioPullOverConfig>()` is called to load the pull over scenario's configuration file and save it into the pull over scenario context variable `context_`. The context variable can pass configuration and data between the Scenario and Stage.

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

### Scenario Transfer Function

The scenario transfer function is mainly called from the scenario manager to determine whether to switch into this scenario. It inherits from IsTransferable() of the Scenario base class and determines whether the current frame can switch into this scenario.

If it returns true, the scenario is entered and the subsequent scenarios are no longer evaluated. Scenario entry includes a geographic location-triggered approach, such as the stop sign scenario:
Get the first overlap from the reference line and check whether it is a stop sign; if so, and the distance from the ego vehicle to the stop sign meets the set distance, the stop sign scenario is entered.

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

Another way to enter a scenario is command-triggered. For example, for the emergency pull over scenario, the entry condition is receiving a `pad_msg` command, upon which the emergency pull over scenario is entered.

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

### Scenario Enter and Exit Functions

The scenario enter function inherits from the base class's `Enter()` function. It is called before first entering the scenario to do some preprocessing and reset variables within the scenario.
For example, in the `Enter()` function of the stop sign scenario, the stop sign id of the reference line is first found and saved into the context variable, and then the global variable of the stop sign is reset.

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

The scenario exit function inherits from the base class's `Exit()` function. It is called when switching out of the scenario and can be used to clear some global variables, such as the exit function of the stop sign scenario.

```bash
bool StopSignUnprotectedScenario::Exit(Frame* frame) {
  injector_->planning_context()
      ->mutable_planning_status()
      ->mutable_stop_sign()
      ->Clear();
  return true;
}
```

### Scenario Process Function

The scenario process function inherits from the base class's `Process()` function and is called every frame while the scenario is running. The base class's Process is mainly used to create Stages, run the Process function of a Stage, and schedule switching between different Stages.

Scenarios in Apollo generally use the base class's `Process()` function by default. If you have more customization needs, you can inherit the `Process` function and rewrite the business strategy.

## Secondary Development of Stages

A Stage in Apollo is the second-layer state machine under a Scenario and can be divided by time. When the business logic in a scenario has a sequential order, it can be divided into multiple Stages. For example, the unprotected left turn at a traffic light scenario can be divided into three stages: the first is the process of approaching the traffic light stop line, the second is the process of creeping slowly and observing when the traffic light is green, and the third is the process of passing quickly when the oncoming straight lane is clear.

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

After the stages are divided, tasks (Task) need to be configured for each stage. In each frame of planning, only one stage runs, and within a stage each task is executed in order. A task is the smallest computing unit that handles path planning or speed planning. You can reuse existing tasks in Apollo for configuration, or develop new planning tasks according to [Secondary Development of Tasks](#secondary-development-of-tasks). For the configuration of tasks and task parameters, refer to the chapter [Development by Configuration Parameters](Apollo_alpha_doc/使用指南/核心模块/规划能力/开发模式/通过配置参数开发.md) for details.

### Stage Initialization

When the scenario runs, the stage instances contained in the scenario are created and the stages are initialized. If you need to initialize a stage, you can inherit and override the stage's `Init()` function. The base class `Init()` function is mainly used to load the task pipeline and initialize the tasks. Apollo generally uses the base class's `Init` function.

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

### Stage Execution

The stage's run function `Process()` is mainly used to maintain the state of the current stage. A stage has three states: RUNNING, FINISHED, and ERROR. Among them:

- RUNNING: indicates the current stage is running, and the Scenario will keep the current stage running;
- FINISHED: indicates the current stage is complete, and the Scenario will switch into the next stage;
- ERROR: indicates a serious fault in the current planning. The Scenario will report it, and the ego vehicle will brake to a stop.

If the current stage is complete, in addition to returning FINISHED for the current stage, you also need to specify the name of the next stage, `next_stage_`, and the Scenario will switch into the next stage according to the current stage's `next_stage`. If the specified `next_stage_` is an empty string, the Scenario considers all Stages to have finished running, and the Scenario also returns the finished state.

To call tasks in turn within a Stage, two base class functions can be reused: `ExecuteTaskOnReferenceLine`, mainly used to run a series of tasks on the main road, and `ExecuteTaskOnOpenSpace`, mainly used to run a series of tasks in open space.

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

The following takes the stage of approaching the traffic light stop line in the unprotected left turn scenario as an example to describe the stage's running process.

First, parse the scenario context data `TrafficLightUnprotectedLeftTurnContext` through `GetContextAs`.

If the current stage is not enabled, skip it,
set the cruise speed of the current stage to the configured speed `approach_cruise_speed`,
execute each planning task in turn with `ExecuteTaskOnReferenceLine()`,
check whether the traffic lights of the current lane are all green, and if so, enter the next Stage,
and if no traffic light can be found, exit the current scenario.

```bash
StageResult TrafficLightUnprotectedLeftTurnStageApproach::Process(
    const TrajectoryPoint& planning_init_point, Frame* frame) {
  auto context = GetContextAs<TrafficLightUnprotectedLeftTurnContext>();
  const ScenarioTrafficLightUnprotectedLeftTurnConfig& scenario_config =
      context->scenario_config;
  if (!pipeline_config_.enabled()) {
    return FinishStage(frame);
  }

  // set cruise_speed to slow down
  frame->mutable_reference_line_info()->front().LimitCruiseSpeed(
      scenario_config.approach_cruise_speed());

  StageResult result = ExecuteTaskOnReferenceLine(planning_init_point, frame);
  if (result.HasError()) {
    AERROR << "TrafficLightUnprotectedLeftTurnStageApproach planning error";
  }

  if (context->current_traffic_light_overlap_ids.empty()) {
    return FinishScenario();
  }

  const auto& reference_line_info = frame->reference_line_info().front();

  const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();

  PathOverlap* traffic_light = nullptr;
  bool traffic_light_all_done = true;
  for (const auto& traffic_light_overlap_id :
       context->current_traffic_light_overlap_ids) {
    // get overlap along reference line
    PathOverlap* current_traffic_light_overlap =
        reference_line_info.GetOverlapOnReferenceLine(
            traffic_light_overlap_id, ReferenceLineInfo::SIGNAL);
    if (!current_traffic_light_overlap) {
      continue;
    }

    traffic_light = current_traffic_light_overlap;

    // set right_of_way_status
    reference_line_info.SetJunctionRightOfWay(
        current_traffic_light_overlap->start_s, false);

    const double distance_adc_to_stop_line =
        current_traffic_light_overlap->start_s - adc_front_edge_s;
    auto signal_color = frame->GetSignal(traffic_light_overlap_id).color();
    if (distance_adc_to_stop_line < 0) return FinishStage(frame);
    // check on traffic light color and distance to stop line
    if (signal_color != TrafficLight::GREEN ||
        distance_adc_to_stop_line >=
            scenario_config.max_valid_stop_distance()) {
      traffic_light_all_done = false;
      break;
    }
  }

  if (traffic_light == nullptr) {
    return FinishScenario();
  }

  if (traffic_light_all_done) {
    return FinishStage(frame);
  }

  return result.SetStageStatus(StageStatusType::RUNNING);
}
```

## Secondary Development of Tasks

When the tasks (Task) in Apollo cannot meet the needs of your scenario, you need to develop a brand-new task plugin. There are several types of Task base classes in Apollo:

- PathGeneration: mainly used to generate paths on the main road, such as planning the lane borrow path LaneBorrowPath, the pull over path PullOverPath, and the lane follow path LaneFollowPath,
- SpeedOptimizer: mainly used to plan speed profiles on the main road, such as speed planning based on quadratic programming and speed planning based on nonlinear programming,
- TrajectoryOptimizer: mainly used to generate trajectories, such as the open space planner OpenSpaceTrajectoryProvider.

You can also inherit from the Task base class to implement your own task.

### Task Initialization

Before a Stage runs a task for the first time, it calls the task's `Init` function to initialize it. The base class's `Init` function is mainly used to obtain the task's default parameter path `default_config_path_` and the scenario task parameter path `config_path_`.

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

Then the template function `Task::LoadConfig<T>(&config_)` can be called to load the task parameters. Take the lane borrow path generation task `LaneBorrowPath` as an example: first call `Task::Init` to get the task parameter path, then call `Task::LoadConfig<LaneBorrowPathConfig>()` to load the parameters of the lane borrow task.

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

The task's default parameters are saved in `default_conf.pb.txt` under the task's `conf` directory; for example, the parameters of the lane borrow path task are in `modules/planning/tasks/lane_borrow_path/conf/default_conf.pb.txt`.
The task's configuration parameters are defined in proto format and saved in the task's `proto` directory; for example, the proto of the lane borrow path task is in `modules/planning/tasks/lane_borrow_path/proto`.

### Task Execution

The task's run function mainly inherits the base class's `Process` function or `Execute` function. The inputs of the run function are usually `frame` and `reference_line_info`, and the outputs are also saved into `frame` and `reference_line_info`.

For example, the lane borrow path task saves the generated path into `reference_line_info->mutable_path_data()`.

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

## Secondary Development of Traffic Rule Plugins

The traffic rule plugin TrafficRule mainly processes traffic rules before the planning module executes a Scenario. When you need to add new decision logic that takes effect in all scenarios, you can develop a new traffic rule plugin.

A traffic rule plugin inherits from the traffic rule base class, and the plugins are then generated and called by the `traffic_decider` in `planning_base`. Each time planning performs a planning task, it calls each traffic rule through `traffic_decider`, which makes the traffic rule plugins take effect.

```bash
Status TrafficDecider::Execute(Frame *frame,
                               ReferenceLineInfo *reference_line_info) {
  CHECK_NOTNULL(frame);
  CHECK_NOTNULL(reference_line_info);

  for (const auto &rule : rule_list_) {
    if (!rule) {
      AERROR << "Could not find rule ";
      continue;
    }
    rule->Reset();
    rule->ApplyRule(frame, reference_line_info);
    ADEBUG << "Applied rule " << rule->Getname();
  }

  BuildPlanningTarget(reference_line_info);
  return Status::OK();
}
```

### Traffic Rule Plugin Initialization

Traffic rule plugin initialization mainly loads the plugin's configuration parameters. Calling the `TrafficRule::Init()` function obtains the path of the plugin configuration parameter file, and the template function `TrafficRule::LoadConfig()` can save the contents of the plugin configuration parameter file into `config_`. For example, the crosswalk plugin loads the configuration parameters in `modules/planning/traffic_rules/crosswalk/conf/default_conf.pb.txt`.

```bash
bool Crosswalk::Init(const std::string& name,
                     const std::shared_ptr<DependencyInjector>& injector) {
  if (!TrafficRule::Init(name, injector)) {
    return false;
  }
  // Load the config this task.
  return TrafficRule::LoadConfig<CrosswalkConfig>(&config_);
}
```

### Traffic Rule Plugin Execution

In each cycle, before the planning module runs the scenario, it first runs the `ApplyRule()` function of each traffic rule plugin. Therefore, we need to implement the `ApplyRule` function and save all decisions or constraints in `reference_line_info`. For example, the crosswalk plugin checks the state of obstacles around the crosswalk and applies a stop decision before the crosswalk in `reference_line_info`.

```bash
Status Crosswalk::ApplyRule(Frame* const frame,
                            ReferenceLineInfo* const reference_line_info) {
  CHECK_NOTNULL(frame);
  CHECK_NOTNULL(reference_line_info);

  if (!FindCrosswalks(reference_line_info)) {
    injector_->planning_context()->mutable_planning_status()->clear_crosswalk();
    return Status::OK();
  }

  MakeDecisions(frame, reference_line_info);
  return Status::OK();
}
```
