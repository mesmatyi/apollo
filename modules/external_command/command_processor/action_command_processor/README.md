# external-command-action

## Introduction

The `external-command-action` plugin provides the ability to operate the autonomous driving planning and control process through the external action command `apollo::external_command::ActionCommand`. It receives external action commands (ActionCommand) through a Cyber service and handles them in a callback function. An external action command is converted in one of two ways: either into the `apollo::planning::PadMessage` channel message required by the planning module, which changes planning decisions (for example, switching to driving along the main road, changing lanes, stopping, or resuming driving); or into the `apollo::control::PadMessage` channel message required by the control and canbus modules, which switches the chassis driving mode (for example, from non-autonomous to autonomous driving, or from autonomous back to non-autonomous driving).

`apollo::external_command::ActionCommand` is an external command for controlling autonomous driving. It is used to switch the vehicle's behavior during autonomous driving and can also make the vehicle enter or exit autonomous driving. It mainly contains the following information:
- **header**: the command message header; information can be injected with the apollo::common::util::FillHeader method
- **command_id**: the unique identifier of the command
- **command**: the custom data structure `ActionCommandType`, which specifies the type of the action command. It covers commands such as car following, lane change, pull over, emergency stop, resume driving, enter autonomous driving, exit autonomous driving, and vehicle identification number (VIN) retrieval.

ActionCommandProcessor inherits from the parent class CommandProcessorBase and therefore has the following features of the parent class:
- **An interface for obtaining the command execution status**: the GetCommandStatus function returns the real-time execution status of a command.
- **A method for receiving and processing external commands**: OnCommand is the callback invoked when a user calls the external interface through a Cyber service. It converts the external command data into the corresponding channel data format and publishes it with Writer(message).


## Directory Structure

```shell

modules/external_command/command_processor/
├── action_command_processor
    ├── conf                               // Parameter configuration files
    ├── BUILD                              // Build rules file
    ├── cyberfile.xml                      // Package management configuration file
    ├── action_command_processor.cc        // Program entry source code
    ├── action_command_processor.h         // Program entry header file
    └── README.md                          // Documentation
```

## Plugin

### ActionCommandProcessor

apollo::planning::ActionCommandProcessor

#### Input

The plugin has two kinds of input:

1. **Executing external action commands**: users call commands through a Cyber service to issue external operation instructions to autonomous driving.

| Service name                        | Request type                                         | Response type                                         |<div style="width: 300pt">Description</div>                                                                                                               |
| ---------------------------------- | -------------------------------------------- | -------------------------------------------- |-------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/external_command/action`               | `apollo::external_command::ActionCommand`   | `apollo::external_command::CommandStatus` | External action command for autonomous driving |

2. **Obtaining command status, planning information, and chassis information**: the plugin listens through Cyber Readers to the command execution status, planning information, and chassis information published by the planning module and others.

| Channel name                            | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/command_status`                     | `apollo::external_command::CommandStatus`             | Command execution status published by the planning module |
| `/apollo/planning` | `apollo::planning::ADCTrajectory` | Current scenario information of the vehicle planning in the planning trajectory information |
| `/apollo/canbus/chassis` | `apollo::canbus::Chassis` | Driving mode information in the vehicle chassis information |

#### Output

The plugin's output is the channel message published after an external action command is executed (/apollo/planning/pad or /apollo/control/pad), together with the updated command execution status (CommandStatus) published by the planning module:

| Channel name                            | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/pad`                     | `apollo::planning::PadMessage`             | Command that changes the planning scenario behavior |
| `/apollo/control/pad`      | `apollo::control::PadMessage`     | Command that changes the chassis driving mode |
| `/apollo/planning/command_status`                     | `apollo::external_command::CommandStatus`             | Updates the execution status of external commands published by the planning module |

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/external_command/command_processor/action_command_processor/conf/config.pb.txt`                 | `apollo::external_command::CommandProcessorConfig`              | Configuration file: channel or service names and other input/output information of the external action command processor  |
| `modules/external_command/command_processor/action_command_processor/conf/special_config.pb.txt`                 | `apollo::external_command::ActionCommandConfig`              | Configuration file: configuration of the external action command processor module  |

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="external-command-action" type="binary">external-command-action</depend>
```
##### Plugin loading configuration
All external command plugins are loaded and run in the external-command-process package. To support handling ActionCommand external action commands, add the following configuration to the configuration file modules/external_command/process_component/conf/config.pb.txt:
```shell
processor: "apollo::external_command::ActionCommandProcessor"
```

##### Starting with mainboard
```shell
mainboard -d /apollo/modules/external_command/process_component/dag/external_command_process.dag
```
