external-command-zone-cover
==============

## Introduction

The main function of the external-command-zone-cover plugin is to wrap and process external interface commands. Users can send control commands to the PNC module through a custom interface, and the PNC module executes the commands after processing. It is implemented mainly through Service communication: a Client sends external PNC control requests, and the Service processes the Client's request and hands it to the PNC module for execution.

### Convert
This function is not yet implemented; `external-command-zone-cover` does no processing in it.

### ProcessSpecialCommand
This function is mainly called in the `OnCommand` function in the `motion_command_processor_base.cc` file. The main purpose of `OnCommand` is to pass through the Client's Request. As the main function implementing the Service, `command` is the Client's Request, and the Client is created in the `Init()` function in `external_command_wrapper_demo`. The `ProcessSpecialCommand` function mainly passes the request received from the Client through to `planning_command`, which is later written to the `apollo::planning::PlanningCommand` channel for the PNC module to execute the request.

## File Organization and Description

```shell
command_processor/command_processor/zone_cover_command_processor/
├── conf/                                                 // Controller configuration parameter files
├── BUILD                                                 // Build rules file
├── cyberfile.xml                                         // Plugin package management configuration file
├── zone_cover_command_processor.cc                       // Processor implementation file
├── zone_cover_command_processor.h                        // Processor implementation file
├── plugins.xml                                           // Plugin configuration file
└── README.md                                             // Documentation
```


## Module Input, Output, and Configuration

### external-command-chassis plugin

#### Input


| Service name                            | Request type                                  | Response type                              | <div style="width: 300pt">Description</div> |
| ------------------------------------- | -------------------------------------------- | ----------------------------------------- | ------------------------------------ |
| `/apollo/external_command/zone_cover` | `apollo::external_command::ZoneCoverCommand` | `apollo::external_command::CommandStatus` | Vehicle Command request in open-area scenarios          |
#### Input
| Channel name                       | Type                                    | Description                           |
| --------------------------------- | --------------------------------------- | ------------------------------ |
| `/apollo/planning/command_status` | apollo::external_command::CommandStatus | Actual status of the request made to the vehicle's Planning module |


#### Configuration file
| File path                                                                                | Type/Structure                                          | Description                                                    |
| --------------------------------------------------------------------------------------- | -------------------------------------------------- | ------------------------------------------------------- |
| `modules/external_command/command_processor/zone_cover_command_processor/config.pb.txt` | `apollo::external_command::CommandProcessorConfig` | Input and output channel configuration file of zone_cover_command_processor |
