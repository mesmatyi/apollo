# planning-planner-navi

## Introduction

`planning-planner-navi` is a plugin package containing the NaviPlanner. NaviPlanner is a planner based on a relative map, mainly used in highway scenarios. It mainly considers actual temporary or moving obstacles and, while respecting speed and dynamics constraints, plans trajectories that follow the planned path as closely as possible.

## Directory Structure

```shell

modules/planning/planner/navi
├── public_road
    ├── conf                        // Configuration files
    ├── decider                     // Decider classes
    ├── proto                       // Configuration proto files
    ├── navi_planner.h              // NaviPlanner header file
    ├── navi_planner.cc             // NaviPlanner source file
    ├── navi_planner_test.cc        // Unit test file
    ├── BUILD                       // Build rules file
    ├── cyberfile.xml               // Package management configuration file
    ├── plugins.xml                 // Plugin description file
    └── README.md                   // Documentation
```
## Plugin

### NaviPlanner

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/planners/navi/conf/planner_config.pb.txt`                 | `apollo::planning::PlannerNaviConfig`              | Configuration file of NaviPlanner               |
