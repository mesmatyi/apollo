control
==============

## Introduction

The Control module is mainly composed of the control component package and the controller packages. The control component package is the basic component of the control module and contains the overall architecture and flow of control. Based on the desired trajectory information from the upstream planning module, the current localization information from the localization module, and the vehicle chassis and body status information, control calculates the commands that control the vehicle (including steering, throttle, brake, etc.) through different control algorithms and outputs them to the canbus module, providing a comfortable driving experience for the autonomous vehicle.

![control_structure](./docs/images/control_structure.png)

#### control_component
control_component is a subclass inherited from apollo::cyber::TimerComponent. It is a timer-triggered component, and the timer period can be modified through the dag configuration. Init and Proc are the entry functions. In the initialization function, it mainly implements the initialization of ControlTaskAgent and the subscription to the upstream messages of control. In the Proc execution function, several steps are executed in turn: obtain the latest data of the subscribed messages --> check whether the input data of the subscribed messages is complete (the code mainly checks whether the trajectory data is empty; checks for other message data can also be added yourself) --> check whether the timestamps of the input data of the subscribed messages are within the tolerance range (whether the data period of the upstream messages has timed out; if it has timed out, control performs emergency handling) --> update the vehicle body pose information --> perform the control computation (this part calls the ComputeControlCommand method of ControlTaskAgent, which manages the loading and execution order of the ControlTask controllers through the configuration file, thereby completing the computation of the control commands) --> output the chassis control commands.

#### control_task_base
control_component/controller_task_base/ mainly contains the definitions of ControlTaskAgent and ControlTask. ControlTaskAgent is used to manage the loading and execution order of ControlTask plugins, and ControlTask is the parent class of the controller plugins. All Control/controller plugins inherit from ControlTask. The controller plugins currently supported by Apollo are the lateral controller (LatController), the longitudinal controller (LonController), the MPC controller (MPCController), and the demo task (DemoControlTask).

#### controller
Apollo's control of the vehicle converts the vehicle from the vehicle body coordinate system to the Frenet coordinate system for position tracking. The direction along the road is longitudinal and the direction perpendicular to the road is lateral, so the vehicle's trajectory-tracking motion can be decomposed into lateral motion and longitudinal motion. By modeling the dynamics of the vehicle body, selecting suitable state variables to observe the vehicle's tracking status, and then computing reasonable control commands through the lateral and longitudinal control algorithms, the goal of tracking the trajectory is achieved.
![frenet](./docs/images/frenet.png)

Therefore, within the controller plugins there are the lateral controller (based on the LQR control algorithm), the longitudinal controller (based on the PID control algorithm), and the MPC controller (which completes lateral and longitudinal control based on the MPC algorithm). The Demo task demonstrates a control strategy of limiting a vehicle's acceleration. In controller design, you can design a controller with an overall function, such as a lateral controller or a longitudinal controller, or you can design a task that has only part of the functions, such as control logic for limiting (limiting the output) or limiting speed in a specific restricted scenario. This type of control executes a specific logical task, and we call it a task, such as the Demo task. Controller or task plugins can be customized according to usage needs.

#### Related Documents
- [Introduction to controller_task_base](./docs/controller_task_base_readme_cn.md)

## File Organization Structure and Description

The control directory contains 2 subdirectories: the control_component base package and the controllers plugin packages.
```shell
control/
├── control_component/                  // control base component
└── controllers/                        // controller algorithm or logic task components
```

The control_component directory is the base component package, with the plugin parent class implementations, configuration files, and so on.
```shell
control/
└── control_component
    ├── common                          // Module-wide gflag definitions
    ├── conf                            // Module configuration files, parameter file directory, including gflags variable configuration, the configuration files for plugin enabling, vehicle calibration tables, and other common configuration files
    ├── controller_task_base/           // control controller parent class component
    │   ├── common/                     // Math formulas, algorithm formulas, filter functions, trajectory analysis
    │   ├── integration_tests/          // Unit test folder
    │   ├── control_task_agent.cc       // Implementation file of the controller loading manager
    │   ├── control_task_agent.h        // Implementation file of the controller loading manager
    │   └── control_task.h              // Implementation file of the controller parent class
    ├── dag/                            // Module startup file (mainboard)
    ├── docs/                           // Documentation related to the module
    ├── launch/                         // Module startup file (cyber_launch)
    ├── proto/                          // Configuration files defined by the component
    ├── submodules/                     // control submodules
    ├── testdata/                       // Unit test data
    ├── tools/                          // Debugging tools
    ├── BUILD                           // Build rules file
    ├── control_component.cc            // Code file of the component implementation
    ├── control_component.h             // Code file of the component implementation
    ├── control_component_test.cc       // Component unit test file
    ├── control.json                    // Packaging description file
    ├── cyberfile.xml                   // Package management configuration file
    └── README_cn.md                    // Documentation
```

