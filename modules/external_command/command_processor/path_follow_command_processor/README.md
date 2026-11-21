# external-command-path-follow

## Introduction

`external-command-path-follow` is the processing module that handles and forwards the external command `apollo::external_command::PathFollowCommand`. It receives PathFollowCommand commands invoked through a Cyber service, converts them into the `apollo::planning::PlanningCommand` channel message required by the planning module, and publishes it to the planning module.

The `apollo::external_command::PathFollowCommand` command is suitable for map-less scenarios in which the user has the vehicle follow a given route. The user supplies coordinate points on a global path in the command, and the vehicle drives along the global path as its reference line. It mainly contains the following information:
- **header**: the command message header; information can be injected with the apollo::common::util::FillHeader method
- **command_id**: the unique identifier of the command
- **way_point**: the coordinate points on the given route
- **boundary**: the lateral drivable area of the vehicle, which can be set in one of two ways:
  * path_boundary: set left and right boundaries for each path point
  * boundary_with_width: set a uniform left and right boundary width for all path points

  If no boundary is set, the vehicle width is used by default as the left and right boundary. In this case the vehicle cannot bypass obstacles it encounters and can only stop.
![](./docs/images/path_boundary.png)
PathFollowCommandProcessor inherits from the template class MotionCommandProcessorBase<PathFollowCommand> and therefore has the following features of the parent class:
- **An interface for obtaining the command execution status**: the GetCommandStatus function returns the real-time execution status of a command.
- **Receiving external commands and converting them into planning input**: OnCommand is the callback invoked when a user calls the external interface through a Cyber service. After converting the external command data into a PlanningCommand, it publishes it through planning_command_writer_.

Besides inheriting the command processing flow from the parent class MotionCommandProcessorBase<PathFollowCommand> (see the readme of `external-command-processor-base`), PathFollowCommandProcessor implements the following two functions specially:
- **Convert**: If an external command received by a subclass of MotionCommandProcessorBase contains information about driving along lane lines, that information must be extracted and placed in the `apollo::routing::RoutingRequest` member of `apollo::planning::PlanningCommand`. PathFollowCommand does not need to drive along lane lines, so this function does no conversion and returns an empty RoutingRequest.
- **ProcessSpecialCommand**: It copies the information of PathFollowCommand directly into the custom_command member of `apollo::planning::PlanningCommand`, to be processed in the planning module. The planning module processes it according to the type of the received custom_command.

## Directory Structure

```shell

modules/external_command/command_processor/path_follow_command_processor/
├── path_follow_command_processor
    ├── conf                    // Parameter configuration files
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── path_follow_command_processor.cc        // Program entry source code
    ├── path_follow_command_processor.h         // Program entry header file
    └── README.md               // Documentation
```

## Plugin

### PathFollowCommandProcessor

apollo::planning::PathFollowCommandProcessor

#### Input

The plugin has two kinds of input:

1. **Executing navigation commands**: users call commands through a Cyber service to execute driving along a specified route.

| Service name                             | Request type                                   | Response type                              | <div style="width: 300pt">Description</div> |
| -------------------------------------- | --------------------------------------------- | ----------------------------------------- | ------------------------------------ |
| `/apollo/external_command/path_follow` | `apollo::external_command::PathFollowCommand` | `apollo::external_command::CommandStatus` | Command for driving along a specified route                 |

2. **Obtaining the command status**: the plugin listens through a Cyber Reader to the command execution status published by the planning module.

| Channel name                        | Type                                      | <div style="width: 300pt">Description</div>      |
| --------------------------------- | ----------------------------------------- | ----------------------------------------- |
| `/apollo/planning/command_status` | `apollo::external_command::CommandStatus` | Topic for receiving the command execution status published by the planning module |


#### Output

The plugin's output is the final result of command conversion:

| Channel name                 | Type                                | <div style="width: 300pt">Description</div>                       |
| -------------------------- | ----------------------------------- | ---------------------------------------------------------- |
| `/apollo/planning/command` | `apollo::planning::PlanningCommand` | Internal command converted from an external command with a navigation action, sent to the planning module |

#### Configuration

| File path                                                                                      | Type/Structure                                          | <div style="width: 300pt">Description</div>                         |
| --------------------------------------------------------------------------------------------- | -------------------------------------------------- | ------------------------------------------------------------ |
| `modules/external_command/command_processor/path_follow_command_processor/conf/config.pb.txt` | `apollo::external_command::CommandProcessorConfig` | Configuration file: channel or service names and other input/output information of the external command processor |

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="external-command-path-follow" type="binary">external-command-path-follow</depend>
```
##### Plugin loading configuration
All external command plugins are loaded and run in the external-command-process package. To support handling PathFollowCommand external commands, add the following configuration to the configuration file modules/external_command/process_component/conf/config.pb.txt:
```shell
processor: "apollo::external_command::PathFollowCommandProcessor"
```
