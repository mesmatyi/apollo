# canbus

## Introduction

- The main purposes of the CANBUS component are: to parse the chassis CAN bus data and obtain vehicle information (such as vehicle speed, gear, steering wheel angle, running status, and safety information); to receive commands from the control module, parse the control commands into chassis CAN bus data, and send them to the vehicle for motion control; to execute the chassis interaction logic, enabling or exiting autonomous driving; and to perform chassis safety monitoring, monitoring situations such as chassis faults or takeovers.

![canbus_structure](./docs/images/canbus_structure.png)

- The main implementation flow of the Canbus component is to create a vehicle-model-based factory object and call the interface methods of the vehicle factory to implement initialization, to process control commands
  `apollo::control::ControlCommand`, and to parse chassis data and write it into
  `modules/common_msgs/chassis_msgs/chassis.proto`. The vehicle factory class `apollo::canbus::AbstractVehicleFactory`
  defines methods for initialization, starting and stopping, parsing control commands, parsing chassis commands, publishing chassis messages, publishing parsed chassis message data, updating the heartbeat, and so on.
  These methods are implemented by calling the vehicle controller object. The vehicle controller `apollo::canbus::VehicleController` defines methods for initialization, starting and stopping,
  updating chassis data from chassis feedback messages, processing control commands, processing chassis commands, and enabling the vehicle, and implements the control logic of the vehicle's throttle, brake, steering, gear, parking,
  lights, and so on.

- Each vehicle model inherits the vehicle factory class `apollo::canbus::AbstractVehicleFactory` and the vehicle controller class `apollo::canbus::VehicleController`,
  creates a `CanClient` client and `CanSender` and `CanReceiver` objects, and implements the corresponding initialization, chassis update, control command processing, and other methods.
  It also creates a message manager `apollo::drivers::canbus::MessageManager` object to manage the sending/receiving messages of that vehicle model. The data format definition of each message of the vehicle model
  inherits `apollo::drivers::canbus::ProtocolData` and implements the message protocol parsing. The code of each vehicle model is organized through package management,
  and the specific vehicle model implementations can be found in `modules/canbus_vehicle`.

- The `CanClient` client is an instance of the CAN card hardware driver, shared by different sensors that use the CAN bus protocol. If the existing CAN card drivers do not meet your needs, you can implement a new CAN card driver in the `modules/drivers/canbus/can_client` folder by inheriting the `apollo::drivers::canbus::CanClient` class. The `CanClient` client can be found in `modules/drivers/canbus/can_client/`.

## File Organization Structure and Description

```shell
modules/canbus
├── common                  // Module-wide gflag definitions
├── conf                    // Module configuration files, parameter file directory
├── dag                     // Module startup file (mainboard)
├── launch                  // Module startup file (cyber_launch)
├── proto                   // Configuration files defined by the component
├── testdata                // Data files for unit tests
├── tools                   // Debugging tools such as teleop
├── vehicle                 // Base class code files of the vehicle chassis (canbus_vehicle) component
├── BUILD                   // Build rules file
├── canbus_component.cc     // Code file of the component implementation
├── canbus_component.h      // Code file of the component implementation
├── canbus_test.cc          // Component unit test file
├── cyberfile.xml           // Package management configuration file
├── README_cn.md            // Documentation
└── README.md               // Documentation
```

## Module Inputs, Outputs, and Configuration

### CanbusComponent Component

apollo::canbus::CanbusComponent

#### Input

| Channel name              | Type                                       | Description              |
| ------------------------- | ------------------------------------------ | ------------------------ |
| `/apollo/control`         | `apollo::control::ControlCommand`          | Control command          |
| `/apollo/guardian`        | `apollo::guardian::GuardianCommand`        | Safety command           |
| `/apollo/chassis_control` | `apollo::external_command::ChassisCommand` | External chassis command |

#### Output

| Channel name             | Type                      | Description                                                                                                                |
| ------------------------ | ------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `/apollo/chassis`        | `apollo::canbus::Chassis` | Vehicle chassis information interface data, including vehicle speed, steering wheel angle, gear, chassis status, and more |
| `/apollo/chassis_detail` | `apollo::${Vehicle_Type}` | Detailed vehicle chassis information, showing the parsed data of the sent and received chassis messages                    |

#### Configuration Files

| File path                                | Type/Structure               | Description                                                       |
| ---------------------------------------- | ---------------------------- | ----------------------------------------------------------------- |
| `modules/canbus/conf/canbus_conf.pb.txt` | `apollo::canbus::CanbusConf` | Configuration file of `apollo::canbus::CanbusComponent`           |
| `modules/canbus/conf/canbus.conf`        | `gflags`                     | Command-line parameter configuration                              |

#### Flags

| flagfile                                 | Type | Description                                      |
| ---------------------------------------- | ---- | ------------------------------------------------ |
| `modules/canbus/common/canbus_gflags.cc` | `cc` | Flags variable definition file of the Canbus component |
| `modules/canbus/common/canbus_gflags.h`  | `h`  | Flags declaration file of the Canbus component   |

#### Usage

##### Modify the `modules/canbus/conf/canbus.conf` configuration file

- Turn on chassis_detail: after canbus starts, the `/apollo/chassis_detail` message is output. It is recommended to turn this on when debugging chassis data
```
--enable_chassis_detail_pub
```
- Turn off chassis_detail: it does not need to be on when not debugging the chassis
```
--noenable_chassis_detail_pub
```
- Turn off guardian control commands: this is off by default; when off, `/apollo/control` data is received. Note: if turned on, `/apollo/guardian` data is received, and the guardian module needs to be started
```
--noreceive_guardian
```

##### Modify the `modules/canbus/conf/canbus_conf.pb.txt` configuration file

Configure can_card_parameter: configure whichever can card you use. The example below uses a HERMES_CAN device, of PCI type, with port number 0 and a maximum of 8 ports.
```
can_card_parameter {
  brand: HERMES_CAN
  type: PCI_CARD
  channel_id: CHANNEL_ID_ZERO
  num_ports: 8
  interface: NATIVE
}
```

##### Start with mainboard

```shell
mainboard -d modules/canbus/dag/canbus.dag
```

##### Start with cyber_launch

```shell
cyber_launch start modules/canbus/launch/canbus.launch
```
