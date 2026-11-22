control-controller-lat-based-lqr-controller
==============

## Introduction

The control-controller-lat-based-lqr-controller plugin package is the implementation of a controller that performs vehicle lateral control based on the LQR control algorithm. Vehicle lateral control, in the Frenet coordinate system and along the normal direction of the tangent of the road reference line, controls the vehicle's lateral position and body heading angle so that the vehicle drives along the direction of the planned trajectory. There are many vehicle lateral control algorithms, and this plugin designs a lateral control method based on the LQR control algorithm.

![lat_based_lqr_controller_structure](./docs/images/control-controller-lat-based-lqr-controller.png)

The control-controller-lat-based-lqr-controller plugin mainly contains the implementation files of LatCotroller. LatCotroller is a subclass inheriting from ControlTask and implements methods such as Init, ComputeControlCommand, Reset, and Name. The main computation logic is in the ComputeControlCommand method, whose inputs are the planned trajectory (apollo::planning::ADCTrajectory), vehicle position information (apollo::localization::LocalizationEstimate), vehicle pose information (VehicleStateProvider), vehicle chassis information (apollo::canbus::Chassis), and so on. Based on the dynamics modeling of vehicle turning, the differential equations of the vehicle dynamics are established, and [lateral position error, lateral position error rate, heading angle error, heading angle error rate]^T is designed as the state variable of the control system. The state variables can be obtained from observed values (computed errors), and the system state transition matrix (A) and control matrix (B) are derived. The continuous-time system state equation is discretized, the Q and R matrices are designed, and LQR computation solves the Riccati equation to obtain the feedback matrix K. The control quantity computed from the feedback matrix K is steer_angle_feedback; the other part is the vehicle feedforward steer_angle_feedforward. A lead-lag regulator is also designed for the lateral error to compensate laterally. Finally the three are added together and fed to the vehicle as one control quantity, completing 1 control cycle.

#### Lateral Error Calculation

The lateral error calculation is in the ComputeLateralErrors() method. The core idea is to use the nearest-distance calculation method (apollo::control::TrajectoryAnalyzer::QueryNearestPointByPosition) to find, from the vehicle's current position, the closest point on the planned trajectory (the point on the trajectory match_point), and then project the actual coordinates (VRF) onto the match_point(Frenet) on the trajectory. Here the matched point (match_point) is the lateral reference trajectory point, and the vehicle's lateral position error is the distance in the L direction of the projection change (d_matched). The lateral distance assumes a direction: turning left is positive. A positive lateral error means the vehicle currently deviates to the left of the reference trajectory point, a positive curvature on the trajectory means turning left, and a positive steering angle output controls the vehicle to turn the wheels left. The heading angle error is the vehicle's current heading angle minus the heading angle of the reference trajectory point, the lateral position error rate is the projection component in the lateral direction of the motion velocity of the vehicle body pose (in the VRF coordinate system), and the heading angle rate is the yaw rate of the vehicle body pose.

The current query method is used to find the target trajectory point on the current planned trajectory, and then the coordinates of the vehicle's current position (VRF coordinates, the vehicle body coordinate system) are projected into the coordinate system of the planned target trajectory point (Frenet). After projection, only the longitudinal error is considered, equal to the reference point minus the actual point, that is, longitudinal error = the reference value of the planned trajectory (position, speed) - the actual value of the current vehicle (the longitudinal position and longitudinal speed after projection). For the handling of the trajectory reference coordinates, refer to the apollo::control::TrajectoryAnalyzer::ToTrajectoryFrame method (`modules/control/control_component/control_task_base/common/trajectory_analyzer.cc`).

#### LQR Controller

The full name of the LQR controller is linear quadratic regulator. The method is defined in `modules/common/math/linear_quadratic_regulator.cc`, and its static method is SolveLQRProblem (matrix difference calculation of the Riccati equation). By setting the system iteration tolerance (eps) and the maximum number of iterations (max_iteration), the precision and solving time can be changed.

#### Vehicle Dynamics Model


## File Organization Structure and Description

```shell
control/controllers/lat_based_lqr_controller/
├── conf/                                                  // Controller configuration parameter files
├── docs/                                                  // Documentation related
├── lateral_controller_test/                               // Unit test data
├── proto
│   ├── BUILD
│   └── lat_based_lqr_controller_conf.proto                // Controller configuration parameter definitions
├── BUILD                                                  // Rule build file
├── cyberfile.xml                                          // Plugin package management configuration file
├── lat_controller.cc                                      // Controller implementation file
├── lat_controller.h                                       // Controller implementation file
├── lat_controller_test.cc                                 // Controller unit test file
├── plugins.xml                                            // Plugin configuration file
└── README_cn.md                                           // Documentation
```


## Module Inputs, Outputs, and Configuration

### control-controller-lat-based-lqr-controller Plugin

#### Input
| Channel name | Type | Description |
| ---- | ---- | ---- |
| `/apollo/planning` | apollo::planning::ADCTrajectory | Vehicle planned trajectory information (trajectory point information). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LatController inherits from the ControlTask base class |
| `/apollo/localization/pose` | apollo::localization::LocalizationEstimate | Vehicle localization information (position in the world coordinate system). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LatController inherits from the ControlTask base class |
| `/apollo/canbus/chassis` | apollo::canbus::Chassis | Vehicle chassis information (the vehicle's actual steering angle and vehicle speed). control_component subscribes to this message and passes it as a parameter to the ComputeControlCommand method that LatController inherits from the ControlTask base class |
| - | apollo::common::VehicleState | Vehicle body pose information (heading angle in the vehicle body coordinate system, vehicle body speed) |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: steering wheel angle command |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Configuration file of ControlComponent |
| `modules/control/control_component/conf/control.conf` | `command line flags` | Command-line parameter configuration, which configures global flag variables |
| `modules/control/controllers/lat_based_lqr_controller/conf/controller_conf.pb.txt` | apollo::control::LatBaseLqrControllerConf | Configuration file of the LQR lateral controller |

#### Flags
| flagfile | Type | Description |
| ---- | ---- | ------ |
| `modules/control/control_component/common/control_gflags.cc` | `flags` | Defines the global flag variables used by LatController, configured through control.conf |
| `modules/control/control_component/common/control_gflags.h` | `declare` | flags declaration file |

#### Usage

##### Configure Loading the LonController Controller

Add the controller parameters to be loaded in `modules/control/control_component/conf/pipeline.pb.txt`. In the configuration parameters, `name` indicates the name of the controller, which is user-defined and only needs to make clear which controller it is, and `type` is the class name of the LatController controller, that is, LatController. Note that if the `type` name in the configuration file is not consistent with the loaded class name, loading the controller will fail.
```
controller {
  name: "LAT_CONTROLLER"
  type: "LatController"
}
```

##### Configure Suitable LatController Controller Parameters

The lateral controller configuration parameters are set in the `modules/control/controllers/lat_based_lqr_controller/conf/controller_conf.pb.txt` file. The main purpose is to adapt to different vehicle models, whose steering systems and mass distributions differ, and find the control parameters with the best control performance. When actually using this plugin, the control parameters should also be adjusted according to the evaluation of the control performance to meet the requirements of the evaluation.

##### Configure the Global Variable Parameters Used by the LatController Controller

Among the configuration parameters of the longitudinal controller, some are global variables defined in `modules/control/control_component/common/control_gflags.cc`. If the default values of these parameters are modified, they need to be configured in the `modules/control/control_component/conf/control.conf` file to take effect.

##### Start with mainboard

```shell
mainboard -d modules/control/dag/control.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/control/launch/control.launch
```
