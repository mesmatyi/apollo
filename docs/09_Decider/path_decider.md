# Path Decider

### *Table of Contents*
- [Overview](#overview)
- [Path decider code and corresponding versions](#path-decider-code-and-corresponding-versions)
- [Path decider code flow and framework](#path-decider-code-flow-and-framework)
- [Path decider algorithm analysis](#path-decider-algorithm-analysis)

# Overview

The `path decider` is a task of the planning module and belongs to the decider category of tasks.

The overall motion flow chart of the planning module is as follows:

![Overall flow chart](images/lane_follow.png)

The overall flow chart is explained using the [lane follow](https://github.com/ApolloAuto/apollo/blob/r6.0.0/modules/planning/conf/scenario/lane_follow_config.pb.txt) scenario as an example. The main functionality of a task is in the `Process` function.

For the detailed running process of Fig.1, see [path_bounds_decider]().

# Path decider code and corresponding versions

The optimal path was obtained in the previous task. The function of the `path decider` is to make decisions for the ego vehicle based on static obstacles.

The code of the `path decider` is [Apollo r6.0.0 path_decider](https://github.com/ApolloAuto/apollo/tree/r6.0.0/modules/planning/tasks/deciders/path_decider)

- Input

`Status PathDecider::Process(const ReferenceLineInfo *reference_line_info, const PathData &path_data,
 PathDecision *const path_decision)`

- Output

The path decision information is all saved in `path_decision`.

# Path decider code flow and framework

The overall flow of the `path decider` is shown in the following figure:

![Flow chart](images/path_decider.png)

The main function of the `Process` function is to call the `MakeObjectDecision` function. The `MakeObjectDecision` function in turn calls the `MakeStaticObstacleDecision` function.

The main functionality of the path decider is all in `MakeStaticObstacleDecision`.

```C++
Status PathDecider::Process(const ReferenceLineInfo *reference_line_info,
                            const PathData &path_data,
                            PathDecision *const path_decision) {
  // skip path_decider if reused path
  if (FLAGS_enable_skip_path_tasks && reference_line_info->path_reusable()) {
    return Status::OK();
  }

  std::string blocking_obstacle_id;
  if (reference_line_info->GetBlockingObstacle() != nullptr) {
    blocking_obstacle_id = reference_line_info->GetBlockingObstacle()->Id();
  }
  // Call the MakeObjectDecision function
  if (!MakeObjectDecision(path_data, blocking_obstacle_id, path_decision)) {
    const std::string msg = "Failed to make decision based on tunnel";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }
  return Status::OK();
}

bool PathDecider::MakeObjectDecision(const PathData &path_data,
                                     const std::string &blocking_obstacle_id,
                                     PathDecision *const path_decision) {
  // The main functionality of the path decider is in MakeStaticObstacleDecision
  if (!MakeStaticObstacleDecision(path_data, blocking_obstacle_id,
                                  path_decision)) {
    AERROR << "Failed to make decisions for static obstacles";
    return false;
  }
  return true;
}
```

# Path decider algorithm analysis

Code analysis for the path-decider flow chart above.

- Get the coordinates in the Frenet coordinate system

```C++
  ... ...
  // 1. Get the path in Frenet coordinates
  const auto &frenet_path = path_data.frenet_frame_path();
  if (frenet_path.empty()) {
    AERROR << "Path is empty.";
    return false;
  }
  ... ...
```

- Make decisions based on obstacles

```C++
  ... ...
  // 2. Iterate over each obstacle and make decisions
  for (const auto *obstacle : path_decision->obstacles().Items()) {
    const std::string &obstacle_id = obstacle->Id();
    const std::string obstacle_type_name =
        PerceptionObstacle_Type_Name(obstacle->Perception().type());
    ADEBUG << "obstacle_id[<< " << obstacle_id << "] type["
           << obstacle_type_name << "]";
    ... ...
```

The red box in the figure above contains the main content of the loop body, whose main function is to iterate over each obstacle and make decisions.

- If the obstacle is not static or is virtual, skip it

```C++
    // 2.1 If the obstacle is not static or is virtual, skip it
    if (!obstacle->IsStatic() || obstacle->IsVirtual()) {    // (stop fence, all kinds of fences)
      continue;
    }
```

- If the obstacle already has an ignore/stop decision, skip it

```C++
    // 2.2 If the obstacle already has an ignore/stop decision, skip it
    if (obstacle->HasLongitudinalDecision() &&
        obstacle->LongitudinalDecision().has_ignore() &&
        obstacle->HasLateralDecision() &&
        obstacle->LateralDecision().has_ignore()) {
      continue;
    }
    if (obstacle->HasLongitudinalDecision() &&
        obstacle->LongitudinalDecision().has_stop()) {
      // STOP decision
      continue;
    }
```

- If the obstacle blocks the path, add a stop decision

```C++
    // 2.3 If the obstacle blocks the path, add a stop decision
    if (obstacle->Id() == blocking_obstacle_id &&
        !injector_->planning_context()
             ->planning_status()
             .path_decider()
             .is_in_path_lane_borrow_scenario()) {
      // Add stop decision
      ADEBUG << "Blocking obstacle = " << blocking_obstacle_id;
      ObjectDecisionType object_decision;
      *object_decision.mutable_stop() = GenerateObjectStopDecision(*obstacle);
      path_decision->AddLongitudinalDecision("PathDecider/blocking_obstacle",
                                             obstacle->Id(), object_decision);
      continue;
    }
```

- If it is a clear-zone, skip it

```C++
    // 2.4 If it is a clear-zone, skip it
    if (obstacle->reference_line_st_boundary().boundary_type() ==
        STBoundary::BoundaryType::KEEP_CLEAR) {
      continue;
    }
```

- If the obstacle is not on the path, skip it

```C++
    // 2.5 If the obstacle is not on the path, skip it
    ObjectDecisionType object_decision;
    object_decision.mutable_ignore();
    const auto &sl_boundary = obstacle->PerceptionSLBoundary();
    if (sl_boundary.end_s() < frenet_path.front().s() ||
        sl_boundary.start_s() > frenet_path.back().s()) {
      path_decision->AddLongitudinalDecision("PathDecider/not-in-s",
                                             obstacle->Id(), object_decision);
      path_decision->AddLateralDecision("PathDecider/not-in-s", obstacle->Id(),
                                        object_decision);
      continue;
    }
```

- Nudge decision

```C++
    // 2.6 Nudge decision: if the distance to the static obstacle is too large, ignore it.
    //               If the static obstacle is too close to the lane center, stop.
    //               If it is close laterally, avoid it.
    if (curr_l - lateral_radius > sl_boundary.end_l() ||
        curr_l + lateral_radius < sl_boundary.start_l()) {
      // 1. IGNORE if laterally too far away.
      path_decision->AddLateralDecision("PathDecider/not-in-l", obstacle->Id(),
                                        object_decision);
    } else if (sl_boundary.end_l() >= curr_l - min_nudge_l &&
               sl_boundary.start_l() <= curr_l + min_nudge_l) {
      // 2. STOP if laterally too overlapping.
      *object_decision.mutable_stop() = GenerateObjectStopDecision(*obstacle);

      if (path_decision->MergeWithMainStop(
              object_decision.stop(), obstacle->Id(),
              reference_line_info_->reference_line(),
              reference_line_info_->AdcSlBoundary())) {
        path_decision->AddLongitudinalDecision("PathDecider/nearest-stop",
                                               obstacle->Id(), object_decision);
      } else {
        ObjectDecisionType object_decision;
        object_decision.mutable_ignore();
        path_decision->AddLongitudinalDecision("PathDecider/not-nearest-stop",
                                               obstacle->Id(), object_decision);
      }
    } else {
      // 3. NUDGE if laterally very close.
      if (sl_boundary.end_l() < curr_l - min_nudge_l) {  // &&
        // sl_boundary.end_l() > curr_l - min_nudge_l - 0.3) {
        // LEFT_NUDGE
        ObjectNudge *object_nudge_ptr = object_decision.mutable_nudge();
        object_nudge_ptr->set_type(ObjectNudge::LEFT_NUDGE);
        object_nudge_ptr->set_distance_l(
            config_.path_decider_config().static_obstacle_buffer());
        path_decision->AddLateralDecision("PathDecider/left-nudge",
                                          obstacle->Id(), object_decision);
      } else if (sl_boundary.start_l() > curr_l + min_nudge_l) {  // &&
        // sl_boundary.start_l() < curr_l + min_nudge_l + 0.3) {
        // RIGHT_NUDGE
        ObjectNudge *object_nudge_ptr = object_decision.mutable_nudge();
        object_nudge_ptr->set_type(ObjectNudge::RIGHT_NUDGE);
        object_nudge_ptr->set_distance_l(
            -config_.path_decider_config().static_obstacle_buffer());
        path_decision->AddLateralDecision("PathDecider/right-nudge",
                                          obstacle->Id(), object_decision);
      }
    }
```
