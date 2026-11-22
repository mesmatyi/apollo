planning-task-pull-over-path
============

## Introduction

The `PullOverPath` task is used to generate the pull-over path
### Module flow

1. If a path has already been generated successfully, or the reference line is a lane-change reference line, skip the task
  ```c++
    if (!reference_line_info->path_data().Empty() ||
      reference_line_info->IsChangeLanePath()) {
    return Status::OK();
  }
  ```
  2. Compute the SL start point state of path planning
   ```c++
   GetStartPointSLState();
   ```
  3. Construct the path boundaries
   ```c++
     if (!DecidePathBounds(&candidate_path_boundaries)) {
    return Status::OK();
  }
   ```
   - Initialize the path boundary
  ```c++
  if (!PathBoundsDeciderUtil::InitPathBoundary(*reference_line_info_, &path_bound, init_sl_state_)) {
  AERROR << "Failed to initialize path boundaries.";
  return false;
}
```
  - Obtain the path boundary from the road boundary
  ```c++
if (!GetBoundaryFromRoads(*reference_line_info_, &path_bound)) {
    AERROR << "Failed to decide a rough boundary based on road boundary.";
    return false;
  }
  ```
   - Obtain the path boundary from the road boundary
   ```c++
  if (!GetBoundaryFromRoads(*reference_line_info_, &path_bound)) {
      AERROR << "Failed to decide a rough boundary based on road boundary.";
      return false;
    }
    ```
- Obtain the path boundary from the lane boundary
  ```c++
UpdatePullOverBoundaryByLaneBoundary(is_pull_over_right, &path_bound);
  ```
- Add static obstacles to the path boundary
```c++
if (!PathBoundsDeciderUtil::GetBoundaryFromStaticObstacles(
        *reference_line_info_, init_sl_state_, &path_bound,
        &blocking_obstacle_id)) {
  AERROR << "Failed to decide fine tune the boundaries after "
            "taking into consideration all static obstacles.";
  return false;
}
```
- If no pull-over position is specified, search for a pull-over position
```c++
  if (!SearchPullOverPosition(path_bound, &pull_over_configuration)) {
        AERROR << "Failed to find a proper pull-over position.";
        return false;
      }
```
  4.  Plan the path
  ```c++
   if (!OptimizePath(candidate_path_boundaries, &candidate_path_data)) {
    return Status::OK();
  }
  ```
  5.  Evaluate whether the path is valid
  ```c++
    if (AssessPath(&candidate_path_data,
                 reference_line_info->mutable_path_data())) {
    ADEBUG << "pull-over path success";
  }
  ```

## Directory Structure

```shell

modules/planning/tasks/pull_over_path/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── pull_over_path.proto
├── pull_over_path.cc
├── pull_over_path.h
└── README.md

```

## Module

### PullOverPath plugin

apollo::planning::PullOverPath

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/pull_over_path/conf/default_conf.pb.txt` | apollo::planning::PullOverPathConfig | Default configuration file of PullOverPath |

#### Module parameters
  
The algorithm parameter configuration is defined in modules/planning/tasks/pull_over_path/proto/pull_over_path.proto
   
   | path_optimizer_config      | Path planner parameters                     |
   | -------------------------- | ---------------------------------- |
   | pull_over_road_edge_buffer | Distance from the road boundary when pulling over             |
   | pull_over_weight           | Pull-over weight (the larger the weight, the larger the curvature) |
   | pull_over_direction        | Pull-over direction                       |
   | pull_over_position         | Pull-over position type                   |
#### Command-line parameters / gflags

The command-line parameters used are defined in `modules/planning/planning_base/common/planning_gflags.cc`, and their values are defined in planning.conf.
| flag | Description |
  |  ---- | ---- |
|  FLAGS_obstacle_lon_start_buffer | Obstacle start position buffer |
|  FLAGS_obstacle_lon_end_buffer | Obstacle end position buffer |
|  FLAGS_num_extra_tail_bound_point | Number of extra points added when the path is blocked |
#### Usage

##### Configuring the PullOverPath Task plugin to load

    In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `PullOverPath` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `PullOverPath`.
    ```
    task {
      name: "PULL_OVER_PATH"
      type: "PullOverPath"
    }
      ```
