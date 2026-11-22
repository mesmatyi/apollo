control-controller-mpc-controller
==============

## Introduction

The control-controller-mpc-controller plugin package is the implementation of a controller that performs joint lateral and longitudinal vehicle control based on the MPC control algorithm. Solving with this algorithm can simultaneously output commands controlling the vehicle's steering wheel angle, throttle, brake, and so on.

![mpc_controller_structure](./docs/images/control-controller-mpc-controller.png)

The control-controller-mpc-controller plugin mainly contains the implementation files of MPCController. MPCController is a subclass inheriting from ControlTask and implements methods such as Init, ComputeControlCommand, Reset, and Name. The main computation logic is in the ComputeControlCommand method, whose inputs are the planned trajectory ( apollo::planning::ADCTrajectory ), vehicle position information ( apollo::localization::LocalizationEstimate ), vehicle pose information ( VehicleStateProvider ), vehicle chassis information ( apollo::canbus::Chassis ), and so on. The differential equations of the vehicle's lateral and longitudinal dynamics are established, and according to trajectory tracking, [lateral position error, lateral position error rate, heading angle error, heading angle error rate, longitudinal error, longitudinal speed error]^T is designed as the state variable of the control system. An optimization cost function is constructed, and the optimal solution that, within the given optimization space, drives the differential equations of the system state variables toward 0 is solved for and taken as the system's control quantity, which is fed to the vehicle, completing 1 control cycle.

#### Lateral and Longitudinal Error Calculation

The lateral error calculation is in the ComputeLateralErrors() method and is consistent with the calculation method of the lateral controller ( apollo::control::LatController::ComputeLongitudinalErrors ); the longitudinal error calculation is in the ComputeLongitudinalErrors() method and is consistent with the calculation method of the longitudinal controller ( apollo::control::LonController::ComputeLongitudinalErrors ). For detailed descriptions, refer to the documentation of the corresponding controllers.

#### MPC Controller

The full name of the MPC controller is model predictive control. As the name suggests, it makes predictions based on a model and, under given constraints, solves for the optimal solution that satisfies the cost function (driving the system state variables toward 0 with the minimum control effort), then takes the first term of the optimal solution as the control output to feed to the system input. The quadratic optimization method is defined in `apollo/modules/common/math/mpc_osqp.cc`, and the quadratic optimization solver used is [OSQP](https://osqp.org/).


## File Organization Structure and Description

```shell
control/controllers/lat_based_lqr_controller/
├── conf/                                                  // Controller configuration parameter files
├── docs/                                                  // Documentation related
├── lateral_controller_test/                               // Unit test data
├── proto
│   ├── BUILD
│   └── mpc_controller.proto                               // Controller configuration parameter definitions
├── BUILD                                                  // Rule build file
├── cyberfile.xml                                          // Plugin package management configuration file
├── mpc_controller.cc                                      // Controller implementation file
├── mpc_controller.h                                       // Controller implementation file
├── mpc_controller_test.cc                                 // Controller unit test file
├── plugins.xml                                            // Plugin configuration file
└── README_cn.md                                           // Documentation
```


## Module Inputs, Outputs, and Configuration

### control-controller-mpc-controller Plugin

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
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: steering wheel angle, throttle, and brake commands |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Configuration file of ControlComponent |
| `modules/control/control_component/conf/control.conf` | `command line flags` | Command-line parameter configuration, which configures global flag variables |
| `modules/control/controllers/mpc_controller/conf/controller_conf.pb.txt` | apollo::control::MPCControllerConf | Configuration file of the MPC lateral and longitudinal controller |

#### Flags
| flagfile | Type | Description |
| ---- | ---- | ------ |
| `modules/control/control_component/common/control_gflags.cc` | `flags` | Defines the global flag variables used by MPCController, configured through control.conf |
| `modules/control/control_component/common/control_gflags.h` | `declare` | flags declaration file |

#### Usage

##### Configure Loading the MPCController Controller

Add the controller parameters to be loaded in `modules/control/control_component/conf/pipeline.pb.txt`. In the configuration parameters, `name` indicates the name of the controller, which is user-defined and only needs to make clear which controller it is, and `type` is the class name of the MPCController controller, that is, MPCController. Note that if the `type` name in the configuration file is not consistent with the loaded class name, loading the controller will fail.
```
controller {
  name: "MPC_CONTROLLER"
  type: "MPCController"
}
```

##### Configure Suitable MPCController Controller Parameters

The MPC controller configuration parameters are set in the `modules/control/controllers/mpc_controller/conf/controller_conf.pb.txt` file. They mainly include information adapted to different vehicle models, such as mass, wheelbase, and tire cornering stiffness, the calibration tables of the steering power systems of different vehicle models, and the related algorithm control parameters. When actually using this plugin, the control parameters should also be adjusted according to the evaluation of the control performance to meet the requirements of the evaluation.

##### Configure the Global Variable Parameters Used by the MPCController Controller

Among the configuration parameters of the longitudinal controller, some are global variables defined in `modules/control/control_component/common/control_gflags.cc`. If the default values of these parameters are modified, they need to be configured in the `modules/control/control_component/conf/control.conf` file to take effect.

##### Start with mainboard

```shell
mainboard -d modules/control/dag/control.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/control/launch/control.launch
```
