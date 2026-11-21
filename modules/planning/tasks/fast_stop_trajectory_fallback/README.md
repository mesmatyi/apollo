planning-task-fast-stop-trajectory-fallback
==============

## Introduction

The planning-task-fast-stop-trajectory-fallback plugin is mainly used as the default fallback_task, executed when no fallback is configured in a stage scenario. The fallback_task, when a task in a stage scenario runs into an error, computes the Speed_data information of the vehicle's fallback trajectory from the vehicle's real-time speed and acceleration, which ensures the safety of the vehicle's driving.

Scenario entry conditions:
  1. When the fallback_task in the current stage scenario is empty, FastStopTrajectoryFallback is set as the preset fallback scenario;
  2. The current task runs into an error;
  3. The speed_data in the reference line path information is empty

### GenerateFallbackSpeed

This function is the main function used when the fallback task executes, to compute the speed information in the fallback scenario. It has two main inputs, `ego_info` and `stop_distance`. It obtains the end-point information (speed and acceleration) of the vehicle's previous-frame trajectory from `ego_info` in `injector_`, and `stop_distance` sets the vehicle's stopping distance. When the vehicle's acceleration is less than 0 and its speed is less than 0, the displacement, speed, and acceleration are set to 0 so that the vehicle stops immediately; otherwise, a piecewise jerk optimization strategy is used to plan the vehicle's Speed_data according to the preset stopping distance. When the piecewise jerk optimization fails to solve, a fixed preset deceleration is used to plan the Speed_data;

### GenerateStopProfile

This function is mainly used to plan the Speed_data with a fixed preset deceleration when the piecewise jerk optimization strategy fails to solve;

## File Organization and Description

```shell
modules/planning/tasks/fast_stop_trajectory_fallback/
├── BUILD                                                 // Build rules file
├── cyberfile.xml                                         // Plugin package management configuration file
├── fast_stop_trajectory_fallback.cc                      // Task implementation file
├── fast_stop_trajectory_fallback.h                       // Task implementation file
├── plugins.xml                                           // Plugin configuration file
└── README.md                                             // Documentation
```


## Module

### FastStopTrajectoryFallback plugin

apollo::planning::FastStopTrajectoryFallback
  
#### Stages

| Stage name                                        | Type                                                   | Description                                                                            |
| -------------------------------------------- | ------------------------------------------------------| -------------------------------------------------------------------------------|
| `Speed v and acceleration a at the trajectory end point are both less than 0`                         | `apollo::common::SpeedPoint`             | The speed and acceleration at the end point of the ego vehicle trajectory are both less than 0, the vehicle has come to a complete stop, and the returned speed_data information is all 0  |
| `Speed or acceleration a at the trajectory end point is greater than 0`                         | `apollo::common::SpeedPoint`             | The speed or acceleration at the end point of the ego vehicle trajectory is greater than 0, the vehicle has not come to a complete stop, and SpeedPoints are computed piecewise  |

#### Usage

The FastStopTrajectoryFallback scenario is the default fallback_task in the code. If no fallback_task is configured in a stage scenario, the FastStopTrajectoryFallback task is entered by default when a task fails to run;
