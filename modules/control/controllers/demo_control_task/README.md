control-controller-demo-control-task
==============

## Introduction

The control-controller-demo-control-task plugin package is an example of a task. Its main job is to limit the magnitude of the acceleration control command in ControlCommand. This acceleration may be the output of a preceding controller (such as the longitudinal PID controller), but under certain conditions we want to limit the acceleration output rather than feed the algorithm's output directly into the vehicle. Therefore, such a task can be added, and manual rules can be implemented in the task to intervene in the control output. In this example, by modifying the configuration file, if the acceleration command in the control command is less than the configured acceleration value, the acceleration command is changed to the configured acceleration value; if it is greater, the original acceleration command is left unchanged.


## File Organization Structure and Description

```shell
control/controllers/demo_control_task/
├── conf/                                                 // Controller configuration parameter files
├── docs/                                                 // Documentation related
├── proto
│   ├── BUILD
│   └── demo_control_task_conf.proto                      // Controller configuration parameter definitions
├── BUILD                                                 // Rule build file
├── cyberfile.xml                                         // Plugin package management configuration file
├── demo_control_task.cc                                  // Implementation file of the task
├── demo_control_task.h                                   // Implementation file of the task
├── plugins.xml                                           // Plugin configuration file
└── README_cn.md                                          // Documentation
```


## Module Inputs, Outputs, and Configuration

### control-controller-demo-control-task Plugin

#### Input
| Channel name | Type | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: acceleration command |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control command: acceleration command |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Configuration file of ControlComponent |
| `modules/control/controllers/demo_control_task/conf/controller_conf.pb.txt` | apollo::control::DemoControlTaskConf | Configuration file of the Demo task |

#### Usage

##### Configure loading the DemoControlTask controller

Add the controller parameters to be loaded in `modules/control/control_component/conf/pipeline.pb.txt`. In the configuration parameters, `name` indicates the name of the controller, which is user-defined and only needs to make clear which controller it is, and `type` is the class name of the DemoControlTask controller, that is, DemoControlTask. Note that if the `type` name in the configuration file is not consistent with the loaded class name, loading the controller will fail.
```
controller {
  name: "DEMO_CONTROLTASK"
  type: "DemoControlTask"
}
```

##### Configure suitable DemoControlTask controller parameters

The configuration parameters of the demo task controller are set in the `modules/control/controllers/demo_control_task/conf/controller_conf.pb.txt` file. Define the limit on the magnitude of the acceleration control command through the configuration parameters according to actual needs.

##### Start with mainboard

```shell
mainboard -d modules/control/dag/control.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/control/launch/control.launch
```
