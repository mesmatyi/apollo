# planning-scenario-square

## Introduction

`planning-scenario-square` is used to perform escape-and-bypass driving in square areas. The scenario is triggered after the vehicle enters a `junction` area. It uses the square area boundary as the road boundary constraint and supports reversing out of a stuck situation after the vehicle is blocked.

- **pipeline**: Configures two `stage`s: `SQUARE_LANE_FOLLOW_STAGE` and `EXTRICATE_STAGE`
  - `SQUARE_LANE_FOLLOW_STAGE`: Compared with the main-road driving `LANE_FOLLOW_STAGE`, the `LANE_CHANGE_PATH` and `LANE_FOLLOW_PATH` tasks are removed and the `SQUARE_PATH` task is added. In this task, the junction boundary is used as the road constraint boundary, which enlarges the drivable space.
  - `EXTRICATE_STAGE`: When `SQUARE_LANE_FOLLOW_STAGE` fails to solve or there is no forward Path, reverse trajectory planning is adopted. `REVERSE_PATH` and `REVERSE_SPEED_DECIDER` are added to plan the reverse path and speed until forward trajectory planning succeeds.
- **Scenario entry condition**: The vehicle enters a `junction` area.

## Directory Structure

```shell

modules/planning/scenarios/square/
├── square
    ├── BUILD                // Build rules file
    ├── conf                 // Parameter configuration files
    ├── context.h            // Scenario context information
    ├── cyberfile.xml        // Package management configuration file
    ├── extricate_stage.cc          // Stage class source code, reverse escape stage
    ├── extricate_stage.h           // Stage class header file
    ├── proto                       // Message definitions
    ├── README.md                   // Documentation
    ├── square_lane_follow_stage.cc     // Stage class source code, lane-keeping stage
    ├── square_lane_follow_stage.h      // Stage class header file
    ├── square_scenario.cc              // Scenario class source code
    └── square_scenario.h               // Scenario class header file
```

## Plugin

### SquareScenario

apollo::planning::SquareScenario

#### Configuration

| File path                                                 | Type/Structure                            | <div style="width: 300pt">Description</div>  |
| -------------------------------------------------------- | ------------------------------------ | ------------------------------------- |
| `modules/planning/scenarios/square/conf/pipeline.pb.txt` | `apollo::planning::ScenarioPipeline` | Configuration file, the list of Stages and Tasks run in the scenario |

#### Flags

| File path                                                                                          | <div style="width: 300pt">Description</div> |
| ------------------------------------------------------------------------------------------------- | ------------------------------------ |
| `modules/planning/scenarios/square/conf/extricate_stage/lane_borrow_path_generic.pb.txt`          | Escape stage, lane-borrow bypass task parameters           |
| `modules/planning/scenarios/square/conf/extricate_stage/piecewise_jerk_speed.pb.txt`              | Escape stage, speed planning parameters               |
| `modules/planning/scenarios/square/conf/square_lane_follow_stage/lane_borrow_path_generic.pb.txt` | Forward planning stage, lane-borrow bypass task parameters       |
| `modules/planning/scenarios/square/conf/square_lane_follow_stage/path_decider.pb.txt`             | Forward planning stage, path decision parameters           |

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="planning-scenario-square" type="binary">planning-scenario-square</depend>
```
##### Plugin loading configuration
All scenario plugins are loaded and run in the planning package. To support the square scenario, add the following configuration to the configuration file `modules/planning/planning_component/conf/public_road_planner_config.pb.txt`:
```shell
  scenario {
    name: "SQUARE"
    type: "SquareScenario"
  }
```
