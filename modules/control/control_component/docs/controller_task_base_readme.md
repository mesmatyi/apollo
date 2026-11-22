Introduction to controller_task_base
==============

## Introduction

control_task_base mainly contains the controller manager ControlTaskAgent, the controller base class ControlTask, and the controller algorithms, filtering algorithms, numerical computation methods, trajectory analysis methods, and so on needed by the controller plugin packages. It holds some methods common to the control module, the common configuration parameters of controllers, and so on.

![control_task_base](./images/control_task_base.png)

#### control_task_agent
ControlTaskAgent is the manager that manages controller plugins. The Init method of ControlTask is used to read the configuration file that loads controllers (`modules/control/control_component/conf/pipeline.pb.txt`) and calls the plugin manager PluginManager to create plugin instances. The controllers are loaded in the order they appear in the configuration file, and then the initialization method Init of each controller instance is called to perform the controller's initialization. In the ComputeControlCommand method, following the ControlTask loading order from the Init method, the ComputeControlCommand method inside each controller ControlTask is executed in sequence to compute the control commands. Take the following `modules/control/control_component/conf/pipeline.pb.txt` configuration as an example,
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
it means that the lateral controller LatController is loaded first and then the longitudinal controller LonController, and the execution order is likewise LatController first and then LonController. Load them accordingly based on the actual controller execution logic.

#### control_task
ControlTask is the parent class of the controller plugins, and all Control/controller plugins inherit from ControlTask. ControlTask has a few common methods: Init is the initialization, ComputeControlCommand computes the control commands, LoadConfig loads the controller's configuration parameters (the controller's configuration file is the `control/controllers/xxxx_controller/conf/controller_conf.pb.txt` file under the controller plugin package), and LoadCalibrationTable loads the vehicle calibration configuration file (the vehicle configuration parameters are common parameters and are placed in `modules/control/control_component/conf/calibration_table.pb.txt`). Every controller inherits and implements the ComputeControlCommand method, so the inputs are all localization, chassis, and planned trajectory data. After computation by the controller, some control quantities are written into ControlCommand; for example, the lateral controller LatController writes the ControlCommand->steering_target control quantity, and then the longitudinal controller outputs control quantities such as ControlCommand->throttle and ControlCommand->brake. The figure below uses the lateral controller LatController and the longitudinal controller LonController to illustrate the order in which data is written into ControlCommand.

![control_task_pipeline](./images/control_task_pipeline.png)

Therefore, different controllers or tasks can be designed to compute the control commands that need to be output, and the desired control goal is achieved by defining the execution order of these controllers/tasks. The controller plugins currently supported by Apollo are the lateral controller (LatController), the longitudinal controller (LonController), the MPC controller (MPCController), and the DemoControlTask task (DemoControlTask).


## File Organization Structure and Description
```shell
control/
└── control_component
│   ├── controller_task_base
│   │   ├── BUILD
│   │   ├── common
│   │   │   ├── BUILD
│   │   │   ├── dependency_injector.h                  // 
│   │   │   ├── hysteresis_filter.cc                   // Hysteresis filter
│   │   │   ├── hysteresis_filter.h                    // 
│   │   │   ├── hysteresis_filter_test.cc              //
│   │   │   ├── interpolation_1d.cc                    // 1D interpolation lookup method
│   │   │   ├── interpolation_1d.h                     //
│   │   │   ├── interpolation_1d_test.cc               //
│   │   │   ├── interpolation_2d.cc                    // 2D interpolation lookup method
│   │   │   ├── interpolation_2d.h                     //
│   │   │   ├── interpolation_2d_test.cc               //
│   │   │   ├── leadlag_controller.cc                  // Lead-lag regulator
│   │   │   ├── leadlag_controller.h                   //
│   │   │   ├── leadlag_controller_test.cc             //
│   │   │   ├── mrac_controller.cc                     // Model reference adaptive controller
│   │   │   ├── mrac_controller.h                      //
│   │   │   ├── mrac_controller_test.cc                //
│   │   │   ├── pid_BC_controller.cc                   // PID anti-windup controller
│   │   │   ├── pid_BC_controller.h                    //
│   │   │   ├── pid_BC_controller_test.cc              //
│   │   │   ├── pid_controller.cc                      // PID controller
│   │   │   ├── pid_controller.h                       //
│   │   │   ├── pid_controller_test.cc                 //
│   │   │   ├── pid_IC_controller.cc                   // PID anti-windup controller   
│   │   │   ├── pid_IC_controller.h                    //
│   │   │   ├── pid_IC_controller_test.cc              //
│   │   │   ├── trajectory_analyzer.cc                 // Method for projecting trajectories into frenet coordinates
│   │   │   ├── trajectory_analyzer.h                  // 
│   │   │   └── trajectory_analyzer_test.cc            //
│   │   ├── control_task_agent.cc                      // Controller plugin loading manager
│   │   ├── control_task_agent.h                       // 
│   │   ├── control_task.h                             // Controller plugin base class 
│   │   └── integration_tests                          // Unit tests
│   │       ├── BUILD                                  //
│   │       ├── control_test_base.cc                   //
│   │       ├── control_test_base.h                    //
│   │       ├── relative_position_test.cc              //
│   │       └── simple_control_test.cc                 //
```


## Module Inputs, Outputs, and Configuration

### Control Component

#### Input
| Channel name | Type | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Description of the controller loading configuration |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | Vehicle control commands, such as steering wheel, throttle, brake, and other information |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/control/control_component/conf/pipeline.pb.txt` | apollo::control::ControlPipeline | Description of the controller loading configuration |
