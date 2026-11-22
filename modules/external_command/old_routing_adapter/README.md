# old-routing-adpter

## Introduction

`old-routing-adpter` is a module that adapts and stays compatible with RoutingRequest from versions before 9.0. In the new version, the command for point-to-point driving along lane lines uses `apollo::external_command::LaneFollowCommand`, and the user only needs to specify the position and heading of the end point to send a point-to-point navigation command. To stay compatible with the old `apollo::routing::RoutingRequest` command, the `old-routing-adpter` module needs to subscribe to this topic, convert the RoutingRequest into the new `apollo::external_command::LaneFollowCommand` command, and send it with `apollo::cyber::Client` to the external command processing module `apollo::external_command::ExternalCommandProcessComponent`.

![](./docs/images/old_routing_adapter.png)

## Directory Structure

```shell

modules/external_command/old_routing_adapter/
├── old_routing_adapter
    ├── conf                    // Parameter configuration files
    ├── dag                     // Module startup files (mainboard)
    ├── docs                    // Documentation
    ├── launch                  // Module startup files (cyber_launch)
    ├── proto                   // Definitions of common (global) parameter configuration structures
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── old_routing_adapter.cc  // Source code of the routing adapter
    ├── old_routing_adapter.h   // Header file of the routing adapter
    └── README.md               // Documentation
```

## Module

### OldRoutingAdapter

apollo::planning::OldRoutingAdapter

#### Input

The input of the `old-routing-adpter` module is the old `apollo::routing::RoutingRequest`.

| Channel name                             | Type                                          | <div style="width: 300pt">Description</div>             |
| -------------------------------------- | --------------------------------------------- | ------------------------------------------------ |
| `/apollo/routing_request`                     | `apollo::routing::RoutingRequest`             | Old-version RoutingRequest command, containing the poses of the start and end points and the corresponding Lane information; for a parking command, it contains the information about the parking spot |

#### Output

The `old-routing-adpter` module converts the old `apollo::routing::RoutingRequest` into the new external command interface and sends it through `apollo::cyber::Client`:

| Client name                         | Request type                                         | Response type                                         |<div style="width: 300pt">Description</div>                                                                                                               |
| ---------------------------------- | -------------------------------------------- | -------------------------------------------- |-------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/external_command/lane_follow`               | `apollo::external_command::LaneFollowCommand`   | `apollo::external_command::CommandStatus` | External command converted from a RoutingRequest containing a point-to-point command |
| `/apollo/external_command/valet_parking`               | `apollo::external_command::ValetParkingCommand`   | `apollo::external_command::CommandStatus` | External command converted from a RoutingRequest containing a parking command |

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/external_command/old_routing_adapter/conf/config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file, containing the names of input and output channels or services  |

#### Usage

##### Starting with mainboard

```shell
mainboard -d modules/external_command/old_routing_adapter/dag/old_routing_adapter.dag
```

##### Starting with cyber_launch

```shell
cyber_launch start modules/external_command/old_routing_adapter/launch/old_routing_adapter.launch
```
