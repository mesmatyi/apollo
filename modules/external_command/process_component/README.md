# external-command-process

## Introduction

`external-command-process` is the program entry of the external interface command processing module. Command requests issued from the user's business modules are received and processed in this program. According to the list of external commands to be processed in the configuration file, it loads the plugins that handle the corresponding commands, and these plugins process and forward the specific commands. When a user business module sends external commands, the external-command-process module must be started at the same time; otherwise the external commands sent will not be forwarded and processed.

![](./docs/images/process_component.png)

## Directory Structure

```shell

modules/external_command/process_component/
├── process_component
    ├── conf                    // Parameter configuration files
    ├── dag                     // Module startup files (mainboard)
    ├── docs                    // Documentation
    ├── launch                  // Module startup files (cyber_launch)
    ├── proto                   // Definitions of common (global) parameter configuration structures
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── external_command_process_component.cc        // Program entry source code
    ├── external_command_process_component.h         // Program entry header file
    └── README.md               // Documentation
```

## Module

### ExternalCommandProcessComponent

apollo::planning::ExternalCommandProcessComponent

#### Input

The commands sent by user business modules are the input of `apollo::external_command::ExternalCommandProcessComponent`, and user input commands are invoked through an `apollo::cyber::Client`. In the CommandProcessor corresponding to each command, an `apollo::cyber::Service` that handles the command is created, and the service's Response is generally the status of the current command processing.

| Service name                         | Request type                                         | Response type                                         |<div style="width: 300pt">Description</div>                                                                                                               |
| ---------------------------------- | -------------------------------------------- | -------------------------------------------- |-------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/external_command/action`               | `apollo::external_command::ActionCommand`   | `apollo::external_command::CommandStatus` | Process intervention commands, such as pause, start, and switching to manual mode |
| `/apollo/external_command/chassis`               | `apollo::external_command::ChassisCommand`   | `apollo::external_command::CommandStatus` | Custom chassis commands (campus) |
| `/apollo/external_command/free_space`               | `apollo::external_command::FreeSpaceCommand`   | `apollo::external_command::CommandStatus` | Command for parking at a specified pose (campus) |
| `/apollo/external_command/lane_follow`               | `apollo::external_command::LaneFollowCommand`   | `apollo::external_command::CommandStatus` | Command for point-to-point driving along the road |
| `/apollo/external_command/path_follow`               | `apollo::external_command::PathFollowCommand`   | `apollo::external_command::CommandStatus` | Command for driving along a specified route (campus) |
| `/apollo/external_command/speed`               | `apollo::external_command::SpeedCommand`   | `apollo::external_command::CommandStatus` | Command for changing the speed (campus) |
| `/apollo/external_command/valet_parking`               | `apollo::external_command::ValetParkingCommand`   | `apollo::external_command::CommandStatus` | Command for parking in a specified parking spot |

#### Output

Input external commands are preprocessed and converted into internal commands that are sent to the planning, control, or canbus modules. The converted internal commands are sent as Cyber topics, as follows:

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/command`                     | `apollo::planning::PlanningCommand`             | Internal command converted from an external command with a navigation action, sent to the planning module |
| `/apollo/routing_response`      | `apollo::routing::RoutingResponse`     | For external commands that drive point to point along lane lines on the HD map, the routing path generated during preprocessing, used for HMI display  |
| `/apollo/planning/pad`      | `apollo::planning::PadMessage`     | Internal command converted from the external command ActionCommand, sent to the planning module |
| `/apollo/control/pad`      | `apollo::control::PadMessage`     | Internal command converted from the external command ActionCommand, sent to the control module |
| `/apollo/canbus/chassis_control`      | `apollo::external_command::ChassisCommand`     | Internal command converted from the external command ChassisCommand, sent to the canbus module |

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/external_command/process_component/conf/config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file, containing which external commands are supported for processing when the program runs  |

#### Usage

##### Starting with mainboard

```shell
mainboard -d modules/external_command/process_component/dag/external_command_process.dag
```

##### Starting with cyber_launch

```shell
cyber_launch start modules/external_command/process_component/launch/external_command_process.launch
```
