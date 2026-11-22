# canbus-vehicle-lincoln

## Introduction

- The canbus-vehicle-lincoln package is the implementation package for adapting the chassis protocol of the Lincoln MKZ vehicle model. It is not a standalone component; it is a concrete implementation that inherits from the vehicle factory class AbstractVehicleFactory and the vehicle controller class VehicleController in the canbus package, and is a plugin of the canbus module. The method call logic implemented for the Lincoln model is defined by the call logic of the canbus component; for the related flow, refer to the [canbus component documentation](../../canbus/README_cn.md).
- The figure below shows the implementation framework flow of the [lincoln](./lincoln) vehicle model package.

![canbus_vheicle_structure](./docs/images/canbus_vehicle_structure.png)

- In the Lincoln vehicle model implementation, there are 3 independent threads to pay attention to:

    (1) The CanSender thread that sends control messages: the CAN data that Apollo sends to the vehicle chassis is called control messages. CanSender creates an independent thread that sends them according to the protocol message period, and the message data being sent is updated by the CanSender::Update method;

    (2) The CANReceiver thread that receives chassis messages: the chassis information CAN data sent by the vehicle is called feedback messages. CanReceiver creates an independent thread to receive them (the receiving mode can be blocking or non-blocking, depending on which CanClient is used). MessageManager calls the Prase method of ProtocolData to parse the received chassis data and update the Lincoln.proto data;

    (3) The real-time chassis status monitoring thread: in LincolnController, a separate watchdog thread is created. The SecurityDogThreadFunc() method monitors the chassis driving mode and chassis faults in real time. If the chassis is detected to have exited autonomous driving (been taken over) or to have a fault, the Emergency() method is executed, which initializes all control messages and sets Chassis::DrivingMode to Emergency or Chassis_Error.

- The protocol messages of the Lincoln vehicle model all inherit from the parent class ProtocolData. Control messages and feedback messages inherit and implement different methods; to tell whether a message is a control message or a feedback message, look at which methods it implements.

    | Method | Description | Control message | Feedback message |
    | --- | --- | --- | --- |
    | GetPeriod | Defines the message period | √ | × |
    | Reset | Defines the default data of the message | √ | × |
    | UpdateData | Defines the update data of the message | √ | × |
    | UpdateData_Heartbeat | Defines the update content of the heartbeat signal | √ | × |
    | Prase | Defines the protocol data parsing of the message | × or √ | √ |

    For example, `Brake60` is a control message protocol parser, and `Brake61` is a feedback message protocol parser.

- The `/apollo/chassis` channel is published by writing the necessary Parse data of the ProtocolData protocols into apollo::Chassis through the Chassis method in VehicleController;

- The `/apollo/chassis_detail` channel is written into the Proto data of the corresponding vehicle model after calling ProtocolData's Parse; check the Proto file of the corresponding vehicle model for details. The data of that vehicle model's proto type is then published.

- The CanClient client is an instance of the CAN card hardware driver, shared by different sensors that use the CAN bus protocol. If the existing CAN card drivers do not meet your needs, you can implement a new CAN card driver in the `drivers/canbus/can_client` folder by inheriting the CanClient class.


## File Organization Structure and Description
Because the file organization structure is the same for every vehicle model, the lincoln model is used as an example here; you can view the directory structures of other vehicle models yourself.
```shell
modules/canbus_vehicle/
└── lincoln                                 // Implementation files for lincoln model protocols and vehicle control
    ├── BUILD                               // Build rules file
    ├── cyberfile.xml                       // Package management configuration file
    ├── lincoln_controller.cc               // Implementation file of the lincoln model controller
    ├── lincoln_controller.h                // Declaration file of the lincoln model controller
    ├── lincoln_controller_test.cc          // Unit test file of the lincoln model controller
    ├── lincoln_message_manager.cc          // Implementation file of the lincoln model send/receive message manager
    ├── lincoln_message_manager.h           // Declaration file of the lincoln model send/receive message manager
    ├── lincoln_message_manager_test.cc     // Unit test file of the lincoln model send/receive message manager
    ├── lincoln_vehicle_factory.cc          // Implementation file of the lincoln model vehicle factory
    ├── lincoln_vehicle_factory.h           // Declaration file of the lincoln model vehicle factory
    ├── lincoln_vehicle_factory_test.cc     // Unit test file of the lincoln model vehicle factory
    ├── docs/                               // Files related to the documentation
    ├── README_cn.md                        // Package documentation
    ├── proto/                              // Folder for vehicle model protocol variable definitions
    │   ├── BUILD                           // Build rules file
    │   └── lincoln.proto                   // Definition file of the lincoln model protocol variable types
    ├── protocol/                           // Folder for protocol parsing implementations
    │   ├── brake_60.cc                     // Implementation file of the brake message control protocol parsing
    │   ├── brake_60.h                      // Implementation file of the brake message control protocol parsing
    │   ├── brake_60_test.cc                // Unit test file of the brake message control protocol parsing
    │   ├── brake_61.cc                     // Implementation file of the brake message feedback protocol parsing
    │   ├── brake_61.h                      // Implementation file of the brake message feedback protocol parsing
    │   ├── brake_61_test.cc                // Unit test file of the brake message feedback protocol parsing
    │   .
    │   .
    │   .
    └── testdata                            // Folder for unit test data
```


## Module Inputs, Outputs, and Configuration

### canbus-vehicle-lincoln Plugin

#### Input
|  Channel name  | Type  | Description  |
| ---- | ---- | ---- |
| `/apollo/control` | apollo::control::ControlCommand | The canbus module subscribes to this message, and LincolnVehicleFactory inherits the AbstractVehicleFactory base class method UpdateCommand(ControlCommand) to pass in this parameter as the input of the lincoln vehicle model plugin |
| `/apollo/chassis_control`| apollo::ChassisCommand | The canbus module subscribes to this message, and LincolnVehicleFactory inherits the AbstractVehicleFactory base class method UpdateCommand(ChassisCommand) to pass in this parameter as the input of the lincoln vehicle model plugin |

#### Output
| Channel name  | Type  | Description |
| ---- | ---- | ---- |
| `/apollo/chassis_detail` | apollo::lincoln::Lincoln | Detailed lincoln vehicle chassis information, showing the parsed data of the chassis bus messages |

#### Configuration Files
| File path | Type/Structure | Description |
| ---- | ---- | ---- |

| `modules/canbus/conf/canbus.conf` | `command line flags` | Command-line parameter configuration |

#### Flags
| flagfile | Type | Description |
| ---- | ---- | ------ |
| `canbus_gflags.cc` | `flags` | Flags definition file of the Canbus component |
| `canbus_gflags.h` | `declare` | Flags declaration file of the Canbus component |

#### Usage

##### Modify the vehicle model package loading configuration
In `modules/canbus/conf/canbus.conf`, modify the `--load_vehicle_library` and `--load_vehicle_class_name` parameters to load the Lincoln vehicle model package. The parameters are configured differently for package management and source code:
- // Under package management
```
--load_vehicle_library=/opt/apollo/neo/lib/modules/canbus_vehicle/lincoln/liblincoln_vehicle_factory_lib.so
--load_vehicle_class_name=LincolnVehicleFactory
```

- // Under source code
```
--load_vehicle_library=/apollo/bazel-bin/modules/canbus_vehicle/lincoln/liblincoln_vehicle_factory_lib.so
--load_vehicle_class_name=LincolnVehicleFactory
```

##### Start with mainboard

```shell
mainboard -d modules/canbus/dag/canbus.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/canbus/launch/canbus.launch
```