The controllers directory is the controller plugin directory. Control supports defining algorithm-based controller plugins as well as controller plugins based on logic rules.
```shell
modules/control/
└── controllers                         // Controller plugin directory
    ├── demo_control_task               // demo controller plugin package
    │   ├── proto/                      // Folder for the controller's configuration definitions
    │   ├── conf/                       // Controller configuration folder
    │   ├── BUILD                       // Build rules file
    │   ├── cyberfile.xml               // Package management configuration file
    │   ├── demo_control_task.cc        // Implementation file of the demo controller
    │   ├── demo_control_task.h         // Implementation file of the demo controller
    │   └── plugins.xml                 // Plugin rules file
    ├── lat_based_lqr_controller        // LQR lateral controller plugin package
    │   ├── proto/                      // Folder for the controller's configuration definitions
    │   ├── conf/                       // Controller configuration folder
    │   ├── BUILD                       // Build rules file
    │   ├── cyberfile.xml               // Package management configuration file
    │   ├── lat_controller.cc           // Implementation file of the LQR lateral controller
    │   ├── lat_controller.h            // Implementation file of the LQR lateral controller
    │   ├── lat_controller_test.cc      // Unit test file of the LQR lateral controller
    │   ├── lateral_controller_test     // Controller test data
    │   └── plugins.xml                 // Plugin rules file
    ├── lon_based_pid_controller        // PID longitudinal controller plugin package
    │   ├── proto/                      // Folder for the controller's configuration definitions
    │   ├── conf/                       // Controller configuration folder
    │   ├── BUILD                       // Build rules file
    │   ├── cyberfile.xml               // Package management configuration file
    │   ├── lon_controller.cc           // Implementation file of the PID longitudinal controller
    │   ├── lon_controller.h            // Implementation file of the PID longitudinal controller
    │   ├── lon_controller_test.cc      // Unit test file of the PID longitudinal controller
    │   ├── longitudinal_controller_test// Controller test data
    │   └── plugins.xml                 // Plugin rules file
    └── mpc_controller                  // MPC lateral and longitudinal controller plugin package
        ├── proto/                      // Folder for the controller's configuration definitions
        ├── conf/                       // Controller configuration folder
        ├── BUILD                       // Build rules file
        ├── cyberfile.xml               // Package management configuration file
        ├── mpc_controller.cc           // Implementation file of the MPC controller
        ├── mpc_controller.h            // Implementation file of the MPC controller
        ├── mpc_controller_test.cc      // Unit test file of the MPC controller
        ├── mpc_controller_test_data    // Controller test data
        └── plugins.xml                 // Plugin rules file
```


## Module Inputs, Outputs, and Configuration

### Control Component

#### Input
| Channel name | Type | Description |
| ---- | ---- | ---- |
| `/apollo/planning` | apollo::planning::ADCTrajectory | Vehicle planned trajectory information |
| `/apollo/localization/pose`  | apollo::localization::LocalizationEstimate | Vehicle localization information |
| `/apollo/canbus/chassis`| apollo::canbus::Chassis | Vehicle chassis information |
| - | apollo::common::VehicleState | Vehicle body pose information |
| `/apollo/control/pad` | apollo::control::ControlCommand::PadMessage | Autonomous driving enable (request to enter autonomous driving) command |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control commands, such as steering wheel, throttle, brake, and other information |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Configuration file of ControlComponent |
| `modules/control/control_component/conf/control.conf` | `command line flags` | Command-line parameter configuration |
| `modules/control/control_component/conf/calibration_table.pb.txt` |  apollo::control::calibration_table | Vehicle longitudinal calibration table configuration |

#### Flags
| flagfile | Type | Description |
| ---- | ---- | ------ |
| `modules/control/control_component/common/control_gflags.cc` | `flags` | Flags variable definition file of the Control component |
| `modules/control/control_component/common/control_gflags.h` | `declare` | Flags declaration file of the Control component |

#### Usage

##### Configure the controllers to load

According to the controllers you need to start, modify the configuration parameters in `modules/control/control_component/conf/pipeline.pb.txt` to start the corresponding controllers or tasks. Controllers are loaded and executed in the order configured in the pipeline, so pay attention to the order in which controllers are configured to avoid errors. In the configuration parameters, `name` indicates the name of the controller, which is user-defined and only needs to make clear which controller it is, and `type` is the name of the controller's subclass. Note that if it is not consistent with the subclass name, loading the controller will fail. The example below loads `LatController` first and then `LonController`.
```
controller {
  name: "LAT_CONTROLLER"
  type: "LatController"
}
controller {
  name: "LON_CONTROLLER"
  type: "LonController"
}
```

##### Start with mainboard

```shell
mainboard -d modules/control/dag/control.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/control/launch/control.launch
```
