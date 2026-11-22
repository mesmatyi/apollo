# external-command-processor-base

## Introduction

`external-command-processor-base` is the package of the external command processing base classes. It contains the interface definitions and implementations of the base classes.

![](./docs/images/class.png)

### CommandProcessorBase
The base class `apollo::external_command::CommandProcessorBase` mainly contains the following implementations:
1. It defines the interface of external command processing classes:
```C++
virtual bool GetCommandStatus(int64_t command_id, CommandStatus* status) const = 0;
```
This function returns the current execution status of a command in real time: in progress, completed, or an error occurred.
2. It creates an apollo::cyber::Node, which subclasses can conveniently use when subscribing to their input topics.
3. It implements a method for reading configuration parameters from the configuration file:
```C++
const CommandProcessorConfig& GetProcessorConfig() const;
```

### MotionCommandProcessorBase
`apollo::external_command::MotionCommandProcessorBase` is a template base class for motion command processing. It mainly contains the following implementations:
1. It overrides the Init function to initialize the `apollo::cyber::Reader`, `apollo::cyber::Writer`, or `apollo::cyber::Service` related to navigation commands.
2. It implements the base class function GetCommandStatus, which obtains the command execution status by subscribing to `apollo::external_command::CommandStatus` and matching command_id.
3. It defines the pure virtual function Convert, which converts the input navigation command into `apollo::routing::RoutingRequest`. This is because, if an external command is a point-to-point drive along the road, a routing path must be searched first, and the routing module takes RoutingRequest as its input format, so the external command must first be converted into a RoutingRequest. If the external command does not contain a point-to-point drive along the road (for example, driving along a specified route), no conversion is needed.
```C++
virtual bool Convert(const std::shared_ptr<T>& command, std::shared_ptr<apollo::routing::RoutingRequest>& routing_request) const = 0;
```
4. It implements the interface for processing external commands. When an external command request is received, the routing request it contains is converted first; if there is a routing request, the routing module is called to search for a route; finally, any special commands it contains are processed, which is implemented in the subclasses.
```C++
void OnCommand(const std::shared_ptr<T>& command, std::shared_ptr<CommandStatus>& status);
```
5. It defines the interface for processing special commands, which lets subclasses handle their own commands specially.
```C++
virtual bool ProcessSpecialCommand(const std::shared_ptr<T>& command, const std::shared_ptr<apollo::planning::PlanningCommand>& planning_command) const = 0;
```
   
## Directory Structure

```shell

modules/external_command/command_processor/command_processor_base/
├── command_processor_base
    ├── docs                             // Documentation
    ├── proto                            // Definitions of common (global) parameter configuration structures
    ├── BUILD                            // Build rules file
    ├── cyberfile.xml                    // Package management configuration file
    ├── command_processor_base.cc        // Source code of the external command processing base class
    ├── command_processor_base.h         // Header file of the external command processing base class
    ├── motion_command_processor_base.h  // Header file of the motion command processing base class
    └── README.md                        // Documentation
```
