control-controller-antislip-control-task
==============

## Introduction

The control-controller-antislip-control-task plugin package is an anti-slip (hill-hold) task. Its main purpose is that when the vehicle is driving on a road with a gradient and needs to stop on the slope, it avoids the vehicle rolling backward; and after stopping on the slope and restarting on the slope, it avoids the vehicle rolling backward. By modifying the configuration file, you can set the conditions for enabling the anti-roll function and the magnitude of the brake command on the slope. The anti-slip task is designed to process after the longitudinal controller: it takes the control command output by the longitudinal controller and then determines whether the anti-slip conditions are met, and if they are, it changes the control commands (brake, throttle) to the control commands after the anti-slip logic is activated.


## File Organization Structure and Description

```shell
control/controllers/slope_anti_slip_control_task/
├── conf/                                                 // Controller configuration parameter files
├── docs/                                                 // Documentation related
├── proto
│   ├── BUILD
│   └── antislip_control_task_conf.proto                  // Controller configuration parameter definitions
├── BUILD                                                 // Rule build file
├── cyberfile.xml                                         // Plugin package management configuration file
├── anti_slip_control_task.cc                             // Task implementation file
├── anti_slip_control_task.h                              // Task implementation file
├── plugins.xml                                           // Plugin configuration file
└── README_cn.md                                          // Documentation
```


## Module Inputs, Outputs, and Configuration

### control-controller-antislip-control-task Plugin

#### Input
| Channel name | Type | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: brake and throttle |
| `/apollo/control` | apollo::control::ControlCommand::SimpleLongitudinalDebug | Vehicle PID longitudinal control debug information (path_remain) |
| - | apollo::common::VehicleState | Vehicle body pose information (vehicle body pitch angle) |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: brake, throttle, and EPB parking brake commands |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Configuration file of ControlComponent |
| `modules/control/control_component/conf/control.conf` | `command line flags` | Command-line parameter configuration, which configures global flag variables |
| `modules/control/controllers/slope_anti_slip_control_task/conf/controller_conf.pb.txt` | apollo::control::DemoControlTaskConf | Configuration file of the AntiSlip (anti-slip) task |

#### Flags
| flagfile | Type | Description |
| ---- | ---- | ------ |
| `modules/control/control_component/common/control_gflags.cc` | `flags` | Defines the global flag variables used by AntiSlipControlTask, configured through control.conf |
| `modules/control/control_component/common/control_gflags.h` | `declare` | flags declaration file |

#### Usage

##### Configure Loading the AntiSlipControlTask Controller

Add the controller parameters to be loaded in `modules/control/control_component/conf/pipeline.pb.txt`. In the configuration parameters, `name` indicates the name of the controller, which is user-defined and only needs to make clear which controller it is, and `type` is the class name of the AntiSlipControlTask controller, that is, AntiSlipControlTask. Note that if the `type` name in the configuration file is not consistent with the loaded class name, loading the controller will fail.
```
controller {
  name: "ANTI_CONTROLTASK"
  type: "AntiSlipControlTask"
}
```

##### Configure Suitable AntiSlipControlTask Controller Parameters

The AntiSlip task controller's configuration parameters are set in the `modules/control/controllers/slope_anti_slip_control_task/conf/controller_conf.pb.txt` file, which configures the threshold for activating anti-slip, the filter cutoff frequency of the vehicle body pose pitch angle, and so on.

##### Start with mainboard

```shell
mainboard -d modules/control/dag/control.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/control/launch/control.launch
```
