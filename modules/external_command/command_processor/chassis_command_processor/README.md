external-command-chassis
==============

## Introduction

The main function of the external-command-chassis plugin is to wrap and process the interface commands inside the chassis (Chassis). Users can send chassis control commands through a custom HMI interface. It is implemented mainly through Service communication: a Client sends external chassis control requests in campus scenarios (for example "LEFT_TURN" and wiper requests), and the Service processes the Client's request and hands it to the PNC module for execution. The module also monitors and reports the status of chassis commands after execution.

### OnCommand
The main purpose of this function is to pass through the Client's Request. As the main function implementing the Service, `command` is the Client's Request. The Client is created in the `Init()` function in `external_command_wrapper_demo`, and the Client's Request is written in the `SendVehicleSignalCommand()` and `SendCustomChassisCommand()` functions in `external_command_wrapper_demo`. `status` is the Service-side Response status. The `OnCommand` function mainly writes the Client's request to the `/apollo/canbus/chassis_control` channel, and the PNC module then responds to the request.

### GetCommandStatus
This function is mainly called in the `Init()` function in `external_command_process_component.cc`. After the Client has sent a request, its main purpose is to determine the chassis status after the command has been executed, and to monitor and report it. It mainly monitors the status of the turn signals, high/low beams, horn, and hazard lights of the chassis.

## File Organization and Description

```shell
command_processor/command_processor/chassis_command_processor/
├── conf/                                                 // Controller configuration parameter files
├── BUILD                                                 // Build rules file
├── cyberfile.xml                                         // Plugin package management configuration file
├── chassis_command_processor.cc                          // Processor implementation file
├── chassis_command_processor.h                           // Processor implementation file
├── plugins.xml                                           // Plugin configuration file
└── README.md                                             // Documentation
```


## Module Input, Output, and Configuration

### external-command-chassis plugin

#### Input


| Service name                         | Request type                                | Response type                              | <div style="width: 300pt">Description</div>           |
| ---------------------------------- | ------------------------------------------ | ----------------------------------------- | ---------------------------------------------- |
| `/apollo/external_command/chassis` | `apollo::external_command::ChassisCommand` | `apollo::external_command::CommandStatus` | Custom chassis commands (such as left turn and wipers in campus scenarios) |
#### Input
| Channel name              | Type                    | Description                                     |
| ------------------------ | ----------------------- | ---------------------------------------- |
| `/apollo/canbus/chassis` | apollo::canbus::Chassis | Vehicle chassis information (actual steering angle and speed of the vehicle) |

#### Output
| Channel name                      | Type                                     | Description                                    |
| -------------------------------- | ---------------------------------------- | --------------------------------------- |
| `/apollo/canbus/chassis_control` | apollo::external_command::ChassisCommand | Vehicle control commands: brake, throttle, and EPB (electronic parking brake) commands |

#### Configuration file
| File path                                                                             | Type/Structure                                          | Description                                                |
| ------------------------------------------------------------------------------------ | -------------------------------------------------- | --------------------------------------------------- |
| `modules/external_command/command_processor/chassis_command_processor/config.pb.txt` | `apollo::external_command::CommandProcessorConfig` | Input and output channel configuration file of external-command-chassis |
