# external-command-valet-parking

## Introduction

`external-command-valet-parking` is the processing module that handles and forwards the external command `apollo::external_command::ValetParkingCommand`. It receives ValetParkingCommand commands invoked through a Cyber service, extracts the parking information from them, converts it into the `apollo::planning::PlanningCommand` channel message required by the planning module, and publishes it to the planning module.

`apollo::external_command::ValetParkingCommand` is suitable for scenarios in which the vehicle parks in a specified parking spot on the map. It mainly contains the following information:
- **header**: the command message header; information can be injected with the apollo::common::util::FillHeader method
- **command_id**: the unique identifier of the command
- **is_start_pose_set**: whether the start point is specified. If true, the first routing point is used as the start point; otherwise the vehicle's current position is used as the start point
- **way_point**: the specified waypoints
- **end_pose**: the specified end point
- **blacklisted_lane**: the specified list of lane segments that cannot be traveled
- **blacklisted_lane**: the specified list of roads that cannot be traveled
- **parking_spot_id**: the specified parking spot ID
- **target_speed**: the specified cruising speed

ValetParkingCommandProcessor inherits from the template class MotionCommandProcessorBase<ValetParkingCommand> and therefore has the following features of the parent class:
- **An interface for obtaining the command execution status**: the GetCommandStatus function returns the real-time execution status of a command.
- **Receiving external commands and converting them into planning input**: OnCommand is the callback invoked when a user calls the external interface through a Cyber service. After converting the external command data into a PlanningCommand, it publishes it through planning_command_writer_.

ValetParkingCommandProcessor implements the conversion of ValetParkingCommand. The main processing is as follows:
- **Convert**: Extracts the along-road driving information from ValetParkingCommand. If the start position of the vehicle sending the ValetParkingCommand is not near the parking spot, the ValetParkingCommand involves driving along lane lines from the start position, stopping at a parking preparation point, and then parking. Otherwise, parking is performed directly. If the vehicle must first drive along lane lines to the parking preparation point, Convert extracts the RoutingRequest information and puts it into the PlanningCommand, to be executed by the planning module. The extraction has the following main steps:
  ![](./docs/images/parking_stages.png)
  - Handle the start position. If no start position is specified in the ValetParkingCommand, the vehicle's current position is used as the start position.
  - Compute the parking preparation point. Get the parking spot ID "parking_spot_id" from the ValetParkingCommand, then get the parking spot's position from the map, find the lane line nearest to the parking spot, and take a point at a certain distance from the projection of the parking spot onto the lane line as the parking preparation point. The vehicle first drives along the road to the parking preparation point and stops, and then plans the parking route with the openspace algorithm.
  - Extract the segment from the start position to the parking preparation point as a RoutingRequest and store it in the lane_follow_command of `apollo::planning::PlanningCommand`.
- **ProcessSpecialCommand**: Besides the lane_follow_command needed in the Approach Stage, the Parking Stage needs the parking spot information. The main processing of ProcessSpecialCommand is to put the parking spot information, that is, "parking_spot_id", into the parking_command of `apollo::planning::PlanningCommand`.

## Directory Structure

```shell

modules/external_command/command_processor/valet_parking_command_processor/
├── valet_parking_command_processor
    ├── conf                    // Parameter configuration files
    ├── docs                    // Content referenced by the documentation
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── valet_parking_command_processor.cc        // Program entry source code
    ├── valet_parking_command_processor.h         // Program entry header file
    └── README.md               // Documentation
```

## Plugin

### ValetParkingCommandProcessor

apollo::planning::ValetParkingCommandProcessor

#### Input

The plugin has two kinds of input:

1. **Executing navigation commands**: users call commands through a Cyber service to execute the parking task.

| Service name                         | Request type                                         | Response type                                         |<div style="width: 300pt">Description</div>                                                                                                               |
| ---------------------------------- | -------------------------------------------- | -------------------------------------------- |-------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/external_command/valet_parking`               | `apollo::external_command::ValetParkingCommand`   | `apollo::external_command::CommandStatus` | Parking command |

2. **Obtaining the command status**: the plugin listens through a Cyber Reader to the command execution status published by the planning module.

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/command_status`                     | `apollo::external_command::CommandStatus`             | Topic for receiving the command execution status published by the planning module |


#### Output

The plugin's output is the final result of command conversion (PlanningCommand) and an intermediate result (RoutingResponse):

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/command`                     | `apollo::planning::PlanningCommand`             | Internal command converted from the parking command, sent to the planning module |
| `/apollo/routing_response`      | `apollo::routing::RoutingResponse`     | For external commands that drive point to point along lane lines on the HD map, the routing path generated during preprocessing, used for HMI display  |

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/external_command/command_processor/valet_parking_command_processor/conf/config.pb.txt`                 | `apollo::planning::CommandProcessorConfig`              | Configuration file: channel or service names and other input/output information of the external command processor  |

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="external-command-valet-parking" type="binary">external-command-valet-parking</depend>
```
##### Plugin loading configuration
All external command plugins are loaded and run in the external-command-process package. To support handling ValetParkingCommand external commands, add the following configuration to the configuration file modules/external_command/process_component/conf/config.pb.txt:
```shell
processor: "apollo::external_command::ValetParkingCommandProcessor"
```
