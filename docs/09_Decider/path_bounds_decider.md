# Path Bounds Decider

### *Table of Contents*

- [Overview](#overview)
- [Path bounds decider code and corresponding versions](#path-bounds-decider-code-and-corresponding-versions)
  + [Class relationships](#class-relationships)
  + [Path bounds decider data](#path-bounds-decider-data)
- [Path bounds decider code flow and framework](#path-bounds-decider-code-flow-and-framework)
- [Path bounds decider algorithm analysis](#path-bounds-decider-algorithm-analysis)
  + [1.fallback](#1.fallback)
  + [2.pull over](#2.pull-over)
  + [3.lane change](#3.lane-change)
  + [4.Regular](#4.Regular)

# Overview

The `path bounds decider` is a task of the planning module and belongs to the decider category of tasks.

The overall motion flow chart of the planning module is as follows:

![Overall flow chart](images/lane_follow.png)

The overall flow chart is explained using the [lane follow](https://github.com/ApolloAuto/apollo/tree/master/modules/planning/scenarios/lane_follow) scenario as an example. Only the main flow is described here, not every detail. The main functionality of a task is in the `Process` function.

First, the entry function of the planning module is PlanningComponent's [Proc](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planning_component/planning_component.cc#L118).

Second, with OnLanePlanning as the planning mode, [RunOnce](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planning_component/on_lane_planning.cc#L231) is executed. In RunOnce, traffic rules are executed first, and then the trajectory is planned. The function that plans the trajectory is [Plan](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planning_component/on_lane_planning.cc#L487).

Third, execution enters the [Plan](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planners/public_road/public_road_planner.cc#L33) function in PublicRoadPlanner to plan the trajectory. The [Update](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planners/public_road/scenario_manager.cc#L56) function of ScenarioManager selects the appropriate scenario according to the current scenario_type. The flow chart here uses lane follow as an example.

Fourth, after the lane follow scenario is selected, the [Process](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planning_interface_base/scenario_base/scenario.cc#L86) function is executed. Then the [Process](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/scenarios/lane_follow/lane_follow_stage.cc#L93) function in LaneFollowStage is executed, and all the tasks in LaneFollowStage are executed in [PlanOnReferenceLine](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/scenarios/lane_follow/lane_follow_stage.cc#L136). Tasks are executed by calling the [Excute](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/scenarios/lane_follow/lane_follow_stage.cc#L154) function, which calls the task's [Process](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/tasks/lane_follow_path/lane_follow_path.cc#L47) function (taking lane_follow_path as an example). In the last figure, TaskType does not refer to a specific class name; it represents all task types. The dashed arrows indicate that all tasks in LaneFollowStage are executed in the order of the vector.

Finally, the task flow is all in the Process function. The later explanations of tasks all start from the Process function.

# Path bounds decider code and corresponding versions

This section describes the path_bounds_decider task.

Please refer to [Apollo r6.0.0 path_bounds_decider](https://github.com/ApolloAuto/apollo/tree/r6.0.0/modules/planning/tasks/deciders/path_bounds_decider)

## Class relationships

![path_bounds_decider_task](images/task.png)

### (1) Inheritance

① The `PathBoundsDecider` class inherits from the `Decider` class and implements the `Process` method. The main execution of the path bounds decision is in the `process` method.
```C++
// modules/planning/tasks/deciders/path_bounds_decider/path_bounds_decider.h
class PathBoundsDecider : public Decider {
... };
```

② The `Decider` class inherits from the `Task` class and implements the Excute method, which mainly assigns values to two variables, `frame` and `reference_line_info`, and executes the **Process** method. This corresponds to the **Process** method above
```C++
// modules/planning/tasks/deciders/decider.h
class Decider : public Task {
... };

// modules/planning/tasks/deciders/decider.cc
apollo::common::Status Decider::Execute(
    Frame* frame, ReferenceLineInfo* reference_line_info) {
  Task::Execute(frame, reference_line_info);
  return Process(frame, reference_line_info);
}

apollo::common::Status Decider::Execute(Frame* frame) {
  Task::Execute(frame);
  return Process(frame);
}
```

③ The `Task` class defines protected variables, which are the inputs of the path bounds decision
```C++
// modules/planning/tasks/task.h
class Task {
 public:
  // Virtual method, mainly used to assign values to frame and reference_line_info
  virtual common::Status Execute(Frame* frame,
                                 ReferenceLineInfo* reference_line_info);
  virtual common::Status Execute(Frame* frame);

 protected:
  // frame and reference_line_info variables
  Frame* frame_ = nullptr;
  ReferenceLineInfo* reference_line_info_ = nullptr;

  // Configuration and name
  TaskConfig config_;
  std::string name_;
... };
```

### (2) Invocation

Mainly describes how a task is created and invoked in a stage

① The `TaskFactory` class registers all tasks, including decider, optimizer and other (E2E tasks). Factory pattern
```C++
// modules/planning/tasks/task_factory.h
class TaskFactory {
 public:
  // Both functions are static
  static void Init(...);    // In the initialization function, register all tasks
  static std::unique_ptr<Task> CreateTask(...);    // Create an instance of a specific task and return a pointer to it
... };
```

② Creation and execution of tasks in a stage

- Creation: in the stage's constructor, tasks are created according to the stage configuration, and the pointers are put into task_ and task_list_
- Usage: in a specific stage, override the Process method. Calling the Process method in turn calls the ExecuteTask* method (ExecuteTaskOnReferenceLine), which finally calls the corresponding task's Process method

```C++
// modules/planning/scenarios/stage.h
class Stage {
 // In the constructor, create tasks according to the stage configuration
 Stage(const ScenarioConfig::StageConfig& config,
        const std::shared_ptr<DependencyInjector>& injector);
 public:

 // Pure virtual function, left for concrete stages to implement; different stages have different implementation logic
 virtual StageStatus Process(
      const common::TrajectoryPoint& planning_init_point, Frame* frame) = 0;
 protected:
  // Three functions that execute tasks; each of them calls the task's Excute method, which further calls the specific task's Process method
  bool ExecuteTaskOnReferenceLine(
      const common::TrajectoryPoint& planning_start_point, Frame* frame);

  bool ExecuteTaskOnReferenceLineForOnlineLearning(
      const common::TrajectoryPoint& planning_start_point, Frame* frame);

  bool ExecuteTaskOnOpenSpace(Frame* frame);

 protected:
  // Map of tasks; the key is TaskType and the value is a pointer to Task
  std::map<TaskConfig::TaskType, std::unique_ptr<Task>> tasks_;
  // Saved task list
  std::vector<Task*> task_list_;
  // Stage configuration
  ScenarioConfig::StageConfig config_;
...};
```

## Path bounds decider data

The main inputs and outputs, data structures and variable settings of the `PathBoundsDecider` class.


### (1) Inputs and outputs

① There are two inputs: `frame` and `reference_line_info`

- **frame**

frame contains all the data required for one planning cycle
```C++
// modules/planning/common/frame.h
class Frame {
 private:
  static DrivingAction pad_msg_driving_action_;
  uint32_t sequence_num_ = 0;

  /* Local_view is a struct containing the following information
  // modules/planning/common/local_view.h
  struct LocalView {
    std::shared_ptr<prediction::PredictionObstacles> prediction_obstacles;
    std::shared_ptr<canbus::Chassis> chassis;
    std::shared_ptr<localization::LocalizationEstimate> localization_estimate;
    std::shared_ptr<perception::TrafficLightDetection> traffic_light;
    std::shared_ptr<routing::RoutingResponse> routing;
    std::shared_ptr<relative_map::MapMsg> relative_map;
    std::shared_ptr<PadMessage> pad_msg;
    std::shared_ptr<storytelling::Stories> stories;
  };
  */
  LocalView local_view_;
  // HD map
  const hdmap::HDMap *hdmap_ = nullptr;
  common::TrajectoryPoint planning_start_point_;
  // Vehicle state
  // modules/common/vehicle_state/proto/vehicle_state.proto
  common::VehicleState vehicle_state_;
  // Reference line information
  std::list<ReferenceLineInfo> reference_line_info_;

  bool is_near_destination_ = false;

  /**
   * the reference line info that the vehicle finally choose to drive on
   **/
  const ReferenceLineInfo *drive_reference_line_info_ = nullptr;

  ThreadSafeIndexedObstacles obstacles_;

  std::unordered_map<std::string, const perception::TrafficLight *>
      traffic_lights_;

  // current frame published trajectory
  ADCTrajectory current_frame_planned_trajectory_;

  // current frame path for future possible speed fallback
  DiscretizedPath current_frame_planned_path_;

  const ReferenceLineProvider *reference_line_provider_ = nullptr;

  OpenSpaceInfo open_space_info_;

  std::vector<routing::LaneWaypoint> future_route_waypoints_;

  common::monitor::MonitorLogBuffer monitor_logger_buffer_;
};
```

- **reference_line_info**

reference_line_info contains all the data about the reference line

```C++
// modules/planning/common/reference_line_info.h

class ReferenceLineInfo {
 ...
 private:
  static std::unordered_map<std::string, bool> junction_right_of_way_map_;
  const common::VehicleState vehicle_state_;  // Vehicle state
  const common::TrajectoryPoint adc_planning_point_;  // TrajectoryPoint is defined in modules/common/proto/pnc_point.proto

  /* Reference line: a smoothed trajectory based on the road center line, extending 80 m backward and 130 m forward.
  class ReferenceLine {
  ...
  private:
  struct SpeedLimit {
    double start_s = 0.0;
    double end_s = 0.0;
    double speed_limit = 0.0;  // unit m/s
    ...};
  
  // This speed limit overrides the lane speed limit
  std::vector<SpeedLimit> speed_limit_;
  std::vector<ReferencePoint> reference_points_;  // ReferencePoint contains the information (k, dk, x, y, heading, s, l)
  hdmap::Path map_path_;
  uint32_t priority_ = 0;
  };
  */
  ReferenceLine reference_line_;

  /**
   * @brief this is the number that measures the goodness of this reference
   * line. The lower the better.
   */
  // Cost function; the lower the value, the better
  double cost_ = 0.0;

  bool is_drivable_ = true;
  // PathDecision contains the decisions for all obstacles on a path, of two kinds: lateral (Nudge, Ignore) and longitudinal (Stop, Yield, Follow, Overtake, Ignore)
  PathDecision path_decision_;
  // Pointer
  Obstacle* blocking_obstacle_;

  /* Path bounds; the result is saved in this variable, through the **SetCandidatePathBoundaries** method
    // modules/planning/common/path_boundary.h
    class PathBoundary {
    ...
      private:
     double start_s_ = 0.0;
     double delta_s_ = 0.0;
     std::vector<std::pair<double, double>> boundary_;
     std::string label_ = "regular";
     std::string blocking_obstacle_id_ = "";
  };
  */
  std::vector<PathBoundary> candidate_path_boundaries_;
  // PathData class, containing mutual conversion between the XY coordinate system and the SL coordinate system
  std::vector<PathData> candidate_path_data_;

  PathData path_data_;
  PathData fallback_path_data_;
  SpeedData speed_data_;

  DiscretizedTrajectory discretized_trajectory_;

  RSSInfo rss_info_;

  /**
   * @brief SL boundary of stitching point (starting point of plan trajectory)
   * relative to the reference line
   */
  SLBoundary adc_sl_boundary_;
... };
```
② Output:
```C++
Status PathBoundsDecider::Process(
    Frame* const frame, ReferenceLineInfo* const reference_line_info)
```

The Process function definition; the final result is saved into `reference_line_info`

### (2) Parameter settings

```C++
// modules/planning/tasks/deciders/path_bounds_decider/path_bounds_decider.cc
// Distance in the s direction
constexpr double kPathBoundsDeciderHorizon = 100.0;
// Interval in the s direction
constexpr double kPathBoundsDeciderResolution = 0.5;

// Lane width
constexpr double kDefaultLaneWidth = 5.0;
// Road
constexpr double kDefaultRoadWidth = 20.0;

// TODO(all): Update extra tail point base on vehicle speed.
constexpr int kNumExtraTailBoundPoint = 20;
constexpr double kPulloverLonSearchCoeff = 1.5;
constexpr double kPulloverLatSearchCoeff = 1.25;
```

### (3) Data structures
```C++
// modules/planning/tasks/deciders/path_bounds_decider/path_bounds_decider.cc
namespace {
// PathBoundPoint contains: (s, l_min, l_max). A path bound point
using PathBoundPoint = std::tuple<double, double, double>;
// PathBound contains a vector of PathBoundPoints. A path bound
using PathBound = std::vector<PathBoundPoint>;
// ObstacleEdge contains: (is_start_s, s, l_min, l_max, obstacle_id). An obstacle edge
using ObstacleEdge = std::tuple<int, double, double, double, std::string>;
}  // namespace
```

# Path bounds decider code flow and framework

Fig.2 is the flow chart of the path bounds decision.

![path_bounds_decider](images/path_bounds_decider.png)

In the **Process** method, the path bounds are computed for four scenarios, in processing order: fallback, pull-over, lane-change and regular.
The regular scenario is further divided into LEFT_BORROW, NO_BORROW and RIGHT_BORROW depending on whether a lane is borrowed.

The path bounds of the fallback scenario are always generated; the other three depend on the situation and each needs an if check.

# Path bounds decider algorithm analysis

## 1.fallback

![fallback](images/fallback.png)


The generation process of the fallback scenario is shown in the figure above.
Fallback considers only the adc information and static road information, and mainly calls two functions


- InitPathBoundary
```C++
bool PathBoundsDecider::InitPathBoundary(
  ...
  // Starting from ADC's current position, increment until the horizon, and
  // set lateral bounds to be infinite at every spot.
  // Starting from the adc's current position, take points at 0.5 m intervals up to the end point, and set the [left, right] bounds to double's [lowest, max]
  for (double curr_s = adc_frenet_s_;
       curr_s < std::fmin(adc_frenet_s_ +
                              std::fmax(kPathBoundsDeciderHorizon,
                                        reference_line_info.GetCruiseSpeed() *
                                            FLAGS_trajectory_time_length),
                          reference_line.Length());
       curr_s += kPathBoundsDeciderResolution) {
    path_bound->emplace_back(curr_s, std::numeric_limits<double>::lowest(),
                             std::numeric_limits<double>::max());
  }
...}
```

- GetBoundaryFromLanesAndADC
```C++
// TODO(jiacheng): this function is to be retired soon.
bool PathBoundsDecider::GetBoundaryFromLanesAndADC(
  ...
  for (size_t i = 0; i < path_bound->size(); ++i) {
    double curr_s = std::get<0>((*path_bound)[i]);
    // 1. Get the current lane width at current point.
    if (!reference_line.GetLaneWidth(curr_s, &curr_lane_left_width,
                                     &curr_lane_right_width)) {
      AWARN << "Failed to get lane width at s = " << curr_s;
      curr_lane_left_width = past_lane_left_width;
      curr_lane_right_width = past_lane_right_width;
    } else {...}

    // 2. Get the neighbor lane widths at the current point.
    double curr_neighbor_lane_width = 0.0;
    if (CheckLaneBoundaryType(reference_line_info, curr_s, lane_borrow_info)) {
      hdmap::Id neighbor_lane_id;
      if (lane_borrow_info == LaneBorrowInfo::LEFT_BORROW) {
        // Borrow the left lane
        ...
      } else if (lane_borrow_info == LaneBorrowInfo::RIGHT_BORROW) {
        // Borrow the right lane
        ...
      }
    }

    // 3. Compute suitable bounds according to the road width and the adc's position and speed.
    static constexpr double kMaxLateralAccelerations = 1.5;
    double offset_to_map = 0.0;
    reference_line.GetOffsetToMap(curr_s, &offset_to_map);

    double ADC_speed_buffer = (adc_frenet_ld_ > 0 ? 1.0 : -1.0) *
                              adc_frenet_ld_ * adc_frenet_ld_ /
                              kMaxLateralAccelerations / 2.0;
    // If the left lane is borrowed, the left bound becomes the left bound of the left-side lane
    double curr_left_bound_lane =
        curr_lane_left_width + (lane_borrow_info == LaneBorrowInfo::LEFT_BORROW
                                    ? curr_neighbor_lane_width
                                    : 0.0);
    // Similar to the above
    double curr_right_bound_lane =
        -curr_lane_right_width -
        (lane_borrow_info == LaneBorrowInfo::RIGHT_BORROW
             ? curr_neighbor_lane_width
             : 0.0);

    double curr_left_bound = 0.0;  // Left bound
    double curr_right_bound = 0.0;  // Right bound
    // Compute the left bound and the right bound
    if (config_.path_bounds_decider_config()
            .is_extend_lane_bounds_to_include_adc() ||
        is_fallback_lanechange) {
      // extend path bounds to include ADC in fallback or change lane path
      // bounds.
      double curr_left_bound_adc =
          std::fmax(adc_l_to_lane_center_,
                    adc_l_to_lane_center_ + ADC_speed_buffer) +
          GetBufferBetweenADCCenterAndEdge() + ADC_buffer;
      curr_left_bound =
          std::fmax(curr_left_bound_lane, curr_left_bound_adc) - offset_to_map;

      double curr_right_bound_adc =
          std::fmin(adc_l_to_lane_center_,
                    adc_l_to_lane_center_ + ADC_speed_buffer) -
          GetBufferBetweenADCCenterAndEdge() - ADC_buffer;
      curr_right_bound =
          std::fmin(curr_right_bound_lane, curr_right_bound_adc) -
          offset_to_map;
    } else {
      curr_left_bound = curr_left_bound_lane - offset_to_map;
      curr_right_bound = curr_right_bound_lane - offset_to_map;
    }

    // 4. Update the boundaries.
    if (!UpdatePathBoundaryWithBuffer(i, curr_left_bound, curr_right_bound,
                                      path_bound, is_left_lane_boundary,
                                      is_right_lane_boundary)) {
      path_blocked_idx = static_cast<int>(i);
    }
... }
```

## 2.pull over

![pull_over](images/pull_over.png)

### (1) GetBoundaryFromRoads
Unlike `GetBoundaryFromLanesAndADC`, the `GetBoundaryFromRoads` function computes bounds from road information:
- Get the reference line information
- For the points on the path, compute point by point
    + Bounds
    + Update


### (2) GetBoundaryFromStaticObstacles
Adjust the bounds according to obstacle vehicles:
- Compute the coordinates of the obstacle vehicles in the Frenet coordinate system
- Sort the scan lines, scanning in the S direction
    + Only focus on obstacles within the path bounds
    + Only focus on obstacles in front of the adc
    + Decompose each obstacle into two boundaries, start and end
- Map obstacle IDs
    + True if the adc can pass on the left, otherwise False
- Check the obstacles on the path point by point
    + According to newly arriving obstacles
    + According to existing obstacles

### (3) SearchPullOverPosition

The process of searching for a pull over position:
- Based on pull_over_status.pull_over_type(), determine whether to search forward (the first point at the start of the pull over) or backward (the point after the end of the pull over)
- Two nested loops: the outer loop controls the search index idx, and the inner loop controls the further index (forward idx+1, backward idx-1).
- Based on the indices of the inner and outer loops, determine whether the space found meets the width and length requirements, and whether pull over is possible

The code is as follows:

```C++
bool PathBoundsDecider::SearchPullOverPosition(
    const Frame& frame, const ReferenceLineInfo& reference_line_info,
    const std::vector<std::tuple<double, double, double>>& path_bound,
    std::tuple<double, double, double, int>* const pull_over_configuration) {
  const auto& pull_over_status =
      injector_->planning_context()->planning_status().pull_over();

  // Search direction; forward search by default
  bool search_backward = false;  // search FORWARD by default

  double pull_over_s = 0.0;
  if (pull_over_status.pull_over_type() ==
      PullOverStatus::EMERGENCY_PULL_OVER) {...}

  int idx = 0;
  if (search_backward) {
    // Backward search: locate the point at the end of the pull over.
    idx = static_cast<int>(path_bound.size()) - 1;
    while (idx >= 0 && std::get<0>(path_bound[idx]) > pull_over_s) {
      --idx;
    }
  } else {
    // Forward search: locate the first point after the start of the emergency pull over.
    while (idx < static_cast<int>(path_bound.size()) &&
           std::get<0>(path_bound[idx]) < pull_over_s) {
      ++idx;
    }
  }
  
  // Search for a feasible position for the pull over, mainly determining the width and length of the region
  const double pull_over_space_length =
      kPulloverLonSearchCoeff *
          VehicleConfigHelper::GetConfig().vehicle_param().length() -
      FLAGS_obstacle_lon_start_buffer - FLAGS_obstacle_lon_end_buffer;
  const double pull_over_space_width =
      (kPulloverLatSearchCoeff - 1.0) *
      VehicleConfigHelper::GetConfig().vehicle_param().width();
  const double adc_half_width =
      VehicleConfigHelper::GetConfig().vehicle_param().width() / 2.0;

  // 2. Find a window that is close to road-edge.
  /*
    Two nested loops are used for the search here; the outer loop controls the starting end point idx of the search.
    The inner loop controls the other end point. Based on the two end points found, determine whether the region allows a pull over
  */
  bool has_a_feasible_window = false;
  while ((search_backward && idx >= 0 &&
          std::get<0>(path_bound[idx]) - std::get<0>(path_bound.front()) >
              pull_over_space_length) ||
         (!search_backward && idx < static_cast<int>(path_bound.size()) &&
          std::get<0>(path_bound.back()) - std::get<0>(path_bound[idx]) >
              pull_over_space_length)) {

    while ((search_backward && j >= 0 &&
            std::get<0>(path_bound[idx]) - std::get<0>(path_bound[j]) <
                pull_over_space_length) ||
           (!search_backward && j < static_cast<int>(path_bound.size()) &&
            std::get<0>(path_bound[j]) - std::get<0>(path_bound[idx]) <
                pull_over_space_length)) {...}
    
    // Found a feasible region; get the position and pose of the parking area
    if (is_feasible_window) {
    ...
    break;}
    ...}  // outer while
...
}
```

## 3.lane change

![lane_change](images/lane_change.png)

The code flow is as follows:


```C++
Status PathBoundsDecider::GenerateLaneChangePathBound(
    const ReferenceLineInfo& reference_line_info,
    std::vector<std::tuple<double, double, double>>* const path_bound) {
  // 1. Initialization, similar to the previous steps
  if (!InitPathBoundary(reference_line_info, path_bound)) {...}


  // 2. Get a rough path bound based on the road and adc information
  std::string dummy_borrow_lane_type;
  if (!GetBoundaryFromLanesAndADC(reference_line_info,
                                  LaneBorrowInfo::NO_BORROW, 0.1, path_bound,
                                  &dummy_borrow_lane_type, true)) {...}
 

  // 3. Remove the S-length of target lane out of the path-bound.
  GetBoundaryFromLaneChangeForbiddenZone(reference_line_info, path_bound);


  // Adjust the bounds according to static obstacles.
  if (!GetBoundaryFromStaticObstacles(reference_line_info.path_decision(),
                                      path_bound, &blocking_obstacle_id)) {...}
  ...
}
```

The GetBoundaryFromLaneChangeForbiddenZone function is an important function for lane change. Its operation is as follows:
- If lane change is possible at the current position, change lanes directly
- If there is a lane-change starting point, use it directly
- Check the bounds of the points before the lane change one by one, and change the bound values (return if the lane-change point has already been passed)

```C++
void PathBoundsDecider::GetBoundaryFromLaneChangeForbiddenZone(
    const ReferenceLineInfo& reference_line_info, PathBound* const path_bound) {


  // 1. Change lanes directly at the current position.
  auto* lane_change_status = injector_->planning_context()
                                 ->mutable_planning_status()
                                 ->mutable_change_lane();
  if (lane_change_status->is_clear_to_change_lane()) {
    ADEBUG << "Current position is clear to change lane. No need prep s.";
    lane_change_status->set_exist_lane_change_start_position(false);
    return;
  }


  // 2. If there is already a lane-change starting point, use it directly; otherwise find another
  double lane_change_start_s = 0.0;
  if (lane_change_status->exist_lane_change_start_position()) {
    common::SLPoint point_sl;
    reference_line.XYToSL(lane_change_status->lane_change_start_position(),
                          &point_sl);
    lane_change_start_s = point_sl.s();
  } else {
    // TODO(jiacheng): train ML model to learn this.
    // Set a point a certain distance in front of the adc as the lane-change starting point
    lane_change_start_s = FLAGS_lane_change_prepare_length + adc_frenet_s_;

    // Update the decided lane_change_start_s into planning-context.
    // Update the information of the lane-change starting point
    common::SLPoint lane_change_start_sl;
    lane_change_start_sl.set_s(lane_change_start_s);
    lane_change_start_sl.set_l(0.0);
    common::math::Vec2d lane_change_start_xy;
    reference_line.SLToXY(lane_change_start_sl, &lane_change_start_xy);
    lane_change_status->set_exist_lane_change_start_position(true);
    lane_change_status->mutable_lane_change_start_position()->set_x(
        lane_change_start_xy.x());
    lane_change_status->mutable_lane_change_start_position()->set_y(
        lane_change_start_xy.y());
  }

  // Remove the target lane out of the path-boundary, up to the decided S.
  // Check the bounds of the points before the lane change one by one, and change the bound values
  for (size_t i = 0; i < path_bound->size(); ++i) {
    double curr_s = std::get<0>((*path_bound)[i]);
    if (curr_s > lane_change_start_s) {
      break;
    }
    double curr_lane_left_width = 0.0;
    double curr_lane_right_width = 0.0;
    double offset_to_map = 0.0;
    reference_line.GetOffsetToMap(curr_s, &offset_to_map);
    if (reference_line.GetLaneWidth(curr_s, &curr_lane_left_width,
                                    &curr_lane_right_width)) {
      double offset_to_lane_center = 0.0;
      reference_line.GetOffsetToMap(curr_s, &offset_to_lane_center);
      curr_lane_left_width += offset_to_lane_center;
      curr_lane_right_width -= offset_to_lane_center;
    }
    curr_lane_left_width -= offset_to_map;
    curr_lane_right_width += offset_to_map;

    std::get<1>((*path_bound)[i]) =
        adc_frenet_l_ > curr_lane_left_width
            ? curr_lane_left_width + GetBufferBetweenADCCenterAndEdge()
            : std::get<1>((*path_bound)[i]);
    std::get<1>((*path_bound)[i]) =
        std::fmin(std::get<1>((*path_bound)[i]), adc_frenet_l_ - 0.1);
    std::get<2>((*path_bound)[i]) =
        adc_frenet_l_ < -curr_lane_right_width
            ? -curr_lane_right_width - GetBufferBetweenADCCenterAndEdge()
            : std::get<2>((*path_bound)[i]);
    std::get<2>((*path_bound)[i]) =
        std::fmax(std::get<2>((*path_bound)[i]), adc_frenet_l_ + 0.1);
  }
}
```

## 4.Regular

![lane_change](images/regular.png)

The code flow is as follows:

```C++
Status PathBoundsDecider::GenerateRegularPathBound(
    const ReferenceLineInfo& reference_line_info,
    const LaneBorrowInfo& lane_borrow_info, PathBound* const path_bound,
    std::string* const blocking_obstacle_id,
    std::string* const borrow_lane_type) {
  // 1. Initialize the bounds.
  if (!InitPathBoundary(reference_line_info, path_bound)) {...}


  // 2. Determine the rough bounds according to the adc position and lane information
  if (!GetBoundaryFromLanesAndADC(reference_line_info, lane_borrow_info, 0.1,
                                  path_bound, borrow_lane_type)) {...}
  // PathBoundsDebugString(*path_bound);

  // 3. Adjust the road bounds according to obstacles
  if (!GetBoundaryFromStaticObstacles(reference_line_info.path_decision(),
                                      path_bound, blocking_obstacle_id)) {...}
  ...
}
```

The flow is basically similar to the several above; there are three types of lane borrowing

```C++
  enum class LaneBorrowInfo {
    LEFT_BORROW,
    NO_BORROW,
    RIGHT_BORROW,
  };
```
