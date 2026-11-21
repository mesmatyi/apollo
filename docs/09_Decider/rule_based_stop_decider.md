# Rule-Based Stop Decider

### *Table of Contents*

- [Overview](#overview)
- [Related code and corresponding versions](#related-code-and-corresponding-versions)
- [Code flow and framework](#code-flow-and-framework)
- [Related algorithm analysis](#related-algorithm-analysis)

# Overview

The `rule-based stop decider` is a task of the planning module and belongs to the decider category of tasks.

The overall motion flow chart of the planning module is as follows:

![Overall flow chart](images/lane_follow.png)

The overall flow chart is explained using the [lane follow](https://github.com/ApolloAuto/apollo/blob/r6.0.0/modules/planning/conf/scenario/lane_follow_config.pb.txt) scenario as an example. The main functionality of a task is in the `Process` function.

For the detailed running process of Fig.1, see [path_bounds_decider]().

# Related code and corresponding versions

The rule-based stop decider sets stop flags according to a set of rules.

The code is located at [Apollo r6.0.0 rule_based_stop_decider](https://github.com/ApolloAuto/apollo/tree/r6.0.0/modules/planning/tasks/deciders/rule_based_stop_decider).

- Input

`apollo::common::Status RuleBasedStopDecider::Process(Frame *const frame, ReferenceLineInfo *const reference_line_info)`

The inputs are frame and reference_line_info. For a detailed explanation, see [path_bounds_decider]().

- Output

The output is saved in reference_line_info.


# Code flow and framework

The running flow of the code is shown in the following figure.

![Flow chart](images/rule_based_stop_decider.png)

The code structure is fairly clear:

```C++
apollo::common::Status RuleBasedStopDecider::Process(
    Frame *const frame, ReferenceLineInfo *const reference_line_info) {
  // 1. Pass through the opposite-direction lane, stop
  StopOnSidePass(frame, reference_line_info);

  // 2. Emergency lane change, stop
  if (FLAGS_enable_lane_change_urgency_checking) {
    CheckLaneChangeUrgency(frame);
  }

  // 3. End of the path, stop
  AddPathEndStop(frame, reference_line_info);

  return Status::OK();
}
```

# Related algorithm analysis

Analyze each part of the overall flow in the figure above separately.

- Stop on side pass

![StopOnSidePass](images/stop_on_side_pass.png)

The code is as follows:

```C++
void RuleBasedStopDecider::StopOnSidePass(
    Frame *const frame, ReferenceLineInfo *const reference_line_info) {
  static bool check_clear;    // Default false
  static common::PathPoint change_lane_stop_path_point;

  // Get path_data
  const PathData &path_data = reference_line_info->path_data();
  double stop_s_on_pathdata = 0.0;

  // 1. Find "self" and return directly
  if (path_data.path_label().find("self") != std::string::npos) {
    check_clear = false;
    change_lane_stop_path_point.Clear();
    return;
  }

  // 2. If check_clear is true and CheckClearDone succeeds, set check_clear to false
  if (check_clear &&
      CheckClearDone(*reference_line_info, change_lane_stop_path_point)) {
    check_clear = false;
  }

  // 3. If check_clear is false, and check the stop fence
  if (!check_clear &&
      CheckSidePassStop(path_data, *reference_line_info, &stop_s_on_pathdata)) {
    // 3.1 If the obstacle is not blocking and a lane change is possible, return directly
    if (!LaneChangeDecider::IsPerceptionBlocked(
            *reference_line_info,
            rule_based_stop_decider_config_.search_beam_length(),
            rule_based_stop_decider_config_.search_beam_radius_intensity(),
            rule_based_stop_decider_config_.search_range(),
            rule_based_stop_decider_config_.is_block_angle_threshold()) &&
        LaneChangeDecider::IsClearToChangeLane(reference_line_info)) {
      return;
    }
    // 3.2 Check whether the adc has stopped before the stop fence; return true if not
    if (!CheckADCStop(path_data, *reference_line_info, stop_s_on_pathdata)) {
      // Set the stop fence; on success, execute check_clear = true;
      if (!BuildSidePassStopFence(path_data, stop_s_on_pathdata,
                                  &change_lane_stop_path_point, frame,
                                  reference_line_info)) {
        AERROR << "Set side pass stop fail";
      }
    } else {
      if (LaneChangeDecider::IsClearToChangeLane(reference_line_info)) {
        check_clear = true;
      }
    }
  }
}
```

- Check lane change Urgency

![StopOnSidePass](images/check_lane_change_urgency.png)

Check the emergency lane change; the code is as follows:

```C++
void RuleBasedStopDecider::CheckLaneChangeUrgency(Frame *const frame) {
  // Enter the loop directly and check each reference_line_info
  for (auto &reference_line_info : *frame->mutable_reference_line_info()) {

    // 1. Check whether the target road is blocked; if it is on the change lane path, skip it
    if (reference_line_info.IsChangeLanePath()) {
      is_clear_to_change_lane_ =
          LaneChangeDecider::IsClearToChangeLane(&reference_line_info);
      is_change_lane_planning_succeed_ =
          reference_line_info.Cost() < kStraightForwardLineCost;
      continue;
    }

    // 2. If it is not a lane-change scenario, or (the target lane is not blocked) and the lane-change planning succeeded, skip
    if (frame->reference_line_info().size() <= 1 ||
        (is_clear_to_change_lane_ && is_change_lane_planning_succeed_)) {
      continue;
    }
    // When the target lane is blocked in change-lane case, check the urgency
    // Get the end point of current routing
    const auto &route_end_waypoint =
        reference_line_info.Lanes().RouteEndWaypoint();

    // 3. Cannot get the lane at the end of the route, skip
    if (!route_end_waypoint.lane) {
      continue;
    }
    auto point = route_end_waypoint.lane->GetSmoothPoint(route_end_waypoint.s);
    auto *reference_line = reference_line_info.mutable_reference_line();
    common::SLPoint sl_point;

    // Map the points of the current reference line to the Frenet coordinate system
    if (reference_line->XYToSL(point, &sl_point) &&
        reference_line->IsOnLane(sl_point)) {
      // Check the distance from ADC to the end point of current routing
      double distance_to_passage_end =
          sl_point.s() - reference_line_info.AdcSlBoundary().end_s();

      // 4. If the adc is far from the routing end point, no stop is needed, skip
      if (distance_to_passage_end >
          rule_based_stop_decider_config_.approach_distance_for_lane_change()) {
        continue;
      }

      // 5. If an emergency is encountered, set a temporary stop fence and wait for the lane change
      const std::string stop_wall_id = "lane_change_stop";
      std::vector<std::string> wait_for_obstacles;
      util::BuildStopDecision(
          stop_wall_id, sl_point.s(),
          rule_based_stop_decider_config_.urgent_distance_for_lane_change(),
          StopReasonCode::STOP_REASON_LANE_CHANGE_URGENCY, wait_for_obstacles,
          "RuleBasedStopDecider", frame, &reference_line_info);
    }
  }
}
```

- Add path end stop

![StopOnSidePass](images/add_path_end_stop.png)

Add a stop fence at the end of the road. The code is as follows:

```C++
void RuleBasedStopDecider::AddPathEndStop(
    Frame *const frame, ReferenceLineInfo *const reference_line_info) {
  if (!reference_line_info->path_data().path_label().empty() &&
      reference_line_info->path_data().frenet_frame_path().back().s() -
              reference_line_info->path_data().frenet_frame_path().front().s() <
          FLAGS_short_path_length_threshold) {
    const std::string stop_wall_id =
        PATH_END_VO_ID_PREFIX + reference_line_info->path_data().path_label();
    std::vector<std::string> wait_for_obstacles;

    // Create the stop fence
    util::BuildStopDecision(
        stop_wall_id,
        reference_line_info->path_data().frenet_frame_path().back().s() - 5.0,
        0.0, StopReasonCode::STOP_REASON_REFERENCE_END, wait_for_obstacles,
        "RuleBasedStopDecider", frame, reference_line_info);
  }
}
```