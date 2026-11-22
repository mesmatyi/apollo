# planning-planner-rtk

## Introduction

`planning-planner-rtk` is a plugin package containing the RTKReplayPlanner. RTKReplayPlanner is a planner that performs record-and-replay (RTK tracking) based on a recorded trajectory; the desired trajectory must be recorded in advance to plan the driving route.

## Directory Structure

```shell

modules/planning/planner/rtk
├── rtk
    ├── testdata                    // Test data
    ├── rtk_replay_planner.h        // RTKReplayPlanner header file
    ├── rtk_replay_planner.cc       // RTKReplayPlanner source file
    ├── rtk_replay_planner_test.cc  // Unit test file
    ├── BUILD                       // Build rules file
    ├── cyberfile.xml               // Package management configuration file
    ├── plugins.xml                 // Plugin description file
    └── README.md                   // Documentation
```
