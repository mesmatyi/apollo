# external-command-speed

## Introduction

`external-command-speed` is the processing module that handles and forwards the external command `apollo::external_command::SpeedCommand`. It receives SpeedCommand commands invoked through a Cyber service, converts them into the `apollo::planning::PlanningCommand` channel message required by the planning module, and publishes it to the planning module.

The `apollo::external_command::SpeedCommand` command is used to change the current cruising speed in real time while driving. It mainly contains the following information:
- **header**: the command message header; information can be injected with the apollo::common::util::FillHeader method
- **command_id**: the unique identifier of the command
- **linear_speed**: the target cruising speed, which can be set in three ways:
  - target_speed: the value of the new target cruising speed
  - target_speed_factor: the adjustment ratio of the current cruising speed; the new cruising speed is the current cruising speed multiplied by target_speed_factor
  - is_restore_target_speed: whether to restore the originally set cruising speed; when true, the cruising speed previously set through SpeedCommand is cancelled

SpeedCommandProcessor inherits from apollo::external_command::CommandProcessorBase (see the readme of the external-command-processor-base package). Within the framework of the parent class, it implements the following three virtual functions itself:
- **Init**: Besides initializing the parent class's member variables, the initialization function also creates its own devices for receiving and sending messages, mainly:
  - std::shared_ptr<cyber::Service<SpeedCommand, CommandStatus>> command_service_: the service that handles SpeedCommand requests.
  - std::shared_ptr<WriterHandle> planning_command_writer_: the sender of the PlanningCommand message after a SpeedCommand is converted into a PlanningCommand.
  - MessageReader* message_reader_: subscribes to the task execution status CommandStatus message published by the planning module.
- **GetCommandStatus**: Obtains the execution status of the command that changes the cruising speed. Generally, the command is considered complete once the planning module has received it.
- **OnCommand**: The callback invoked when a SpeedCommand request is received. It mainly copies the received SpeedCommand information into custom_command in the PlanningCommand message body as an extended user-defined command. The planning module processes it according to the type of the received custom_command.

## Directory Structure

```shell

modules/external_command/command_processor/speed_command_processor/
├── speed_command_processor
    ├── conf                    // Parameter configuration files
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── speed_command_processor.cc        // Program entry source code
    ├── speed_command_processor.h         // Program entry header file
    └── README.md               // Documentation
```

## Plugin

### SpeedCommandProcessor

apollo::planning::SpeedCommandProcessor

#### Input

The plugin has two kinds of input:

1. **Executing navigation commands**: users call commands through a Cyber service to execute the command that sets the cruising speed.

| Service name                       | Request type                              | Response type                              | <div style="width: 300pt">Description</div> |
| -------------------------------- | ---------------------------------------- | ----------------------------------------- | ------------------------------------ |
| `/apollo/external_command/speed` | `apollo::external_command::SpeedCommand` | `apollo::external_command::CommandStatus` | Command for setting the cruising speed                   |

2. **Obtaining the command status**: the plugin listens through a Cyber Reader to the command execution status published by the planning module.

| Channel name                        | Type                                      | <div style="width: 300pt">Description</div>      |
| --------------------------------- | ----------------------------------------- | ----------------------------------------- |
| `/apollo/planning/command_status` | `apollo::external_command::CommandStatus` | Topic for receiving the command execution status published by the planning module |


#### Output

The plugin's output is the final result of command conversion:

| Channel name                 | Type                                | <div style="width: 300pt">Description</div>                 |
| -------------------------- | ----------------------------------- | ---------------------------------------------------- |
| `/apollo/planning/command` | `apollo::planning::PlanningCommand` | Internal command converted from the cruising speed command, sent to the planning module |

#### Configuration

| File path                                                                                | Type/Structure                                          | <div style="width: 300pt">Description</div>                         |
| --------------------------------------------------------------------------------------- | -------------------------------------------------- | ------------------------------------------------------------ |
| `modules/external_command/command_processor/speed_command_processor/conf/config.pb.txt` | `apollo::external_command::CommandProcessorConfig` | Configuration file: channel or service names and other input/output information of the external command processor |

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="external-command-speed" type="binary">external-command-speed</depend>
```
##### Plugin loading configuration
All external command plugins are loaded and run in the external-command-process package. To support handling SpeedCommand external commands, add the following configuration to the configuration file modules/external_command/process_component/conf/config.pb.txt:
```shell
processor: "apollo::external_command::SpeedCommandProcessor"
```
