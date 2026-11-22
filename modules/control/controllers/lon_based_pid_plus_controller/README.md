control-controller-lon-based-pid-plus-controller
==============

## Introduction

The control-controller-lon-based-pid-plus-controller plugin package is the implementation of a controller that performs vehicle longitudinal control computation based on a PID controller. Vehicle longitudinal control, in the Frenet coordinate system and along the direction of the tangent of the road reference line, controls the vehicle's position, speed, and acceleration so that the vehicle drives according to the reference position and reference speed of the planned trajectory. There are many vehicle longitudinal control algorithms, and this plugin designs a control method based on the PID controller.

The control-controller-lon-based-pid-plus-controller plugin mainly contains the implementation files of LonPLusController. LonPlusController is a subclass inheriting from ControlTask and implements methods such as Init, ComputeControlCommand, Reset, and Name. The main computation logic is in the ComputeControlCommand method, whose inputs are the planned trajectory (apollo::planning::ADCTrajectory), vehicle position information (apollo::localization::LocalizationEstimate), vehicle pose information (VehicleStateProvider), vehicle chassis information (apollo::canbus::Chassis), and so on. It solves the longitudinal errors (position error, speed error) and the desired vehicle acceleration, and adds the control quantities of the two-stage PID controller. You can choose whether to add slope acceleration compensation. When the vehicle approaches the end point of the planned trajectory, an acceleration for approaching the end point can be set to stop accurately, and if a pedestrian has been standing in front of the vehicle for a long time, a corresponding acceleration can also be configured to act on the vehicle. After the acceleration is solved, the vehicle calibration table is looked up to convert it into the vehicle's control interface, such as the throttle pedal or brake pedal, and it is fed to the vehicle, completing 1 control cycle.

#### Longitudinal Error Calculation

The longitudinal error calculation is in the ComputeLongitudinalErrors() method. The core idea is to find the point on the planned trajectory as the target point for control reference through an absolute-time query method, then use the optimal distance calculation method (apollo::control::TrajectoryAnalyzer::QueryMatchedPathPoint) to find the best matching point of the vehicle's current position on the trajectory (the point on the planned line matched_point), project the coordinates of the vehicle's current position (VRF coordinates, the vehicle body coordinate system) into the best-matching trajectory point coordinate system (Frenet), and obtain the distance (PathPoint.s) from the vehicle's current position projected onto the S axis (the Frenet longitudinal direction) (this distance is defined relative to the planning starting point). In this way, the longitudinal error is calculated in the coordinate system of the same planned trajectory, that is, the actual longitudinal position (Frenet: S axis) error = the longitudinal distance of the reference point of the planned trajectory (TrajectoryPoint.s) - the longitudinal distance of the matching point onto which the current vehicle is projected on the trajectory (s_matched.s). For the handling of the trajectory reference coordinates, refer to the apollo::control::TrajectoryAnalyzer::ToTrajectoryFrame method (`modules/control/control_component/control_task_base/common/trajectory_analyzer.cc`).

#### PID Controller

The full name of the PID controller is Proportional–Integral–Derivative controller. The methods are defined in `modules/control/control_component/control_task_base/common/pid_controller.cc`, and the methods of PIDController are Init (initialization), SetPID (set the p, i, d parameters), Reset (reset), Reset_integral (reset the integral term), and Control (PID computation). PID sums the magnitude of the error, the integral of the error, and the derivative of the error and applies it to the control system to achieve the goal of eliminating the error. PID has a proportional parameter kp, an integral parameter ki, and a derivative parameter kd. Different parameters have different regulating roles and effects. When tuning, you can first tune kp, gradually increasing it from small to large, then appropriately increase ki to eliminate steady-state error, and increase kd when necessary to improve the regulation speed of the system. Tune the speed loop speed_pid first, and then appropriately add the position loop station_pid.

#### Vehicle Calibration Table
Different vehicles have different control interfaces. The main control interfaces of the drive-by-wire vehicles supported by Apollo are based on throttle and brake interfaces, and the throttle/brake control quantity is linearly correlated with the vehicle's driving speed. The vehicle may not be able to directly provide the corresponding data or calculation formula for this correlation. To control vehicle speed relatively accurately through the drive-by-wire interface, a quantitative calibration of the actuation quantity versus the observed value is needed to obtain the correlation. Select different vehicle speeds, drive in a straight line while accelerating or decelerating, and record the longitudinal acceleration of the vehicle body at the current moment. Repeat this, recording hard acceleration, gentle acceleration, hard deceleration, gentle deceleration, and so on at different speeds, to form a calibration table, as shown in `modules/control/control_component/conf/calibration_table.pb.txt`, which is plotted as shown in the figure below.

