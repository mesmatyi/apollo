planning-task-fast-stop-trajectory-fallback
==============

## Introduction
The planning-task-smooth-stop-trajectory-fallback plugin mainly handles the case where the default fallback_task in a stage scenario is SMOOTH_STOP_TRAJECTORY_FALLBACK and its task fails. Based on the vehicle's real-time speed and acceleration and considering the collision situation with surrounding obstacles, it computes the Speed_data information of the vehicle's fallback trajectory. While making the vehicle speed fall back, it also ensures the passenger comfort of the vehicle and avoids collisions with obstacles, improving driving safety.

### GenerateFallbackSpeed
This function is the main function used when the fallback task executes, to compute the speed information in the fallback scenario. It has two main inputs, `ego_info` and `FLAGS_speed_fallback_distance` (unused; set to the constant 0). It obtains the end-point information (speed and acceleration) of the vehicle's previous-frame trajectory from `ego_info` in `injector_`, and `FLAGS_speed_fallback_distance` sets the vehicle's stopping distance (unused). The main strategy is: when the vehicle's acceleration is less than 0 and its speed is less than 0, the displacement, speed, and acceleration are set to 0 so that the vehicle stops immediately; when the vehicle's acceleration is less than 0 (the vehicle has started to decelerate) and its speed is greater than 0, SpeedPoints are computed piecewise and it is checked whether the vehicle is at risk of colliding with surrounding obstacles; when the vehicle's acceleration is greater than 0 (the vehicle is accelerating), depending on whether the vehicle's current state carries a collision risk with surrounding obstacles, one of the two strategies Fast_stop and Smooth_stop is adopted for speed planning;

### GetLowerSTBound
Based on the S-T graph of the reference line `reference_line_info` information in each frame of driving, it computes the minimum ST boundaries of obstacles and stores them in `lower_bounds`. It traverses all `st_boundaries` on the reference line, computes the minimum boundary points and stores them in the point set `lower_bound_points`, and then stores all point sets in the `lower_bounds` container as the boundary condition for collision detection.

### IsCollisionWithSpeedBoundaries
The main function of this function is to detect whether the currently planned speed information would collide with surrounding obstacles. Its two main inputs are `lower_bounds` and `speed_data`. `lower_bounds` is the point set of the minimum boundaries in the current S-T graph where collisions with all obstacles occur, and `speed_data` is the currently planned speed, acceleration, S displacement, and time point information. The main principle of the function is to look at the time information of the planned `speed_data` and determine whether a collision occurs by checking whether the S displacement of `speed_data` at that moment is greater than the minimum S of the point set of `lower_bounds` at that moment.

## File Organization and Description

```shell
modules/planning/tasks/smooth_stop_trajectory_fallback/
├── util
│   ├── speed_planner.cc                                  // Task implementation file
│   └── speed_planner.h                                   // Task implementation file
├── BUILD                                                 // Build rules file
├── cyberfile.xml                                         // Plugin package management configuration file
├── smooth_stop_trajectory_fallback.cc                    // Task implementation file
├── smooth_stop_trajectory_fallback.h                     // Task implementation file
├── plugins.xml                                           // Plugin configuration file
└── README.md                                             // Documentation
```


## Module

### SmoothStopTrajectoryFallback plugin

apollo::planning::SmoothStopTrajectoryFallback

#### Scenario entry conditions
  1. The fallback_task in the current stage scenario is: SMOOTH_STOP_TRAJECTORY_FALLBACK;
  2. A task in this stage scenario runs into an error;

#### Flags

| File path                                                 | <div style="width: 300pt">Description</div> |
| -------------------------------------------------------- | ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` | Flag configuration file of the planning module           |

#### Stages

| Stage name                                | Type                         | Description                                                                             |
| ------------------------------------- | ---------------------------- | ------------------------------------------------------------------------------------------------ |
| `Speed v and acceleration a at the trajectory end point are both less than 0`   | `apollo::common::SpeedPoint` | The speed and acceleration at the end point of the ego vehicle trajectory are both less than 0, the vehicle has come to a complete stop, and the returned speed_data information is all 0                   |
| `Acceleration at the trajectory end point is less than 0 and speed v is greater than 0` | `apollo::common::SpeedPoint` | The vehicle has not come to a complete stop; SpeedPoints are computed piecewise and it is checked whether the vehicle is at risk of colliding with surrounding obstacles                           |
| `Acceleration a at the trajectory end point is greater than 0`          | `apollo::common::SpeedPoint` | Depending on whether the vehicle is at risk of colliding with surrounding obstacles, one of the two strategies Fast_stop and Smooth_stop is adopted to compute the vehicle's speed_data piecewise |

#### Usage
##### Configuring the LaneChangePath Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, SmoothStopTrajectoryFallback.
```
fallback_task: {
    name: "SMOOTH_STOP_TRAJECTORY_FALLBACK"
    type: "SmoothStopTrajectoryFallback"
}
```
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
