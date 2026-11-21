# Apollo Vehicle Adaptation Tutorial
## Introduction
The method for adapting a new vehicle to Apollo described in this document helps developers quickly integrate a new vehicle so that they can develop software on Apollo. The drive-by-wire signal standards and requirements described here originate from the Apollo Open Vehicle Certification Platform. The platform currently supports vehicle models such as Lincoln, Lexus, GE3, WEY, Devkit and Neolix. Automakers and vehicle providers who wish to add their vehicles to the Apollo Open Vehicle Certification Platform can follow the specific drive-by-wire requirements in this document to become familiar with and prepare the vehicle's low-level control protocol, saving preparation time. Interested developers are welcome to visit the [Apollo Open Vehicle Certification Platform](http://apollo.auto/vehicle/certificate_cn.html) (http://apollo.auto/vehicle/certificate_cn.html) to learn more.

## Adapting a vehicle that meets the Apollo standard

 - This document mainly describes how to add a new vehicle that meets the Apollo standard in Apollo, so that developers can design chassis systems that meet Apollo's drive-by-wire requirements, develop chassis adaptation code for the Apollo software, and build the ability to quickly integrate with the Apollo software.
 - When adapting the CAN communication of a new vehicle, developers first need to design a chassis communication protocol that meets Apollo's drive-by-wire requirements. This part builds a DBC file of the vehicle chassis signals according to the Apollo drive-by-wire list: design the chassis-related interface definitions, with messages and signals in one-to-one correspondence with the DBC. After the DBC file is designed, use the DBC conversion code tool provided by Apollo to obtain a first version of the low-level canbus adaptation code, then add the control logic of the vehicle, set up the vehicle's low-level chassis signals and vehicle control signals in Apollo. Finally, use the canbus debugging tools to debug and verify the signals, ensuring that there are no problems at all in the communication between the vehicle's chassis signals and the upper layers of Apollo.

## 1. Vehicles that meet the Apollo drive-by-wire standard

### Vehicle DBC
The first task in developing a vehicle equipped with the Apollo system is to establish communication between Apollo and the vehicle's low-level systems. The vehicle uses the CAN bus (Controller Area Network) for communication among its controllers (ECUs, electronic control units), so when the Apollo control system is connected to the vehicle system, it also controls the vehicle through CAN bus communication. The vehicle needs to open up a portion of its chassis signals and configure them into a DBC file (Database file, a dedicated file for describing the data on the CAN bus, which makes it easy for users to quickly build the network structure of vehicle CAN communication and define the signals, messages and other information on the CAN bus) that meets Apollo's drive-by-wire standard requirements. Developers are recommended to use files edited and generated with CANdb++ of the CANoe software. The Apollo software processes the chassis DBC file to generate a chassis adaptation code template; through simple programming, developers can adapt the chassis signals opened by the vehicle into the Apollo software, enabling communication between Apollo and the vehicle chassis.

