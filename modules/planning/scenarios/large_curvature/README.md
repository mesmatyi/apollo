planning-scenario-large-curvature
============

## Introduction

`LargeCurvatureScenario` generates a drivable trajectory on large-curvature roads, solving the jerky motion of the main-road algorithm on large-curvature roads.

## Directory Structure

```shell

modules/planning/scenarios/large_curvature/
├── BUILD
├── conf
│   ├── large_curvature
│   │   ├── open_space_fallback_decider_park.pb.txt
│   │   ├── open_space_path_planning.pb.txt
│   │   ├── open_space_replan_decider.pb.txt
│   │   ├── open_space_roi_decider_park.pb.txt
│   │   ├── open_space_rt_optimizer.pb.txt
│   │   ├── open_space_trajectory_optimizer.pb.txt
│   │   └── open_space_trajectory_post_process.pb.txt
│   ├── pipeline.pb.txt
│   └── scenario_conf.pb.txt
├── cyberfile.xml
├── large_curvature_scenario.cc
├── large_curvature_scenario.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── large_curvature_scenario.proto
├── README.md
├── stage_large_curvature.cc
└── stage_large_curvature.h
```

## Module

### LargeCurvatureScenario plugin

apollo::planning::LargeCurvatureScenario

#### Scenario entry conditions

  1. The curvature of the reference line 3 meters ahead of the ego vehicle is greater than min_curvature
  2. The length of the reference line ahead of the ego vehicle is less than 10 meters
  
#### Stages

| Stage name                | Type                                    | <div style="width: 300pt">Description</div> |
| --------------------- | --------------------------------------- | ------------------------------------ |
| `StageLargeCurvature` | `apollo::planning::StageLargeCurvature` | Plans a trajectory for passing through the large-curvature road           |


#### Configuration

| File path                                                               | Type/Structure                                        | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------- | ------------------------------------------------ | ------------------------------------ |
| `modules/planning/scenarios/large_curvature/conf/scenario_conf.pb.txt` | `apollo::planning::ScenarioLargeCurvatureConfig` | Scenario configuration file                       |
| `modules/planning/scenarios/large_curvature/conf/pipeline.pb.txt`      | `apollo::planning::ScenarioPipeline`             | Scenario pipeline file                     |

