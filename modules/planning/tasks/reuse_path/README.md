planning-task-pull-over-path
============

## Introduction

The `ReusePath` task is used to generate a reused path. A reused path means that if the previous frame's path does not collide with obstacles in the current frame, the previous frame's path is reused as the current path
### Module flow

1. Determine whether the path is reused
  ```c++
    if (!IsPathReusable(frame, reference_line_info)) {
    path_reusable_ = false;
    return Status::OK();
  }
  ```
    - The path cannot be reused if replanning is triggered
    - The path cannot be reused if the path collides
    - The path cannot be reused if speed planning failed in the previous frame
  2. If it can be reused, trim the previous frame's path to use as the current frame's path
   ```c++
  if (!TrimHistoryPath(frame, reference_line_info)) {
    path_reusable_ = false;
    return Status::OK();
  }
   ```
## Directory Structure

```shell

modules/planning/tasks/reuse_path/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── reuse_path.proto
├── README.md
├── reuse_path.cc
└── reuse_path.h

```

## Module

### ReusePath plugin

apollo::planning::ReusePath

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/reuse_path/conf/default_conf.pb.txt` | apollo::planning::ReusePathConfig | Default configuration file of ReusePath |

#### Module parameters
  
   The algorithm parameter configuration is defined in modules/planning/tasks/reuse_path/proto/reuse_path.proto
   
   | enable_reuse_path_in_lane_follow | Whether to enable path reuse in non-lane-change scenarios |
   | -------------------------------- | -------------------------- |
   | short_path_threshold             | Minimum path length               |

#### Command-line parameters / gflags

The command-line parameters used are defined in `modules/planning/planning_base/common/planning_gflags.cc`, and their values are defined in planning.conf.
| flag | Description |
  |  ---- | ---- |
|  FLAGS_path_bounds_decider_resolution | Path planning sampling point distance |
|  FLAGS_num_extra_tail_bound_point | Number of extra sampling points added at the tail of the path |

#### Usage

##### Configuring the ReusePath Task plugin to load

    In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `ReusePath` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `ReusePath`.
    ```
    task {
      name: "REUSE_PATH"
      type: "ReusePath"
    }
      ```