### DBC drive-by-wire standard in detail
 - When developing vehicle chassis control signals, developers need to refer to the vehicle drive-by-wire standards published by the vehicle certification platform on the Apollo official website (http://apollo.auto/docs/procedure_cn.html),
 Passenger vehicle standard: http://apollo-homepage.bj.bcebos.com/Apollo_by_wire_requirement.xlsx ;
Micro vehicle standard: https://apollohomepage.bj.bcebos.com/Apollo_by_wire_requirement_micro_car.xlsx
The vehicle drive-by-wire standard specifies the requirements for vehicle chassis signals, as well as the response time requirements of the chassis for each frame of control signal. Vehicle chassis signals must meet the requirements not only in function but also in performance, so that the chassis can satisfy the control requirements of a vehicle that conforms to the Apollo drive-by-wire standard.
 - When converting a vehicle chassis into a drive-by-wire chassis, developers should refer to the vehicle drive-by-wire standard to meet the control performance requirements, for example the steering signal must respond within 100 ms and the maximum overshoot must meet specific requirements. Please read the performance requirements in the drive-by-wire list carefully; during Apollo vehicle certification, the chassis performance will also be tested accordingly.

In the actual DBC development process, there are also specific requirements for the details of each signal definition. To save developers' time and improve the efficiency of vehicle certification, developers need to focus on understanding and paying attention to each signal. Apollo's requirements for vehicle chassis signals are shown in the table below:
![apollo_by_wire_requirement1](images/vehicle_adaption_tutorial/apollo_by_wire_requirement1.png)
![apollo_by_wire_requirement2](images/vehicle_adaption_tutorial/apollo_by_wire_requirement2.png)
![apollo_by_wire_requirement3](images/vehicle_adaption_tutorial/apollo_by_wire_requirement3.png)

Vehicle chassis control is the foundation of communication between Apollo and the chassis, and every control signal must be reliable and accurate. Manual takeover and out-of-range handling ensure that the vehicle can be taken over at any time during testing and use, guaranteeing driving safety when unexpected situations occur. In addition to the detailed signal requirements in the drive-by-wire list, **developers also need to pay special attention to the independence of the lateral and longitudinal enable signals**, **the chassis takeover logic and other special requirements**. The special functional requirements of the signals are described in detail below.

#### Steer-by-wire
1. Steering enable signal

(1) The steering enable signal controls whether the vehicle enters or exits the steering autonomous driving mode. This enable signal must be a signal that is **rising-edge triggered**, and in principle the enable signal is a single signal. If some developers have several different signals that must simultaneously control the steering to enter the autonomous driving state, the developers need to integrate the logic of the relevant enable signals at the low level themselves; in principle, there must be only one enable signal given to Apollo.

(2) The steering autonomous driving mode must be independent, and the enable signal must be independent, not associated with the enable signals of the longitudinal modules (such as drive, brake, gear and parking). The following must be satisfied: when the steering autonomous driving mode is enabled on its own, the longitudinal driving mode does not change, and when a steering wheel angle or speed control signal is sent, the vehicle responds to the corresponding angle and speed; when the steering autonomous driving mode is exited on its own, the longitudinal driving mode does not change, and when a steering wheel angle or speed control signal is sent, the vehicle does not respond;

(3) If the lateral control has been taken over, re-entering autonomous driving mode requires a rising-edge triggered signal.

2. Steering control signals

Target steering wheel angle: the signal that controls the steering wheel angle, in degrees (deg). Turning the steering wheel left (counterclockwise) is a positive value, and turning it right (clockwise) is a negative value.

Target steering wheel speed: most vehicles cannot open up this control signal interface. It is recommended to set the steering wheel speed to the maximum value to improve the system response time.

3. Steering feedback signals

Actual steering wheel angle feedback: feeds back the actual steering wheel angle. The feedback must be accurate, with left turns reported as positive values and right turns as negative values.

Steering driving mode feedback: feeds back the steering driving mode the vehicle is currently in. At least 3 modes are required: autonomous driving mode, manual driving mode and manually-taken-over mode.
 
Fault information feedback: feeds back whether there is a fault in the steering control system.

4. Manual takeover handling

(1) For passenger/commercial vehicles, when the steering wheel torque applied by a human exceeds the takeover threshold, all modules of the vehicle exit autonomous driving, and the steering driving mode feedback reports taken over; for chassis carts and similar vehicles, when the cart chassis receives a command from the remote controller to steer the vehicle, all modules of the vehicle exit autonomous driving. The steering takeover must be independent: at this time the steering driving mode feedback reports the manually-taken-over mode, and the drive driving mode and brake driving mode should report the manual driving mode;

(2) When steering autonomous driving is taken over manually, all modules exit autonomous driving mode.

5. Out-of-range handling

The principle of out-of-range handling is to refuse execution when out of range and exit autonomous driving mode.
When the control command sent by Apollo exceeds the defined range of the original control command, this is called out of range. When out of range occurs, the steering needs to exit autonomous driving mode.
For example, if the maximum range of the target steering wheel angle is defined as -500deg to 500deg, and the angle command sent by Apollo is not within this range, the chassis needs to refuse to execute this out-of-range command and exit autonomous driving mode.

#### Throttle-by-wire (drive)

1. Drive enable:

(1) The drive enable signal controls whether the vehicle enters or exits the drive autonomous driving mode. This enable signal must be a signal that is **rising-edge triggered**, and **in principle the enable signal is a single signal**. If some developers have several different signals that must simultaneously control the drive to enter the autonomous driving state, the developers need to integrate the logic of the relevant enable signals at the low level themselves; in principle, there must be only one enable signal given to Apollo.

(2) The drive enable signal must be independent, not associated with the steering module enable signal. The following must be satisfied: when the longitudinal autonomous driving mode is enabled on its own, the steering driving mode does not change, and when a drive control signal is sent, the vehicle responds to the corresponding control command; when the drive autonomous driving mode is exited on its own, the steering driving mode does not change, and when a drive control signal is sent, the vehicle does not respond;

(3) **In principle the drive enable signal is independent of the brake, gear and parking enables. According to past practical experience**, on some vehicles the longitudinal drive and brake enable signals are combined into one signal. Developers need to pay extra attention to such special cases and make sure that the longitudinal enable is separate and independent from the lateral enable. If the longitudinal modules are associated with each other, then drive, brake, gear and parking must enter autonomous driving mode together when enabled, and exit autonomous driving mode together when not enabled.

(4) If the lateral control has been taken over, re-entering autonomous driving mode requires a rising-edge triggered signal.

2. Drive control signals:

Target accelerator pedal position: the control quantity is the accelerator pedal position as a percentage, in the range 0-100%, where 100% means the accelerator pedal is pressed to its maximum position.

Target vehicle longitudinal acceleration and target vehicle drive torque are optional vehicle control signals.

3. Drive feedback signals:

Driving mode: feeds back the drive driving mode the vehicle is currently in. At least 3 modes are required: autonomous driving mode, manual driving mode and manually-taken-over mode.

Accelerator pedal position: must accurately feed back the actual current pedal position of the vehicle.

Longitudinal acceleration: must accurately feed back the current longitudinal acceleration of the vehicle.

Vehicle speed: must accurately feed back the current vehicle speed. Apollo requires the speed in m/s, **while vehicles generally report speed in km/h, in which case the unit conversion must be done separately after the DBC is converted into adaptation code**.

Wheel speed: for passenger vehicles, the relevant wheel speed signals must be provided, with the wheel speed magnitude in km/h. If possible, wheel movement direction feedback can be provided, with the Value Description in the following order: `0x00, FORWARD; 0x01, BACKWARD; 0x02, STANDSTILL; 0x03, INVALID`.

Engine/motor speed: must accurately feed back the current engine or motor speed, in r/min.

Fault information: feeds back whether there is a fault in the drive system.

4. Manual takeover:

(1) When the vehicle chassis receives a manually applied accelerator pedal command (or, for a chassis cart, an acceleration signal sent by the remote controller), all modules of the vehicle exit autonomous driving. The takeover must be independent: at this time the drive driving mode feedback reports the manually-taken-over mode, and the steering driving mode and brake driving mode should report the manual driving mode;

(2) When drive autonomous driving is taken over manually, all modules exit autonomous driving mode.

5. Out-of-range handling

When out of range occurs, the steering needs to exit autonomous driving mode. For example, if the target accelerator pedal position range is defined as 0-100%, and the pedal command sent by Apollo is not within this range, the chassis needs to refuse to execute this out-of-range command and exit autonomous driving mode.

#### Brake-by-wire

1. Brake enable:

(1) The brake enable signal controls whether the vehicle enters or exits the drive autonomous driving mode. This enable signal must be a signal that is **rising-edge triggered**, and in principle the enable signal is a single signal. If some developers have several different signals that must simultaneously control the steering to enter the autonomous driving state, the developers need to integrate the logic of the relevant enable signals at the low level themselves; in principle, there must be only one enable signal given to Apollo.

(2) The brake enable signal must be independent and cannot be associated with the steering module enable signal. The following must be satisfied: when the longitudinal autonomous driving mode is enabled on its own, the steering driving mode does not change, and when a brake control signal is sent, the vehicle responds to the corresponding control command; when the drive autonomous driving mode is exited on its own, the steering driving mode does not change, and when a brake control signal is sent, the vehicle does not respond;

(3) In principle the drive enable signal is independent of the drive, gear and parking enables. According to past practical experience, on some vehicles the longitudinal drive and brake enable signals are combined into one signal. Developers need to pay extra attention to such special cases and make sure that the longitudinal enable is separate and independent from the lateral enable. If the longitudinal modules are associated with each other, then drive, brake, gear and parking must enter autonomous driving mode together when enabled, and exit autonomous driving mode together when not enabled.

(4) If the lateral control has been taken over, re-entering autonomous driving mode requires a rising-edge triggered signal.

2. Brake control signals

Target brake pedal position: the control quantity is the pedal position as a percentage, in the range 0-100%, where 100% means the brake pedal is pressed to its maximum position.

Target deceleration: optional; the control quantity is the target deceleration of the vehicle.

Brake light control: optional; controls whether the brake light is on. The chassis of a typical vehicle turns on the brake light by default when braking.

3. Brake feedback signals

Driving mode: feeds back the brake driving mode the vehicle is currently in. At least 3 modes are required: autonomous driving mode, manual driving mode and manually-taken-over mode. 

Brake pedal position: the brake pedal position, as a percentage.

Brake light status: feeds back whether the brake light is on.

Fault information: feeds back whether there is a fault in the brake system.

4. Manual takeover

(1) When the vehicle chassis receives a manually applied brake pedal command (or, for a chassis cart, a deceleration signal sent by the remote controller), all modules of the vehicle exit autonomous driving. The takeover must be independent: at this time the drive driving mode feedback reports the manually-taken-over mode, and the steering driving mode and brake driving mode should report the manual driving mode;

(2) When drive autonomous driving is taken over manually, all modules exit autonomous driving mode.

5. Out-of-range handling

When out of range occurs, the steering needs to exit autonomous driving mode. For example, if the target brake pedal position range is defined as 0-100%, and the pedal command sent by Apollo is not within this range, the chassis needs to refuse to execute this out-of-range command and make all modes exit autonomous driving mode.

#### Gear-by-wire

1. Gear enable

The enable must be independent and not associated with lateral control. It may be associated with the longitudinal enable signals or enabled independently. The gear enable signal must be a rising-edge triggered signal, and in principle the enable signal is a single signal.

2. Gear control

The gear control signal must follow this order: `0x00: N; 0x01: D; 0x02: R; 0x03: P; 0x04: NONE`. No extra values need to be added.

3. Gear feedback

Gear information: feeds back the current gear signal, in the same order as the gear control signal: `0x00: N; 0x01: D; 0x02: R; 0x03: P; 0x04: NONE`. No extra values need to be added.

Fault information: feeds back whether there is a fault in the shifting system.

#### Parking-by-wire

1. Parking enable

The enable must be independent and not associated with lateral control. It may be associated with the longitudinal enable signals or enabled independently. The parking enable signal must be a rising-edge triggered signal, and in principle the enable signal is a single signal.

2. Parking control

When the parking signal is active, the electronic parking brake is engaged and the vehicle is parked; when the parking signal is inactive, the electronic parking brake is released and the vehicle is not parked.

3. Parking feedback

EPB switch status: feeds back whether the electronic parking brake is currently engaged or released.

Parking status feedback: feeds back whether the parking is currently in autonomous driving mode or manual control mode. At least 2 modes are required: `autonomous driving mode`, `manual driving mode`, `manually-taken-over mode`.

Parking system fault feedback: feeds back parking system faults.

#### Other body functions

These include drive-by-wire lighting, mainly controlling turning the high beam, low beam, turn signals and hazard warning lights on and off, together with the corresponding status feedback signals.

Wiper and horn control are also optional.

#### Vehicle VIN

The VIN is generally 17 characters in ASCII format, with each ASCII character taking 1 byte, and it needs to be sent out continuously in 3 message frames. However, the VIN does not need to be updated in real time, so when the system requests to enter autonomous driving, the VIN is sent out over the CAN bus and kept at that value without further updates, which also reduces the bus load.

### DBC file requirements

Once you are familiar with Apollo's requirements for vehicle chassis signals as described above, the second step is to edit the vehicle chassis signal database (DBC) file, setting the communication network structure and each signal's initial value, sign type, precision, size range, values and so on, which are then combined into the corresponding CAN communication messages to communicate with Apollo. The following uses the CANdb++ software to edit the DBC file; it has a good visual interface, and is currently available only on Windows.

Because the DBC file will later be converted into C++ code for the messages and signals defined by the chassis through Apollo's translation script tool, the names, comments and values of signals must conform to C++ language conventions when editing the DBC, to ensure that CANBUS communication will not fail to work during later debugging because of problems in the DBC file. According to the Apollo code requirements, we have summarized the following notes:
#### 1. It is recommended to name the control unit ACU
When defining the ECU names on the network, it is recommended to name the Apollo-side controller ACU (Apollo Control Unit).

![name](images/vehicle_adaption_tutorial/name.jpg)
#### 2. It is recommended that CAN signal IDs not exceed 2048
Currently, passenger vehicle CAN communication is recommended to use the standard frame format (CAN Standard 2.0); Apollo can support extended frames.

![ID](images/vehicle_adaption_tutorial/messageID_define.jpg)
#### 3. Comments must not contain carriage returns or line feeds, and comments must be in English
If a message has a comment, the comment must not contain line breaks, must not be written in Chinese, and must be in English.

![comment](images/vehicle_adaption_tutorial/comment.jpg)
##### 4. VAL_ (enumeration values) (Value Description) must be in English, must not have duplicate names, and must be letters or a combination of letters and digits, with no symbols.
For most status feedback signals and control signals, such as gear feedback and driving mode feedback, the signal needs to be defined in the Value Description item of the signal definition. The defined names must follow C++ naming conventions: they must be in English, must not have duplicate names, and must be letters or a combination of letters and digits, with no symbols. The figure below shows the Value Description definition of the gear feedback signal.

![gear1](images/vehicle_adaption_tutorial/gear1.jpg)
##### 5. For double-type feedback and control signals such as vehicle speed, wheel speed, acceleration and pedal position (percentage), the Value Description item in the DBC must be empty.
For real-time numeric feedback signals and numeric control signals, such as vehicle speed (actual speed), wheel speed feedback (actual wheel speed), pedal control (percentage) and steering angle control (actual angle value), nothing may be added to the Value Description item in the definition of such signals.

![val](images/vehicle_adaption_tutorial/val.jpg)
##### 6. For the range of the steering signal, fill in the accurate value range when defining it. Note that the precision (resolution) of the steering angle control should generally be no coarser than 0.05 deg, and the pedal percentage precision (factor) should be no larger than 0.1.
For the Byte Order of all messages, the signals within one DBC can only be defined uniformly: either all in Motorola format or all in Intel format.

![define](images/vehicle_adaption_tutorial/define.jpg)


## 2. Adapting the CANBUS code
### 1. Converting the DBC file into canbus template code
The Canbus adaptation code can be generated with an Apollo tool. Before converting the code, make sure the DBC is completed according to the DBC file requirements above, and open the dbc file with gedit and save it again, converting the encoding to UTF-8.

(1) Place the DBC file (such as ge3.dbc) in the specified directory, inside `/apollo_workspace/output/`.

(2) Modify the configuration file of the DBC conversion script. Taking the addition of the GE3 model as an example, in the `apollo/modules/tools/gen_vehicle_protocol` directory, copy the existing default `lincoln_conf.yml` file and rename it `ge3_conf.yml`, then modify this configuration file as shown below:

```yml
# Specify the dbc file, taking ge3.dbc as an example. Note that the absolute path of the file inside the apollo container must be specified here
# By default, put ge3.dbc in the /apollo_workspace/output/ folder
dbc_file: /apollo_workspace/output/ge3.dbc
# Define the name of the yml file that the dbc is converted into
protocol_conf: /apollo_workspace/output/ge3.yml
# Define the vehicle model name, customized according to the vehicle model
car_type: ge3
# Define the list of control message protocols, generally empty
sender_list: []
# The ECU network node name of the autonomous driving module in the dbc; the node name defined in ge3.dbc is SCU
sender: SCU
# Define which signal IDs do not need to be generated, generally empty
black_list: []
# Whether to generate the vehicle adaptation code based on apollo_demo.dbc; the dbc currently used is lincoln.dbc, and false means no
use_demo_dbc: false

# Define the folder where the code converted from ge3.dbc is generated; the default folder below is fine
output_dir: /apollo_workspace/modules/canbus_vehicle
# Define the canbus configuration file corresponding to the new vehicle model; the default folder below is fine
output_canbus_conf_dir: /apollo_workspace/output
```

(3) After completing the `ge3_conf.yml` configuration file, enter the Apollo container and check that the files are ready by executing the code:

```shell

ls -al /apollo_workspace/output
```
![ge3_check_output](images/vehicle_adaption_tutorial/ge3_check_output.png)

- Execute the generation script command
```python
gen output/ge3_conf.yml
```

- After the above script finishes, the terminal shows that 11 control protocols and 5 feedback protocols were generated.

![creat_ge3protocol](images/vehicle_adaption_tutorial/creat_ge3protocol.png)

- After the above script is executed, a `ge3` folder is generated in the `/apollo_workspace/modules/canbus_vehicle/` directory. This folder contains the vehicle protocol adaptation code converted from the ge3 protocol dbc.

```shell
/apollo_workspace/modules/canbus_vehicle/
|-- ge3
    |-- BUILD
    |-- cyberfile.xml
    |-- ge3_controller.cc
    |-- ge3_controller.h
    |-- ge3_message_manager.cc
    |-- ge3_message_manager.h
    |-- ge3_vehicle_factory.cc
    |-- ge3_vehicle_factory.h
    |-- proto
    |-- protocol
```
- In the `/apollo_workspace/output/` directory, a `ge3` folder is generated. The configuration files in the `canbus_conf` directory inside this folder are the canbus configuration files for the ge3 model; replace the contents of the related configuration files in the `/apollo/modules/canbus/conf/` folder with them.

```shell
/apollo_workspace/output/ge3
|-- canbus_conf
    |-- canbus.conf
    |-- canbus_conf.pb.txt
```

(4) Build the ge3 model

- Execute the following build command

```shell
buildtool build --opt -p modules/canbus_vehicle/ge3/
```
After the build completes, the preliminary adaptation code for the ge3 model is done.

### 2. Implementing the new vehicle control logic

- The adaptation code generated automatically in the previous step completes about 60% of the code development work. This step needs to complete the model-specific implementation logic by writing the control logic code in the `/apollo_workspace/modules/canbus_vehicle/ge3/ge3_controller.cc` file, mainly including:
- Implementation of the chassis() method: mainly vehicle speed, wheel speed, gear, pedal percentage, steering wheel angle, EPB status, turn signals, and horn and indicator light signals. Write the `Ge3.proto` information into `chassis.proto`, which implements the update of the content of the `/apollo/canbus/chassis` channel message.
- Set the autonomous driving mode and edit the related enable logic: both lateral and longitudinal enable autonomous driving EnableAutoMode(), both lateral and longitudinal exit autonomous driving DisableAutoMode(), only lateral enables autonomous driving EnableSteeringOnlyMode(), only longitudinal enables autonomous driving EnableSpeedOnlyMode()
- Add the necessary control signal method implementations, including gear control Gear(), brake control Brake(), throttle control Throttle() and steering control Steer(); other control signals include headlight control SetBeam(), horn control SetHorn, turn signal control SetTurningSignal() and electronic parking brake control SetEpbBreak().
- Add the `CheckResponse()` logic: implement the signal feedback of the vehicle's autonomous driving state mode in the chassis() method, then adjust the retry time and other functions in the `CheckResponse()` method.

- Implement the feedback message information in the chassis() method, writing the code as follows:

```C++
  // 3
  chassis_.set_engine_started(true);

  // check if there is not ge3, no chassis detail can be retrieved and return
  if (!chassis_detail.has_ge3()) {
    AERROR << "NO GE3 chassis information!";
    return chassis_;
  }
  Ge3 ge3 = chassis_detail.ge3();
  // 5
  if (ge3.has_scu_bcs_3_308()) {
    Scu_bcs_3_308 scu_bcs_3_308 = ge3.scu_bcs_3_308();
    if (scu_bcs_3_308.has_bcs_rrwheelspd()) {
      if (chassis_.has_wheel_speed()) {
        chassis_.mutable_wheel_speed()->set_is_wheel_spd_rr_valid(
            scu_bcs_3_308.bcs_rrwheelspdvd());
        chassis_.mutable_wheel_speed()->set_wheel_direction_rr(
            (WheelSpeed::WheelSpeedType)scu_bcs_3_308.bcs_rrwheeldirection());
        chassis_.mutable_wheel_speed()->set_wheel_spd_rr(
            scu_bcs_3_308.bcs_rrwheelspd());
      }
    }

    if (scu_bcs_3_308.has_bcs_rlwheelspd()) {
      if (chassis_.has_wheel_speed()) {
        chassis_.mutable_wheel_speed()->set_is_wheel_spd_rl_valid(
            scu_bcs_3_308.bcs_rlwheelspdvd());
        chassis_.mutable_wheel_speed()->set_wheel_direction_rl(
            (WheelSpeed::WheelSpeedType)scu_bcs_3_308.bcs_rlwheeldirection());
        chassis_.mutable_wheel_speed()->set_wheel_spd_rl(
            scu_bcs_3_308.bcs_rlwheelspd());
      }
    }

    if (scu_bcs_3_308.has_bcs_frwheelspd()) {
      if (chassis_.has_wheel_speed()) {
        chassis_.mutable_wheel_speed()->set_is_wheel_spd_fr_valid(
            scu_bcs_3_308.bcs_frwheelspdvd());
        chassis_.mutable_wheel_speed()->set_wheel_direction_fr(
            (WheelSpeed::WheelSpeedType)scu_bcs_3_308.bcs_frwheeldirection());
        chassis_.mutable_wheel_speed()->set_wheel_spd_fr(
            scu_bcs_3_308.bcs_frwheelspd());
      }
    }

    if (scu_bcs_3_308.has_bcs_flwheelspd()) {
      if (chassis_.has_wheel_speed()) {
        chassis_.mutable_wheel_speed()->set_is_wheel_spd_fl_valid(
            scu_bcs_3_308.bcs_flwheelspdvd());
        chassis_.mutable_wheel_speed()->set_wheel_direction_fl(
            (WheelSpeed::WheelSpeedType)scu_bcs_3_308.bcs_flwheeldirection());
        chassis_.mutable_wheel_speed()->set_wheel_spd_fl(
            scu_bcs_3_308.bcs_flwheelspd());
      }
    }
  }

  if (ge3.has_scu_bcs_2_307() && ge3.scu_bcs_2_307().has_bcs_vehspd()) {
    chassis_.set_speed_mps(
        static_cast<float>(ge3.scu_bcs_2_307().bcs_vehspd()));
  } else {
    chassis_.set_speed_mps(0);
  }

  // 7
  // ge3 only has fuel percentage
  // to avoid confusing, just don't set
  chassis_.set_fuel_range_m(0);

  if (ge3.has_scu_vcu_1_312() && ge3.scu_vcu_1_312().has_vcu_accpedact()) {
    chassis_.set_throttle_percentage(
        static_cast<float>(ge3.scu_vcu_1_312().vcu_accpedact()));
  } else {
    chassis_.set_throttle_percentage(0);
  }
  // 9
  if (ge3.has_scu_bcs_1_306() && ge3.scu_bcs_1_306().has_bcs_brkpedact()) {
    chassis_.set_brake_percentage(
        static_cast<float>(ge3.scu_bcs_1_306().bcs_brkpedact()));
  } else {
    chassis_.set_brake_percentage(0);
  }
  // 23, previously 10
  if (ge3.has_scu_vcu_1_312() && ge3.scu_vcu_1_312().has_vcu_gearact()) {
    switch (ge3.scu_vcu_1_312().vcu_gearact()) {
      case Scu_vcu_1_312::VCU_GEARACT_INVALID: {
        chassis_.set_gear_location(Chassis::GEAR_INVALID);
      } break;
      case Scu_vcu_1_312::VCU_GEARACT_DRIVE: {
        chassis_.set_gear_location(Chassis::GEAR_DRIVE);
      } break;
      case Scu_vcu_1_312::VCU_GEARACT_NEUTRAL: {
        chassis_.set_gear_location(Chassis::GEAR_NEUTRAL);
      } break;
      case Scu_vcu_1_312::VCU_GEARACT_REVERSE: {
        chassis_.set_gear_location(Chassis::GEAR_REVERSE);
      } break;
      case Scu_vcu_1_312::VCU_GEARACT_PARK: {
        chassis_.set_gear_location(Chassis::GEAR_PARKING);
      } break;
      default:
        chassis_.set_gear_location(Chassis::GEAR_INVALID);
        break;
    }
  } else {
    chassis_.set_gear_location(Chassis::GEAR_INVALID);
  }

  // 11
  if (ge3.has_scu_eps_311() && ge3.scu_eps_311().has_eps_steerangle()) {
    chassis_.set_steering_percentage(
        static_cast<float>(ge3.scu_eps_311().eps_steerangle() /
                           vehicle_params_.max_steer_angle() * M_PI / 1.80));
  } else {
    chassis_.set_steering_percentage(0);
  }

  // 13
  if (ge3.has_scu_epb_310() && ge3.scu_epb_310().has_epb_sysst()) {
    chassis_.set_parking_brake(ge3.scu_epb_310().epb_sysst() ==
                               Scu_epb_310::EPB_SYSST_APPLIED);
  } else {
    chassis_.set_parking_brake(false);
  }

  // 14, 15: ge3 light control
  if (ge3.has_scu_bcm_304() && ge3.scu_bcm_304().has_bcm_highbeamst() &&
      Scu_bcm_304::BCM_HIGHBEAMST_ACTIVE ==
          ge3.scu_bcm_304().bcm_highbeamst()) {
    if (chassis_.has_signal()) {
      chassis_.mutable_signal()->set_high_beam(true);
    }
  } else {
    if (chassis_.has_signal()) {
      chassis_.mutable_signal()->set_high_beam(false);
    }
  }

  // 16, 17
  if (ge3.has_scu_bcm_304()) {
    Scu_bcm_304 scu_bcm_304 = ge3.scu_bcm_304();
    if (scu_bcm_304.has_bcm_leftturnlampst() &&
        Scu_bcm_304::BCM_LEFTTURNLAMPST_ACTIVE ==
            scu_bcm_304.bcm_leftturnlampst()) {
      chassis_.mutable_signal()->set_turn_signal(
          common::VehicleSignal::TURN_LEFT);
    } else if (scu_bcm_304.has_bcm_rightturnlampst() &&
               Scu_bcm_304::BCM_RIGHTTURNLAMPST_ACTIVE ==
                   scu_bcm_304.bcm_rightturnlampst()) {
      chassis_.mutable_signal()->set_turn_signal(
          common::VehicleSignal::TURN_RIGHT);
    } else {
      chassis_.mutable_signal()->set_turn_signal(
          common::VehicleSignal::TURN_NONE);
    }
  } else {
    chassis_.mutable_signal()->set_turn_signal(
        common::VehicleSignal::TURN_NONE);
  }
  // 18
  if (ge3.has_scu_bcm_304() && ge3.scu_bcm_304().has_bcm_hornst() &&
      Scu_bcm_304::BCM_HORNST_ACTIVE == ge3.scu_bcm_304().bcm_hornst()) {
    chassis_.mutable_signal()->set_horn(true);
  } else {
    chassis_.mutable_signal()->set_horn(false);
  }

  // vin number will be written into KVDB once.
  chassis_.mutable_vehicle_id()->set_vin("");
  if (ge3.has_scu_1_301() && ge3.has_scu_2_302() && ge3.has_scu_3_303()) {
    Scu_1_301 scu_1_301 = ge3.scu_1_301();
    Scu_2_302 scu_2_302 = ge3.scu_2_302();
    Scu_3_303 scu_3_303 = ge3.scu_3_303();
    if (scu_2_302.has_vin00() && scu_2_302.has_vin01() &&
        scu_2_302.has_vin02() && scu_2_302.has_vin03() &&
        scu_2_302.has_vin04() && scu_2_302.has_vin05() &&
        scu_2_302.has_vin06() && scu_2_302.has_vin07() &&
        scu_3_303.has_vin08() && scu_3_303.has_vin09() &&
        scu_3_303.has_vin10() && scu_3_303.has_vin11() &&
        scu_3_303.has_vin12() && scu_3_303.has_vin13() &&
        scu_3_303.has_vin14() && scu_3_303.has_vin15() &&
        scu_1_301.has_vin16()) {
      int n[17];
      n[0] = scu_2_302.vin00();
      n[1] = scu_2_302.vin01();
      n[2] = scu_2_302.vin02();
      n[3] = scu_2_302.vin03();
      n[4] = scu_2_302.vin04();
      n[5] = scu_2_302.vin05();
      n[6] = scu_2_302.vin06();
      n[7] = scu_2_302.vin07();
      n[8] = scu_3_303.vin08();
      n[9] = scu_3_303.vin09();
      n[10] = scu_3_303.vin10();
      n[11] = scu_3_303.vin11();
      n[12] = scu_3_303.vin12();
      n[13] = scu_3_303.vin13();
      n[14] = scu_3_303.vin14();
      n[15] = scu_3_303.vin15();
      n[16] = scu_1_301.vin16();

      char ch[17];
      memset(&ch, '\0', sizeof(ch));
      for (int i = 0; i < 17; i++) {
        ch[i] = static_cast<char>(n[i]);
      }
      if (chassis_.has_vehicle_id()) {
        chassis_.mutable_vehicle_id()->set_vin(ch);
      }
    }
  }

  // give engage_advice based on error_code and canbus feedback
  if (chassis_error_mask_) {
    if (chassis_.has_engage_advice()) {
      chassis_.mutable_engage_advice()->set_advice(
          apollo::common::EngageAdvice::DISALLOW_ENGAGE);
      chassis_.mutable_engage_advice()->set_reason("Chassis error!");
    }
  } else if (chassis_.parking_brake() || CheckSafetyError(chassis_detail)) {
    if (chassis_.has_engage_advice()) {
      chassis_.mutable_engage_advice()->set_advice(
          apollo::common::EngageAdvice::DISALLOW_ENGAGE);
      chassis_.mutable_engage_advice()->set_reason(
          "Vehicle is not in a safe state to engage!");
    }
  } else {
    if (chassis_.has_engage_advice()) {
      chassis_.mutable_engage_advice()->set_advice(
          apollo::common::EngageAdvice::READY_TO_ENGAGE);
    }
  }
```

- Set the autonomous driving mode and edit the related enable logic. In Apollo, the driving modes of the vehicle mainly include:

- Complete autonomous driving mode (`COMPLETE_AUTO_DRIVE`): both lateral and longitudinal enabled;

- Lateral autonomous driving mode (`AUTO_STEER_ONLY`): lateral enabled, longitudinal not enabled;

- Longitudinal autonomous driving mode (`AUTO_SPEED_ONLY`): lateral not enabled, longitudinal enabled;

- The control logic of the vehicle enable control signals is as follows:

```C++
ErrorCode Ge3Controller::EnableAutoMode() {
  if (driving_mode() == Chassis::COMPLETE_AUTO_DRIVE) {
    AINFO << "already in COMPLETE_AUTO_DRIVE mode";
    return ErrorCode::OK;
  }
  pc_bcs_202_->set_pc_brkpedenable(Pc_bcs_202::PC_BRKPEDENABLE_ENABLE);
  pc_vcu_205_->set_pc_accpedenable(Pc_vcu_205::PC_ACCPEDENABLE_ENABLE);
  pc_vcu_205_->set_pc_gearenable(Pc_vcu_205::PC_GEARENABLE_ENABLE);
  pc_epb_203_->set_pc_epbenable(Pc_epb_203::PC_EPBENABLE_ENABLE);
  pc_eps_204_->set_pc_steerenable(Pc_eps_204::PC_STEERENABLE_ENABLE);

  can_sender_->Update();
  const int32_t flag =
      CHECK_RESPONSE_STEER_UNIT_FLAG | CHECK_RESPONSE_SPEED_UNIT_FLAG;
  if (!CheckResponse(flag, true)) {
    AERROR << "Failed to switch to COMPLETE_AUTO_DRIVE mode.";
    Emergency();
    set_chassis_error_code(Chassis::CHASSIS_ERROR);
    return ErrorCode::CANBUS_ERROR;
  }
  set_driving_mode(Chassis::COMPLETE_AUTO_DRIVE);
  // If the auto mode can be set normally, the harzad lamp should be off.
  pc_bcm_201_->set_pc_hazardlampreq(Pc_bcm_201::PC_HAZARDLAMPREQ_NOREQ);
  AINFO << "Switch to COMPLETE_AUTO_DRIVE mode ok.";
  return ErrorCode::OK;
}

ErrorCode Ge3Controller::DisableAutoMode() {
  ResetProtocol();
  can_sender_->Update();
  set_driving_mode(Chassis::COMPLETE_MANUAL);
  set_chassis_error_code(Chassis::NO_ERROR);
  AINFO << "Switch to COMPLETE_MANUAL OK.";
  return ErrorCode::OK;
}

ErrorCode Ge3Controller::EnableSteeringOnlyMode() {
  if (driving_mode() == Chassis::COMPLETE_AUTO_DRIVE ||
      driving_mode() == Chassis::AUTO_STEER_ONLY) {
    set_driving_mode(Chassis::AUTO_STEER_ONLY);
    AINFO << "Already in AUTO_STEER_ONLY mode";
    return ErrorCode::OK;
  }
  pc_bcs_202_->set_pc_brkpedenable(Pc_bcs_202::PC_BRKPEDENABLE_DISABLE);
  pc_vcu_205_->set_pc_accpedenable(Pc_vcu_205::PC_ACCPEDENABLE_DISABLE);
  pc_vcu_205_->set_pc_gearenable(Pc_vcu_205::PC_GEARENABLE_DISABLE);
  pc_epb_203_->set_pc_epbenable(Pc_epb_203::PC_EPBENABLE_DISABLE);
  pc_eps_204_->set_pc_steerenable(Pc_eps_204::PC_STEERENABLE_ENABLE);

  can_sender_->Update();
  if (!CheckResponse(CHECK_RESPONSE_STEER_UNIT_FLAG, true)) {
    AERROR << "Failed to switch to AUTO_STEER_ONLY mode.";
    Emergency();
    set_chassis_error_code(Chassis::CHASSIS_ERROR);
    return ErrorCode::CANBUS_ERROR;
  }
  set_driving_mode(Chassis::AUTO_STEER_ONLY);
  // If the auto mode can be set normally, the harzad lamp should be off.
  pc_bcm_201_->set_pc_hazardlampreq(Pc_bcm_201::PC_HAZARDLAMPREQ_NOREQ);
  AINFO << "Switch to AUTO_STEER_ONLY mode ok.";
  return ErrorCode::OK;
}

ErrorCode Ge3Controller::EnableSpeedOnlyMode() {
  if (driving_mode() == Chassis::COMPLETE_AUTO_DRIVE ||
      driving_mode() == Chassis::AUTO_SPEED_ONLY) {
    set_driving_mode(Chassis::AUTO_SPEED_ONLY);
    AINFO << "Already in AUTO_SPEED_ONLY mode";
    return ErrorCode::OK;
  }
  pc_bcs_202_->set_pc_brkpedenable(Pc_bcs_202::PC_BRKPEDENABLE_ENABLE);
  pc_vcu_205_->set_pc_accpedenable(Pc_vcu_205::PC_ACCPEDENABLE_ENABLE);
  pc_vcu_205_->set_pc_gearenable(Pc_vcu_205::PC_GEARENABLE_ENABLE);
  pc_epb_203_->set_pc_epbenable(Pc_epb_203::PC_EPBENABLE_ENABLE);
  pc_eps_204_->set_pc_steerenable(Pc_eps_204::PC_STEERENABLE_DISABLE);

  can_sender_->Update();
  if (!CheckResponse(CHECK_RESPONSE_SPEED_UNIT_FLAG, true)) {
    AERROR << "Failed to switch to AUTO_STEER_ONLY mode.";
    Emergency();
    set_chassis_error_code(Chassis::CHASSIS_ERROR);
    return ErrorCode::CANBUS_ERROR;
  }
  set_driving_mode(Chassis::AUTO_SPEED_ONLY);
  // If the auto mode can be set normally, the harzad lamp should be off.
  pc_bcm_201_->set_pc_hazardlampreq(Pc_bcm_201::PC_HAZARDLAMPREQ_NOREQ);
  AINFO << "Switch to AUTO_SPEED_ONLY mode ok.";
  return ErrorCode::OK;
}
```
- Add the related functions of the control signals. The control signals that must be added include the vehicle's throttle and brake pedal control, steering control and gear control; other control signals include headlight control, horn control, turn signal control and electronic parking brake control. The code is as follows:

```C++
// NEUTRAL, REVERSE, DRIVE
void Ge3Controller::Gear(Chassis::GearPosition gear_position) {
  if (driving_mode() != Chassis::COMPLETE_AUTO_DRIVE &&
      driving_mode() != Chassis::AUTO_SPEED_ONLY) {
    AINFO << "This drive mode no need to set gear.";
    return;
  }
  switch (gear_position) {
    case Chassis::GEAR_NEUTRAL: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_NEUTRAL);
      break;
    }
    case Chassis::GEAR_REVERSE: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_REVERSE);
      break;
    }
    case Chassis::GEAR_DRIVE: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_DRIVE);
      break;
    }
    case Chassis::GEAR_PARKING: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_PARK);
      break;
    }
    case Chassis::GEAR_LOW: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_INVALID);
      break;
    }
    case Chassis::GEAR_NONE: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_INVALID);
      break;
    }
    case Chassis::GEAR_INVALID: {
      AERROR << "Gear command is invalid!";
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_INVALID);
      break;
    }
    default: {
      pc_vcu_205_->set_pc_gearreq(Pc_vcu_205::PC_GEARREQ_INVALID);
      break;
    }
  }
}

// brake with new acceleration
// acceleration:0.00~99.99, unit:
// acceleration:0.0 ~ 7.0, unit:m/s^2
// acceleration_spd:60 ~ 100, suggest: 90
// -> pedal
void Ge3Controller::Brake(double pedal) {
  // Update brake value based on mode
  if (driving_mode() != Chassis::COMPLETE_AUTO_DRIVE &&
      driving_mode() != Chassis::AUTO_SPEED_ONLY) {
    AINFO << "The current drive mode does not need to set acceleration.";
    return;
  }
  pc_bcs_202_->set_pc_brkpedreq(pedal);
}

// drive with old acceleration
// gas:0.00~99.99 unit:
void Ge3Controller::Throttle(double pedal) {
  if (driving_mode() != Chassis::COMPLETE_AUTO_DRIVE &&
      driving_mode() != Chassis::AUTO_SPEED_ONLY) {
    AINFO << "The current drive mode does not need to set acceleration.";
    return;
  }
  pc_vcu_205_->set_pc_accpedreq(pedal);
}

// ge3 default, -470 ~ 470, left:+, right:-
// need to be compatible with control module, so reverse
// steering with old angle speed
// angle:-99.99~0.00~99.99, unit:, left:-, right:+
void Ge3Controller::Steer(double angle) {
  if (!(driving_mode() == Chassis::COMPLETE_AUTO_DRIVE ||
        driving_mode() == Chassis::AUTO_STEER_ONLY)) {
    AINFO << "The current driving mode does not need to set steer.";
    return;
  }
  const double real_angle =
      vehicle_params_.max_steer_angle() / M_PI * 180 * angle / 100.0;
  pc_eps_204_->set_pc_steerangreq(real_angle)->set_pc_steerspdreq(500);
}

// drive with acceleration/deceleration
// acc:-7.0 ~ 5.0, unit:m/s^2
void Ge3Controller::Acceleration(double acc) {
  if (driving_mode() != Chassis::COMPLETE_AUTO_DRIVE &&
      driving_mode() != Chassis::AUTO_SPEED_ONLY) {
    AINFO << "The current drive mode does not need to set acceleration.";
    return;
  }
  // None
}

// steering with new angle speed
// angle:-99.99~0.00~99.99, unit:, left:-, right:+
// angle_spd:0.00~99.99, unit:deg/s
void Ge3Controller::Steer(double angle, double angle_spd) {
  if (driving_mode() != Chassis::COMPLETE_AUTO_DRIVE &&
      driving_mode() != Chassis::AUTO_STEER_ONLY) {
    AINFO << "The current driving mode does not need to set steer.";
    return;
  }
  const double real_angle =
      vehicle_params_.max_steer_angle() / M_PI * 180 * angle / 100.0;
  const double real_angle_spd =
      ProtocolData<::apollo::canbus::ChassisDetail>::BoundedValue(
          vehicle_params_.min_steer_angle_rate() / M_PI * 180,
          vehicle_params_.max_steer_angle_rate() / M_PI * 180,
          vehicle_params_.max_steer_angle_rate() / M_PI * 180 * angle_spd /
              100.0);
  pc_eps_204_->set_pc_steerangreq(real_angle)
      ->set_pc_steerspdreq(static_cast<int>(real_angle_spd));
}

void Ge3Controller::SetEpbBreak(const ControlCommand& command) {
  if (command.parking_brake()) {
    pc_epb_203_->set_pc_epbreq(Pc_epb_203::PC_EPBREQ_APPLY);
  } else {
    pc_epb_203_->set_pc_epbreq(Pc_epb_203::PC_EPBREQ_RELEASE);
  }
}

void Ge3Controller::SetBeam(const ControlCommand& command) {
  if (command.signal().high_beam()) {
    pc_bcm_201_->set_pc_lowbeamreq(Pc_bcm_201::PC_LOWBEAMREQ_NOREQ);
    pc_bcm_201_->set_pc_highbeamreq(Pc_bcm_201::PC_HIGHBEAMREQ_REQ);
  } else if (command.signal().low_beam()) {
    pc_bcm_201_->set_pc_lowbeamreq(Pc_bcm_201::PC_LOWBEAMREQ_REQ);
    pc_bcm_201_->set_pc_highbeamreq(Pc_bcm_201::PC_HIGHBEAMREQ_NOREQ);
  } else {
    pc_bcm_201_->set_pc_lowbeamreq(Pc_bcm_201::PC_LOWBEAMREQ_NOREQ);
    pc_bcm_201_->set_pc_highbeamreq(Pc_bcm_201::PC_HIGHBEAMREQ_NOREQ);
  }
}

void Ge3Controller::SetHorn(const ControlCommand& command) {
  if (command.signal().horn()) {
    pc_bcm_201_->set_pc_hornreq(Pc_bcm_201::PC_HORNREQ_REQ);
  } else {
    pc_bcm_201_->set_pc_hornreq(Pc_bcm_201::PC_HORNREQ_NOREQ);
  }
}

void Ge3Controller::SetTurningSignal(const ControlCommand& command) {
  // Set Turn Signal
  auto signal = command.signal().turn_signal();
  if (signal == common::VehicleSignal::TURN_LEFT) {
    pc_bcm_201_->set_pc_leftturnlampreq(Pc_bcm_201::PC_LEFTTURNLAMPREQ_REQ);
    pc_bcm_201_->set_pc_rightturnlampreq(Pc_bcm_201::PC_RIGHTTURNLAMPREQ_NOREQ);
  } else if (signal == common::VehicleSignal::TURN_RIGHT) {
    pc_bcm_201_->set_pc_leftturnlampreq(Pc_bcm_201::PC_LEFTTURNLAMPREQ_NOREQ);
    pc_bcm_201_->set_pc_rightturnlampreq(Pc_bcm_201::PC_RIGHTTURNLAMPREQ_REQ);
  } else {
    pc_bcm_201_->set_pc_leftturnlampreq(Pc_bcm_201::PC_LEFTTURNLAMPREQ_NOREQ);
    pc_bcm_201_->set_pc_rightturnlampreq(Pc_bcm_201::PC_RIGHTTURNLAMPREQ_NOREQ);
  }
}
```

- Add the `CheckResponse()` logic. Apollo has added monitoring of whether the vehicle's low level is in autonomous driving mode, that is, whether the driving mode feedback of the vehicle's lateral, drive and brake modules is in the autonomous driving state. If within one `CheckResponse` period the driving mode feedback of any module of the vehicle reports takeover or manual driving mode, Apollo will put the vehicle enable into emergency stop mode (`Emergency`), that is, all modules are controlled into manual mode, ensuring safety when controlling the vehicle. The `CheckResponse` period may differ between vehicles, and developers need to set the `check` period by setting `retry_num` according to the situation.
Developers do not have to change the original check code scheme; they can map the 3 driving mode feedback messages to `chassis_detail` in Apollo:

`is_eps_online->steering mode feedback signal`

`is_vcu_online->drive mode feedback signal`

`is_esp_online->brake mode feedback signal`

- In the Chassis() method, add the following code:

```C++
  // 19 add checkresponse signal
  if (chassis_detail.has_scu_bcs_1_306() &&
      chassis_detail.scu_bcs_1_306().has_bcs_drvmode()) {
    chassis_.mutable_check_response()->set_is_esp_online(
        chassis_detail.scu_bcs_1_306().bcs_drvmode() == 1);
  }
  if (chassis_detail.has_scu_eps_311() &&
      chassis_detail.scu_eps_311().has_eps_drvmode()) {
    chassis_.mutable_check_response()->set_is_eps_online(
        chassis_detail.scu_eps_311().eps_drvmode() == 1);
  }
  if (chassis_detail.has_scu_vcu_1_312() &&
      chassis_detail.scu_vcu_1_312().has_vcu_drvmode()) {
    chassis_.mutable_check_response()->set_is_vcu_online(
        chassis_detail.scu_vcu_1_312().vcu_drvmode() == 1);
  }
```

- In the CheckResponse() method, implement the handshake disconnection check logic with the chassis:

```C++
bool Ge3Controller::CheckResponse(const int32_t flags, bool need_wait) {
  int32_t retry_num = 20;
  Ge3 chassis_detail;
  bool is_eps_online = false;
  bool is_vcu_online = false;
  bool is_esp_online = false;
  Chassis chassis = Chassis();

  do {
    if (message_manager_->GetSensorData(&chassis_detail) != ErrorCode::OK) {
      AERROR_EVERY(100) << "get chassis detail failed.";
      return false;
    }
    bool check_ok = true;
    if (flags & CHECK_RESPONSE_STEER_UNIT_FLAG) {
      is_eps_online = chassis.has_check_response() &&
                      chassis.check_response().has_is_eps_online() &&
                      chassis.check_response().is_eps_online();
      check_ok = check_ok && is_eps_online;
    }

    if (flags & CHECK_RESPONSE_SPEED_UNIT_FLAG) {
      is_vcu_online = chassis.has_check_response() &&
                      chassis.check_response().has_is_vcu_online() &&
                      chassis.check_response().is_vcu_online();
      is_esp_online = chassis.has_check_response() &&
                      chassis.check_response().has_is_esp_online() &&
                      chassis.check_response().is_esp_online();
      check_ok = check_ok && is_vcu_online && is_esp_online;
    }
    if (check_ok) {
      return true;
    }
    AINFO << "Need to check response again.";
    if (need_wait) {
      --retry_num;
      std::this_thread::sleep_for(
          std::chrono::duration<double, std::milli>(20));
    }
  } while (need_wait && retry_num);

  AINFO << "check_response fail: is_eps_online:" << is_eps_online
        << ", is_vcu_online:" << is_vcu_online
        << ", is_esp_online:" << is_esp_online;
  return false;
}
```

### 3. Modify the chassis speed feedback protocol to convert the speed feedback unit from km/h to m/s

- The Apollo system uses `m/s` as the default speed feedback unit. The chassis speed information is very important to Apollo, and the data is needed in vehicle calibration, control, planning and so on, so developers should check the unit of the speed feedback with particular care when developing the adaptation code. To convert speed from `km/h` to `m/s`, simply divide the speed feedback signal by `3.6`.
The message in which the Ge3 vehicle feeds back speed is in the file `/apollo_workspace/modules/canbus_vehicle/ge3/protocol/scu_bcs_2_307.cc`, and the speed feedback message is `Scubcs2307::bcs_vehspd{}`, as shown in the figure below:
![speed_transfer](images/vehicle_adaption_tutorial/speed_transfer.png)

### 4. Updating the CANBUS configuration files

- As mentioned above, the canbus configuration files are generated in the `/apollo_workspace/output/ge3/canbus_conf/` folder, namely `canbus.conf` and `canbus_conf.pb.txt`.

```shell
canbus_conf/
|-- canbus.conf
|-- canbus_conf.pb.txt
```

- View the `canbus.conf` configuration file

```shell
--flagfile=/apollo/modules/common/data/global_flagfile.txt
--canbus_conf_file=/apollo/modules/canbus/conf/canbus_conf.pb.txt
--load_vehicle_library=/opt/apollo/neo/lib/modules/canbus_vehicle/ge3/libge3_vehicle_factory_lib.so
--load_vehicle_class_name=Ge3VehicleFactory
--enable_chassis_detail_pub
```


- View the `canbus_conf.pb.txt` configuration file

```shell
vehicle_parameter {
  max_enable_fail_attempt: 5
  driving_mode: COMPLETE_AUTO_DRIVE
}

can_card_parameter {
  brand: SOCKET_CAN_RAW
  type: PCI_CARD
  channel_id: CHANNEL_ID_ZERO
  interface: NATIVE
}

enable_debug_mode: false
enable_receiver_log: false
```

- Replace the contents of the `canbus.conf` and `canbus_conf.pb.txt` configuration files in the `/apollo/modules/canbus/conf/` folder with the contents of the above canbus files.

## 3. Open-loop verification of the DBC for the vehicle chassis

### 1. Open-loop test

- After the vehicle chassis DBC is determined, perform open-loop tests on the signals defined in the DBC. The main purposes of the open-loop test are to test whether the vehicle's drive-by-wire signals match the vehicle's actual functions, to test whether the vehicle's steering and acceleration/deceleration performance response meets the vehicle's drive-by-wire needs, and to test whether the vehicle's takeover logic meets the requirements.

- In the chassis open-loop test, developers should test the key DBC requirements described above, such as the independence of the vehicle's lateral and longitudinal enables, the independence of lateral and longitudinal takeover, whether each control signal meets the control requirements and control boundaries, and whether the feedback signals are correct. In terms of performance, check whether the steering and acceleration/deceleration delay of the control signals meets Apollo's control performance requirements, and whether the overshoot error is within the range of the drive-by-wire list. Please test the control signals according to the performance requirements in the drive-by-wire list.

### 2. Teleop chassis joint debugging test

The chassis joint debugging test connects Apollo to the vehicle through canbus communication, and tests whether the control signals sent by Apollo (such as accelerate/decelerate/steer/enable) can accurately control the vehicle, whether the vehicle's chassis feedback signals (such as current pedal percentage feedback, current steering angle feedback, enable feedback and takeover feedback) reflect the actual state of the vehicle, and verifies that the vehicle chassis can accurately execute the control commands sent by Apollo.

#### Introduction to the teleop test tool

Apollo provides developers with a teleop test tool. After entering the apollo container environment, execute
```
teleop
```
to enter the teleop interface, as shown in the figure below:

![teleop](images/vehicle_adaption_tutorial/teleop.png)

Press the `h` key to bring up the help interface shown in the figure above, where you can look up how to use the Teleop tool. The following briefly introduces the teleop control commands.

##### Set Action: executes Apollo's enable control of the vehicle:

Pressing the `m` and `0` keys together executes the reset command, and the vehicle exits autonomous driving mode;
Pressing the `m` and `1` keys together executes the start command, and the vehicle enters autonomous driving mode.

##### Set Gear: sets the gear; press `g` and a `number` together to set the corresponding gear:

Press `g`+`0` to shift into N (neutral);
Press `g`+`1` to shift into D (drive);
Press `g`+`2` to shift into R (reverse);
Press `g`+`3` to shift into P (park);
Other gear control commands are not needed for now; according to our DBC requirements, these are generally the only gear commands for vehicle control.

##### Throttle/Speed up: increases the throttle pedal amount by 2% each time, and the vehicle accelerates

Press the `w` key to increase the throttle pedal by 2% and make the vehicle accelerate. If a brake command is currently being executed, pressing `w` reduces the brake pedal amount by 2%.
The control range of the throttle pedal amount is 0-100%, where 100% is equivalent to the throttle pedal fully depressed. By default, each press of the `w` key increases the throttle pedal amount by 2%. Developers can change this value based on real-vehicle tests; experience shows that a 2% change each time is appropriate.

`Note: please first use teleop to shift into D and then execute the throttle command. Test in an open area and pay attention to safety`!

##### Set Throttle: sets the throttle pedal percentage

Press `t`+`number` to directly set a specific throttle pedal percentage; the throttle pedal can be set to 0-100. For example, executing t20 directly sets the current throttle pedal amount to 20% and sets the brake pedal percentage to 0, which is consistent with real driving logic: if the throttle pedal is pressed, the brake pedal cannot be pressed.

`Note: when directly setting the throttle pedal percentage, do not set too large a value each time, test in an open area and pay attention to safety! Please execute this command with the vehicle in D gear`.

##### Brake/Speed down: increases the brake pedal amount by 2% each time, and the vehicle decelerates

Press the `s` key to increase the brake pedal percentage and make the vehicle decelerate. If a throttle acceleration command is currently being executed, pressing the `s` key reduces the throttle pedal percentage.

`Note: please first use teleop to shift into D and then execute the throttle command. Test in an open area and pay attention to safety`!

##### Set Brake: sets the brake pedal percentage

Press `b`+`number` to directly set a specific brake pedal percentage; the brake pedal can be set to 0-100. For example, executing b20 directly sets the current brake pedal amount to 20% and sets the throttle pedal percentage to 0, which is consistent with real driving logic: if the brake pedal is pressed, the throttle pedal cannot be pressed.

`Note: when directly setting the brake pedal percentage, do not set too large a value each time, test in an open area and pay attention to safety! Please execute this command with the vehicle in D gear`.

##### Steer LEFT: the steering wheel turns left by 2% each time

Pressing the `a` key turns the steering wheel left by 2% of the maximum steering wheel angle each time; the specific rotation angle should be calculated by multiplying the maximum steering wheel angle set for the vehicle by 2%.
This command can be executed while the vehicle is stationary or after it has started.

##### Steer RIGHT: the steering wheel turns right by 2% each time

Pressing the `s` key turns the steering wheel right by 2% of the maximum steering wheel angle each time; the specific rotation angle should be calculated by multiplying the maximum steering wheel angle set for the vehicle by 2%.
This command can be executed while the vehicle is stationary or after it has started.

##### Parking Brake: turns on the electronic parking brake

Press the `P` key (note: uppercase P) to manually control the vehicle's electronic parking brake switch. This function works only if the vehicle provides a control interface for the electronic parking brake.

`Note: when engaging or releasing the electronic parking brake, please use teleop to set the vehicle to P gear`.

##### Emergency Stop

Press the `E` key (note: uppercase E) to bring the vehicle to an emergency stop, executing 50% braking by default.
Developers are advised to use this function as little as possible during testing because it feels harsh; pay attention to the surroundings when debugging the vehicle. In an emergency, take over the vehicle manually in time by stepping on the brake pedal with an external pedal.

### Diagnostic tool introduction

Once you understand the basic operation of teleop, developers execute specific control commands on the vehicle with the corresponding instructions, and then use Apollo's visual monitoring tool `cyber_monitor` to view the vehicle's current feedback signals, confirming whether the vehicle's execution results are correct after the control commands are sent.
Apollo provides a visual monitoring tool that can monitor the chassis `chassis` and `chassis_detail` information, by executing

```shell
cyber_monitor
```

In the `/apollo/modules/canbus/conf/canbus.conf` file:
Change the configuration `--noenable_chassis_detail_pub` to `--enable_chassis_detail_pub`, which turns on the detailed chassis information `chassis_detail`, so that the raw information of every frame of the chassis feedback messages can be viewed.
Change the configuration `--receive_guardian` to `--noreceive_guardian`, which turns off guardian mode and enters the canbus debugging mode, so that the vehicle can be controlled during teleop. The modified `canbus_conf` configuration file is shown below.

```shell
--flagfile=/apollo/modules/common/data/global_flagfile.txt
--canbus_conf_file=/apollo/modules/canbus/conf/canbus_conf.pb.txt
--load_vehicle_library=/opt/apollo/neo/lib/modules/canbus_vehicle/ge3/libge3_vehicle_factory_lib.so
--load_vehicle_class_name=Ge3VehicleFactory
--enable_chassis_detail_pub
```

The `cyber_monitor` interface when opened is as follows

![cyber_monitor](images/vehicle_adaption_tutorial/cyber_monitor.png)

You can see that both `chassis` and `chassis_detail` have data, at a frequency of about 100 Hz. The data under `chassis` is mainly the vehicle's actual speed, actual pedal percentages, actual steering wheel angle percentage, actual gear feedback and so on; `chassis_detail` contains all the information of the feedback messages the chassis uploads to apollo (that is, all the messages apollo receives from the chassis).
The `chassis` information is shown in the figure below

![chassis](images/vehicle_adaption_tutorial/chassis.png)

The `chassis_detail` information is shown in the figure below

![chassis_detail1](images/vehicle_adaption_tutorial/chassis_detail1.png)
![chassis_detail](images/vehicle_adaption_tutorial/chassis_detail.png)