Throttle calibration table
![](./docs/images/throttle.png)

Brake calibration table
![](./docs/images/brake.png)

## File Organization Structure and Description

```shell
control/controllers/lon_based_pid_plus_controller/
├── conf/                                                 // Controller configuration parameter files
├── docs/                                                 // Documentation related
├── longitudinal_controller_test/                         // Unit test data
├── proto
│   ├── BUILD
│   └── lon_based_pid_plus_controller_conf.proto               // Controller configuration parameter definitions
├── BUILD                                                 // Rule build file
├── cyberfile.xml                                         // Plugin package management configuration file
├── lon_plus_controller.cc                                     // Controller implementation file
├── lon_plus_controller.h                                      // Controller implementation file
├── lon_plus_controller_test.cc                                // Controller unit test file
├── plugins.xml                                           // Plugin configuration file
└── README_cn.md                                          // Documentation
```


## Module Inputs, Outputs, and Configuration

### control-controller-lon-based-pid-plus-controller Plugin

#### Input
| Channel name | Type | Description |
| ---- | ---- | ---- |
| `/apollo/planning` | apollo::planning::ADCTrajectory | Vehicle planned trajectory information (trajectory point information). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LonPlusController inherits from the ControlTask base class |
| `/apollo/localization/pose` | apollo::localization::LocalizationEstimate | Vehicle localization information (position in the world coordinate system). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LonPLusController inherits from the ControlTask base class |
| `/apollo/canbus/chassis` | apollo::canbus::Chassis | Vehicle chassis information (chassis vehicle speed). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LonPlusController inherits from the ControlTask base class |
| - | apollo::common::VehicleState | Vehicle body pose information (vehicle body pitch angle) |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command (steering wheel control command). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LonPlusController inherits from the ControlTask base class |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: throttle and brake commands |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Configuration file of ControlComponent |
| `modules/control/control_component/conf/control.conf` | `command line flags` | Command-line parameter configuration, which configures global flag variables |
| `modules/control/controllers/lon_based_pid_plus_controller/conf/controller_conf.pb.txt` | apollo::control::LonBasedPidControllerConf | Configuration file of the PID longitudinal controller |

#### Flags
| flagfile | Type | Description |
| ---- | ---- | ------ |
| `modules/control/control_component/common/control_gflags.cc` | `flags` | Defines the global flag variables used by LonPlusController, configured through control.conf |
| `modules/control/control_component/common/control_gflags.h` | `declare` | flags declaration file |

#### Usage

##### Configure Loading the LonPLusController Controller

Add the controller parameters to be loaded in `modules/control/control_component/conf/pipeline.pb.txt`. In the configuration parameters, `name` indicates the name of the controller, which is user-defined and only needs to make clear which controller it is, and `type` is the class name of the LonPlusController controller, that is, LonPlusController. Note that if the `type` name in the configuration file is not consistent with the loaded class name, loading the controller will fail.
```
controller {
  name: "LON_PLUS_CONTROLLER"
  type: "LonPlusController"
}
```
##### Configure Suitable LonPLusController Controller Parameters

The longitudinal controller configuration parameters are set in the `modules/control/controllers/lon_based_pid_plus_controller/conf/controller_conf.pb.txt` file. The main purpose is to adapt to different vehicle models and different dynamics systems and find the control parameters with the best control performance. When actually using this plugin, the control parameters should also be adjusted according to the evaluation of the control performance to meet the requirements of the evaluation. At the same time, pay attention to modifying the dynamics calibration table: in the `modules/control/control_component/conf/calibration_table.pb.txt` file, change it to the dynamics calibration table of the vehicle model to be adapted.

##### Configure the Global Variable Parameters Used by the LonPLusController Controller

Among the configuration parameters of the longitudinal controller, some are global variables defined in `modules/control/control_component/common/control_gflags.cc`. If the default values of these parameters are modified, they need to be configured in the `modules/control/control_component/conf/control.conf` file to take effect.

##### Start with mainboard

```shell
mainboard -d modules/control/dag/control.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/control/launch/control.launch
```
