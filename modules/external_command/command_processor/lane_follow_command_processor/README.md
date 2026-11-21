# external-command-lane-follow

## Introduction

`external-command-lane-follow` is the processing module that handles and forwards the external command `apollo::external_command::LaneFollowCommand`. It receives LaneFollowCommand commands invoked through a Cyber service, extracts the point-to-point driving information from them, converts it into the `apollo::planning::PlanningCommand` channel message required by the planning module, and publishes it to the planning module.

`apollo::external_command::LaneFollowCommand` is the most commonly used driving command of planning and is suitable for point-to-point driving along the road on structured roads. It mainly contains the following information:
- **header**: the command message header; information can be injected with the apollo::common::util::FillHeader method
- **command_id**: the unique identifier of the command
- **is_start_pose_set**: whether the start point is specified. If true, the first routing point is used as the start point; otherwise the vehicle's current position is used as the start point
- **way_point**: the specified waypoints
- **end_pose**: the specified end point
- **blacklisted_lane**: the specified list of lane segments that cannot be traveled
- **blacklisted_lane**: the specified list of roads that cannot be traveled
- **target_speed**: the specified cruising speed

LaneFollowCommandProcessor inherits from the template class MotionCommandProcessorBase<LaneFollowCommand> and therefore has the following features of the parent class:
- **An interface for obtaining the command execution status**: the GetCommandStatus function returns the real-time execution status of a command.
- **Receiving external commands and converting them into planning input**: OnCommand is the callback invoked when a user calls the external interface through a Cyber service. After converting the external command data into a PlanningCommand, it publishes it through planning_command_writer_.

In particular, LaneFollowCommandProcessor implements the conversion of LaneFollowCommand. There are two main cases:
- **The vehicle is within the range of the lane lines**: This case is simple. For the start and end positions in the command, the nearest lanes are found by querying the map and sent as the start and end positions of the `apollo::routing::RoutingRequest`. If the external command LaneFollowCommand does not specify a start position, the vehicle's current pose is used as the start position.
- **The vehicle is outside the range of the lane lines**: The vehicle is currently far from the road. In this case the vehicle must first be navigated to the nearest lane line, and then perform point-to-point driving along the road. This is done in two steps: 1. navigate the vehicle to the nearest lane line with the openspace algorithm; 2. drive point to point along the road from the point on the nearest lane line. Note that although the vehicle actually drives in two steps, the converted PlanningCommand still only needs to contain a single point-to-point RoutingRequest command. When the planning module runs, it automatically switches to the ParkAndGo scenario based on the vehicle's current localization to navigate the vehicle to the nearest lane line. In LaneFollowCommandProcessor, the case where no start position on a lane line is specified must be handled: according to the vehicle's current pose, the nearest point on the lane lines is matched. If the nearby lane line is a two-way single-lane road, two points will be matched. In this case, based on the routing results between each matched point and the end point, the one with the shortest routing path is chosen as the start point of the path.

## Directory Structure

```shell

modules/external_command/command_processor/lane_follow_command_processor/
├── lane_follow_command_processor
    ├── conf                    // Parameter configuration files
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── lane_follow_command_processor.cc        // Program entry source code
    ├── lane_follow_command_processor.h         // Program entry header file
    └── README.md               // Documentation
```

## Plugin

### LaneFollowCommandProcessor

apollo::planning::LaneFollowCommandProcessor

#### Input

The plugin has two kinds of input:

1. **Executing navigation commands**: users call commands through a Cyber service to execute point-to-point driving along the road.

| Service name                         | Request type                                         | Response type                                         |<div style="width: 300pt">Description</div>                                                                                                               |
| ---------------------------------- | -------------------------------------------- | -------------------------------------------- |-------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/external_command/lane_follow`               | `apollo::external_command::LaneFollowCommand`   | `apollo::external_command::CommandStatus` | Command for point-to-point driving along the road |

2. **Obtaining the command status**: the plugin listens through a Cyber Reader to the command execution status published by the planning module.

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/command_status`                     | `apollo::external_command::CommandStatus`             | Topic for receiving the command execution status published by the planning module |


#### Output

The plugin's output is the final result of command conversion (PlanningCommand) and an intermediate result (RoutingResponse):

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/planning/command`                     | `apollo::planning::PlanningCommand`             | Internal command converted from an external command with a navigation action, sent to the planning module |
| `/apollo/routing_response`      | `apollo::routing::RoutingResponse`     | For external commands that drive point to point along lane lines on the HD map, the routing path generated during preprocessing, used for HMI display  |

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/external_command/command_processor/lane_follow_command_processor/conf/config.pb.txt`                 | `apollo::planning::CommandProcessorConfig`              | Configuration file: channel or service names and other input/output information of the external command processor  |

#### Usage
##### Adding the package dependency
If you run Apollo in package management mode, add the dependency on the plugin to the cyber.xml file of the package management:
```shell
<depend repo_name="external-command-lane-follow" type="binary">external-command-lane-follow</depend>
```
##### Plugin loading configuration
All external command plugins are loaded and run in the external-command-process package. To support handling LaneFollowCommand external commands, add the following configuration to the configuration file modules/external_command/process_component/conf/config.pb.txt:
```shell
processor: "apollo::external_command::LaneFollowCommandProcessor"
```
