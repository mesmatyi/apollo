# Path Assessment Decider

### *Table of Contents*

- [Overview](#overview)
- [Path assessment decider code and corresponding versions](#path-assessment-decider-code-and-corresponding-versions)
- [Path assessment decider code flow and framework](#path-assessment-decider-code-flow-and-framework)
  - [Path reuse](#path-reuse)
  - [Remove invalid paths](#remove-invalid-paths)
  - [Analyze and add important information](#analyze-and-add-important-information)
  - [Sort and select the optimal path](#sort-and-select-the-optimal-path)
  - [Update necessary information](#update-necessary-information)
- [Path sorting algorithm analysis](#path-sorting-algorithm-analysis)

# Overview

The `path assessment decider` is a task of the planning module and belongs to the decider category of tasks.

The overall motion flow chart of the planning module is as follows:

![Overall flow chart](images/lane_follow.png)

The overall flow chart is explained using the [lane follow](https://github.com/ApolloAuto/apollo/blob/r6.0.0/modules/planning/conf/scenario/lane_follow_config.pb.txt) scenario as an example. The main functionality of a task is in the `Process` function.

For the detailed running process of Fig.1, see [path_bounds_decider]().

# Path assessment decider code and corresponding versions

This section describes the code flow of the path assessment decider.

Please refer to the code [Apollo 10.0 path_assessment_decider_util](https://github.com/ApolloAuto/apollo/blob/master/modules/planning/planning_interface_base/task_base/common/path_util/path_assessment_decider_util.cc)

- Input

`Status PathAssessmentDecider::Process(Frame* const frame, ReferenceLineInfo* const reference_line_info)`

The inputs are Frame and reference_line_info. For a detailed explanation, see [path_bounds_decider]().

- Output

After the paths are sorted, the first path is selected. The result is saved in reference_line_info.

# Path assessment decider code flow and framework

The main code flow is shown in the following figure:

![Flow chart](images/path_assessment.png)

## Path reuse

```C++
  ... ...
  // Skip if the path is reused
  if (FLAGS_enable_skip_path_tasks && reference_line_info->path_reusable()) {
    return Status::OK();
  ... ...
```

## Remove invalid paths

```C++
  ... ...
  // 1. Remove invalid paths.
  std::vector<PathData> valid_path_data;
  for (const auto& curr_path_data : candidate_path_data) {
    // RecordDebugInfo(curr_path_data, curr_path_data.path_label(),
    //                 reference_line_info);
    if (curr_path_data.path_label().find("fallback") != std::string::npos) {
      if (IsValidFallbackPath(*reference_line_info, curr_path_data)) {
        valid_path_data.push_back(curr_path_data);
      }
    } else {
      if (IsValidRegularPath(*reference_line_info, curr_path_data)) {
        valid_path_data.push_back(curr_path_data);
      }
    }
  }
  const auto& end_time1 = std::chrono::system_clock::now();
  std::chrono::duration<double> diff = end_time1 - end_time0;
  ADEBUG << "Time for path validity checking: " << diff.count() * 1000
         << " msec.";
  ... ...
```
Among them, invalid fallback paths are paths that deviate from the reference line and the road. Invalid regular paths are paths that deviate from the reference line or the road, collide, or stop in an adjacent opposite-direction lane.

## Analyze and add important information

```C++
  ... ...
  // 2. Analyze and add important information for the speed decision
  size_t cnt = 0;
  const Obstacle* blocking_obstacle_on_selflane = nullptr;
  for (size_t i = 0; i != valid_path_data.size(); ++i) {
    auto& curr_path_data = valid_path_data[i];
    if (curr_path_data.path_label().find("fallback") != std::string::npos) {
      // remove empty path_data.
      if (!curr_path_data.Empty()) {
        if (cnt != i) {
          valid_path_data[cnt] = curr_path_data;
        }
        ++cnt;
      }
      continue;
    }
    SetPathInfo(*reference_line_info, &curr_path_data);
    // Trim all lane-borrow paths so that they can end in-lane
    if (curr_path_data.path_label().find("pullover") == std::string::npos) {
      TrimTailingOutLanePoints(&curr_path_data);
    }

    // Find blocking_obstacle_on_selflane, in preparation for the next step of lane selection
    if (curr_path_data.path_label().find("self") != std::string::npos) {
      const auto blocking_obstacle_id = curr_path_data.blocking_obstacle_id();
      blocking_obstacle_on_selflane =
          reference_line_info->path_decision()->Find(blocking_obstacle_id);
    }

    // Remove empty paths
    if (!curr_path_data.Empty()) {
      if (cnt != i) {
        valid_path_data[cnt] = curr_path_data;
      }
      ++cnt;
    }

    // RecordDebugInfo(curr_path_data, curr_path_data.path_label(),
    //                 reference_line_info);
    ADEBUG << "For " << curr_path_data.path_label() << ", "
           << "path length = " << curr_path_data.frenet_frame_path().size();
  }
  valid_path_data.resize(cnt);
  // If there is no valid path, exit
  if (valid_path_data.empty()) {
    const std::string msg = "Neither regular nor fallback path is valid.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }
  ADEBUG << "There are " << valid_path_data.size() << " valid path data.";
  const auto& end_time2 = std::chrono::system_clock::now();
  diff = end_time2 - end_time1;
  ADEBUG << "Time for path info labeling: " << diff.count() * 1000 << " msec.";
  ... ...
```
The code execution flow of this step is as follows:
1). Remove empty paths
2). Trim lane-borrow paths from the tail: search forward from the tail and trim path_points of the following types:
&emsp; (1) OUT_ON_FORWARD_LANE
&emsp; (2) OUT_ON_REVERSE_LANE
&emsp; (3) Unknown type
3). Find the obstacle id in the ego lane, for use in lane selection
4). If there is no valid path, return an error code

## Sort and select the optimal path

For this step, see the last chapter, `Path sorting algorithm analysis`

## Update necessary information

```C++
  // 4. Update necessary info for lane-borrow decider's future uses.
  // Update front static obstacle's info.
  auto* mutable_path_decider_status = injector_->planning_context()
                                          ->mutable_planning_status()
                                          ->mutable_path_decider();
  if (reference_line_info->GetBlockingObstacle() != nullptr) {
    int front_static_obstacle_cycle_counter =
        mutable_path_decider_status->front_static_obstacle_cycle_counter();
    mutable_path_decider_status->set_front_static_obstacle_cycle_counter(
        std::max(front_static_obstacle_cycle_counter, 0));
    mutable_path_decider_status->set_front_static_obstacle_cycle_counter(
        std::min(front_static_obstacle_cycle_counter + 1, 10));
    mutable_path_decider_status->set_front_static_obstacle_id(
        reference_line_info->GetBlockingObstacle()->Id());
  } else {
    int front_static_obstacle_cycle_counter =
        mutable_path_decider_status->front_static_obstacle_cycle_counter();
    mutable_path_decider_status->set_front_static_obstacle_cycle_counter(
        std::min(front_static_obstacle_cycle_counter, 0));
    mutable_path_decider_status->set_front_static_obstacle_cycle_counter(
        std::max(front_static_obstacle_cycle_counter - 1, -10));
  }

  // Update self-lane usage info.
  if (reference_line_info->path_data().path_label().find("self") !=
      std::string::npos) {
    // && std::get<1>(reference_line_info->path_data()
    //                 .path_point_decision_guide()
    //                 .front()) == PathData::PathPointType::IN_LANE)
    int able_to_use_self_lane_counter =
        mutable_path_decider_status->able_to_use_self_lane_counter();

    if (able_to_use_self_lane_counter < 0) {
      able_to_use_self_lane_counter = 0;
    }
    mutable_path_decider_status->set_able_to_use_self_lane_counter(
        std::min(able_to_use_self_lane_counter + 1, 10));
  } else {
    mutable_path_decider_status->set_able_to_use_self_lane_counter(0);
  }

  // Update side-pass direction.
  if (mutable_path_decider_status->is_in_path_lane_borrow_scenario()) {
    bool left_borrow = false;
    bool right_borrow = false;
    const auto& path_decider_status =
        injector_->planning_context()->planning_status().path_decider();
    for (const auto& lane_borrow_direction :
         path_decider_status.decided_side_pass_direction()) {
      if (lane_borrow_direction == PathDeciderStatus::LEFT_BORROW &&
          reference_line_info->path_data().path_label().find("left") !=
              std::string::npos) {
        left_borrow = true;
      }
      if (lane_borrow_direction == PathDeciderStatus::RIGHT_BORROW &&
          reference_line_info->path_data().path_label().find("right") !=
              std::string::npos) {
        right_borrow = true;
      }
    }

    mutable_path_decider_status->clear_decided_side_pass_direction();
    if (right_borrow) {
      mutable_path_decider_status->add_decided_side_pass_direction(
          PathDeciderStatus::RIGHT_BORROW);
    }
    if (left_borrow) {
      mutable_path_decider_status->add_decided_side_pass_direction(
          PathDeciderStatus::LEFT_BORROW);
    }
  }
  const auto& end_time4 = std::chrono::system_clock::now();
  diff = end_time4 - end_time3;
  ADEBUG << "Time for FSM state updating: " << diff.count() * 1000 << " msec.";

  // Plot the path in simulator for debug purpose.
  RecordDebugInfo(reference_line_info->path_data(), "Planning PathData",
                  reference_line_info);
  return Status::OK();
```

Update necessary information:

1. Update the information of static obstacles in front of the adc
2. Update the ego-lane usage information 3. Update the direction of the side lane
(1) Based on whether PathDeciderStatus is RIGHT_BORROW or LEFT_BORROW, determine whether the lane is borrowed from the left or from the right

# Path sorting algorithm analysis

This last part explains the sorting algorithm.
```C++
  ... ...
  // 3. Pick the optimal path.
  std::sort(valid_path_data.begin(), valid_path_data.end(),
            std::bind(ComparePathData, std::placeholders::_1,
                      std::placeholders::_2, blocking_obstacle_on_selflane));

  ADEBUG << "Using '" << valid_path_data.front().path_label()
         << "' path out of " << valid_path_data.size() << " path(s)";
  if (valid_path_data.front().path_label().find("fallback") !=
      std::string::npos) {
    FLAGS_static_obstacle_nudge_l_buffer = 0.8;
  }
  *(reference_line_info->mutable_path_data()) = valid_path_data.front();
  reference_line_info->SetBlockingObstacle(
      valid_path_data.front().blocking_obstacle_id());
  const auto& end_time3 = std::chrono::system_clock::now();
  diff = end_time3 - end_time2;
  ADEBUG << "Time for optimal path selection: " << diff.count() * 1000
         << " msec.";

  reference_line_info->SetCandidatePathData(std::move(valid_path_data));
  ... ...
```
The flow of the sorting algorithm is as follows:

`ComparePathData(lhs, rhs, …)`

Path sorting: (the quality of the paths is obtained through sorting)
- 1. Empty paths are always placed last
- 2.regular > fallback
- 3. If one self-lane path exists, choose it. If both exist, choose the longer one. If the lengths are close, choose the self-lane one. If neither self-lane path exists, choose the longer path
- 4. If the path lengths are close and both borrow a lane:
    - (1) If both borrow the opposite-direction lane, choose the one with the shorter distance
    - (2) For the case with two borrow directions:
      + If there are obstacles, choose the appropriate direction, borrowing from the left or the right
      + If there are no obstacles, choose the borrow direction according to the adc position
    - (3) If the path lengths are the same and the adjacent lanes are both forward-direction, choose the path that returns to the ego lane earlier
    - (4) If the path lengths are the same, both borrow forward lanes, and the return-to-ego-lane time is the same, choose the path that borrows from the left
- 5. Finally, if the two paths are identical, then lhs is not < rhl. After sorting: select the optimal path, i.e. the first path
