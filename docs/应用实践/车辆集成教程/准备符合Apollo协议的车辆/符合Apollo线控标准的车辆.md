To develop a vehicle equipped with the Apollo system, you first need to establish communication between Apollo and the vehicle's lower layers.

The whole vehicle communicates among its **controllers (ECUs)** (Electronic Control Units) through the **CAN bus** (Controller Area Network). Therefore, when the Apollo control system connects to the vehicle system, it also controls the vehicle through CAN bus communication. The vehicle needs to expose a set of chassis signals and configure them into a **DBC file** that meets the requirements of the Apollo drive-by-wire standard (a DBC file, Database file, is a dedicated file for describing data on the CAN bus; it lets users quickly build the network structure of the vehicle's CAN communication and define the signals, messages and other information on the CAN bus).

It is recommended to use files edited and generated with CANdb++ of the **canoe software**.

The Apollo software processes the chassis DBC file to generate a chassis adaptation code template. With simple programming, the chassis signals exposed by the vehicle can be adapted into the Apollo software, enabling communication between Apollo and the vehicle chassis.

## Prerequisites

Understand the basics of CAN bus communication.

## DBC Drive-by-Wire Standard in Detail

When developing the vehicle chassis control signals, refer to the vehicle drive-by-wire standard published by the [vehicle certification platform on the Apollo official website](http://developer.apollo.auto/docs/procedure_cn.html): see the [passenger vehicle standard](http://apollo-homepage.bj.bcebos.com/Apollo_by_wire_requirement.xlsx).

The vehicle drive-by-wire standard specifies the requirements for vehicle chassis signals, and also the response time requirement of each frame of control signal on the chassis. Vehicle chassis signals must not only **meet the requirements functionally**, but also **meet the control requirements in performance**, so that the chassis can be controlled as a vehicle that conforms to the Apollo drive-by-wire standard.

When converting a vehicle chassis into a drive-by-wire chassis, **refer to the vehicle drive-by-wire standard to meet the control performance requirements**, for example **the steering signal must respond within 100 ms** and **the maximum overshoot must meet specific requirements**.

Please read the performance requirements in the drive-by-wire list carefully; during Apollo vehicle certification, the chassis performance will also be tested accordingly.

In the actual process of developing the DBC, there are also specific requirements for the details of each signal definition. Following them not only saves you time but also improves the efficiency of vehicle certification. Please focus on understanding and paying attention to each signal. Apollo's requirements for vehicle chassis signals are shown in the table below:

- Drive-by-wire steering
  
  <img src="https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_e494043.png" width = "100%" />

- Drive-by-wire drive and braking
  
  <img src="https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_9a3527a.png" width = "100%" />

- Body control functions
  
  <img src="https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_402fd86.png" width = "100%" />

Vehicle chassis control is the basis of communication between Apollo and the chassis, and every control signal must be reliable and accurate. Manual takeover and out-of-range handling ensure that the vehicle can be taken over at any time during testing and use, guaranteeing driving safety when emergencies occur. In addition to the detailed requirements for signals in the drive-by-wire list, **you also need to pay attention to the independence of the lateral and longitudinal enable signals** and **special requirements such as the chassis takeover logic**. The special functional requirements for the signals are described in detail below.

## Drive-by-Wire Steering

### Steering Enable Signal

- The steering enable signal controls the vehicle entering or exiting the steering autonomous driving mode. The enable signal must be a **rising-edge triggered** signal, and in principle there should be 1 enable signal. If a developer's steering needs several different signals together to be enabled into the autonomous driving state, the logic of those enable signals must be integrated at the lower layer, and in principle only 1 enable signal may be given to Apollo.

- The steering autonomous driving mode and its enable signal must be independent and not associated with the enable signals of the longitudinal modules (such as drive, braking, gear and parking). It must satisfy:
  
  - When the steering autonomous driving mode is enabled alone, the longitudinal driving mode does not change, and when a steering wheel angle or speed control signal is sent, the vehicle can respond with the corresponding angle and speed;
  
  - When the steering autonomous driving mode is disabled alone (exited), the longitudinal driving mode does not change, and when a steering wheel angle or speed control signal is sent, the vehicle does not respond.

- If the lateral control is taken over, entering the autonomous driving mode again requires a rising-edge trigger signal.

### Steering Control Signals

- Target steering wheel angle: the signal that controls the steering wheel angle, in degrees (deg); turning the steering wheel left (counterclockwise) is positive and turning it right (clockwise) is negative.

- Target steering wheel speed: most vehicles cannot expose this control signal interface; it is recommended to set the steering wheel speed to the maximum value to improve the system response time.

### Steering Feedback Signals

- Actual steering wheel angle feedback: feeds back the actual steering wheel angle, which must be accurate; left turns are positive and right turns are negative.

- Steering driving mode feedback: feeds back the steering driving mode the vehicle is currently in. At least 3 modes are required: autonomous driving mode, manual driving mode, and manually taken-over mode.

- Fault information feedback: feeds back whether the steering control system has a fault.

### Manual Takeover Handling

- For passenger/commercial vehicles:
  
  - When the manually applied steering wheel torque exceeds its takeover threshold, all modules of the vehicle exit autonomous driving, and the steering driving mode feedback indicates taken over;
  
  - For chassis carts and the like, when the cart chassis receives a remote-controller command to steer the vehicle, all modules of the vehicle exit autonomous driving;
  
  - Steering takeover must be independent: at this time the steering driving mode feedback indicates the manually taken-over mode, while the drive driving mode and braking driving mode should feed back the manual driving mode.

- When steering autonomous driving is manually taken over, all modules exit the autonomous driving mode.

### Out-of-Range Handling

The principle of out-of-range handling is to refuse to execute out-of-range commands and exit the autonomous driving mode.
When a control command issued by Apollo exceeds the range defined for the original control command, this is called out of range. When it occurs, steering needs to exit the autonomous driving mode.
For example, if the maximum range of the target steering wheel angle is defined as -500 deg to 500 deg and an angle command issued by Apollo is outside this range, the chassis must refuse to execute this out-of-range command and exit the autonomous driving mode.

## Drive-by-Wire Drive

### Drive Enable

- The drive enable signal controls the vehicle entering or exiting the drive autonomous driving mode. The enable signal must be a **rising-edge triggered** signal, and **in principle there should be 1 enable signal**. If a developer's drive needs several different signals together to be enabled into the autonomous driving state, the logic of those enable signals must be integrated at the lower layer, and in principle only 1 enable signal may be given to Apollo.

- The drive enable signal must be independent and not associated with the enable signal of the steering module. It must satisfy:
  
  - When the longitudinal autonomous driving mode is enabled alone, the steering driving mode does not change, and when a drive control signal is sent, the vehicle can respond to the corresponding control command;
  
  - When the drive autonomous driving mode is disabled alone (exited), the steering driving mode does not change, and when a drive control signal is sent, the vehicle does not respond.

- **In principle, the drive enable signal should be independent of the braking, gear and parking enables. Based on past practical experience**, some vehicles combine the longitudinal drive and braking enable signals into one signal. Such special cases need particular attention: make sure the longitudinal enable is separate from and independent of the lateral enable. If the longitudinal modules are associated with one another, then enabling requires drive, braking, gear and parking to enter the autonomous driving mode together, and disabling makes them exit the autonomous driving mode together.

- If the lateral control is taken over, entering the autonomous driving mode again requires a rising-edge trigger signal.

### Drive Control Signals

Target accelerator pedal position: the control quantity is the accelerator pedal position as a percentage, in the range 0 to 100%, where 100% means the accelerator pedal is pressed to its maximum position.

The vehicle target longitudinal acceleration and the vehicle target drive torque are optional vehicle control signals.

### Drive Feedback Signals

- Driving mode: feeds back the drive driving mode the vehicle is currently in. At least 3 modes are required: autonomous driving mode, manual driving mode, and manually taken-over mode.

- Accelerator pedal position: must accurately feed back the actual current pedal position of the vehicle.

- Longitudinal acceleration: must accurately feed back the current longitudinal acceleration of the vehicle.

- Vehicle speed: must accurately feed back the current vehicle speed. The vehicle speed Apollo requires is in m/s. **Vehicles generally feed back speed in km/h, in which case the unit conversion must be done separately after the DBC is converted into the adaptation code**.

- Wheel speed: for passenger vehicles, the related wheel speed signals must be provided, giving the wheel speed in km/h. If possible, provide wheel motion direction feedback, with the Value Description in the following order: `0x00, FORWARD; 0x01, BACKWARD; 0x02, STANDSTILL; 0x03, INVALID`.

- Engine/motor speed: must accurately feed back the current engine or motor speed, in r/min.

- Fault information: feeds back whether the drive system has a fault.

### Manual Takeover

- When the vehicle chassis receives a manually applied accelerator pedal command (or, for a chassis cart, an acceleration signal issued by the remote controller), all modules of the vehicle exit autonomous driving. Takeover must be independent: at this time the drive driving mode feedback indicates the manually taken-over mode, while the steering driving mode and braking driving mode should feed back the manual driving mode.
- When drive autonomous driving is manually taken over, all modules exit the autonomous driving mode.

### Out-of-Range Handling

When out of range occurs, steering needs to exit the autonomous driving mode. For example, if the target accelerator pedal position range is defined as 0 to 100% and a pedal command issued by Apollo is outside this range, the chassis must refuse to execute this out-of-range command and exit the autonomous driving mode.

## Drive-by-Wire Braking

### Braking Enable

- The braking enable signal controls the vehicle entering or exiting the drive autonomous driving mode. The enable signal must be a **rising-edge triggered** signal, and in principle there should be 1 enable signal. If a developer's braking needs several different signals together to be enabled into the autonomous driving state, the developer must integrate the logic of those enable signals at the lower layer, and in principle only 1 enable signal may be given to Apollo.

- The braking enable signal must be independent and must not be associated with the enable signal of the steering module. It must satisfy:
  
  - When the longitudinal autonomous driving mode is enabled alone, the steering driving mode does not change, and when a braking control signal is sent, the vehicle can respond to the corresponding control command,
  
  - When the drive autonomous driving mode is disabled alone (exited), the steering driving mode does not change, and when a braking control signal is sent, the vehicle does not respond.

- In principle, the drive enable signal should be independent of the drive, gear and parking enables. Based on past practical experience, some vehicles combine the longitudinal drive and braking enable signals into one signal. Such special cases need particular attention: make sure the longitudinal enable is separate from and independent of the lateral enable. If the longitudinal modules are associated with one another, then enabling requires drive, braking, gear and parking to enter the autonomous driving mode together, and disabling makes them exit the autonomous driving mode together.

- If the lateral control is taken over, entering the autonomous driving mode again requires a rising-edge trigger signal.

### Braking Control Signals

- Target brake pedal position: the control quantity is the pedal position as a percentage, in the range 0 to 100%, where 100% means the brake pedal is pressed to its maximum position.

- Target deceleration: optional; the control quantity is the target deceleration of the vehicle.

- Brake light control: optional; controls whether the brake light is lit. The chassis of a typical vehicle lights the brake light by default when braking.

### Braking Feedback Signals

- Driving mode: feeds back the braking driving mode the vehicle is currently in. At least 3 modes are required: autonomous driving mode, manual driving mode, and manually taken-over mode.

- Brake pedal position: the brake pedal position, as a percentage.

- Brake light status: feeds back whether the brake light is lit.

- Fault information: feeds back whether the braking system has a fault.

### Manual Takeover

- When the vehicle chassis receives a manually applied brake pedal command (or, for a chassis cart, a deceleration signal issued by the remote controller), all modules of the vehicle exit autonomous driving. Takeover must be independent: at this time the drive driving mode feedback indicates the manually taken-over mode, while the steering driving mode and braking driving mode should feed back the manual driving mode
- When drive autonomous driving is manually taken over, all modules exit the autonomous driving mode.

### Out-of-Range Handling

When out of range occurs, steering needs to exit the autonomous driving mode. For example, if the target brake pedal position range is defined as 0 to 100% and a pedal command issued by Apollo is outside this range, the chassis must refuse to execute this out-of-range command and make all modes exit the autonomous driving mode.

## Drive-by-Wire Gear

### Gear Enable

The enable is independent and not associated with lateral control. It may be associated with the longitudinal enable signal or enabled independently. The gear enable signal must be a rising-edge triggered signal, and in principle there should be 1 enable signal.

### Gear Control

For the gear control signal, the gear values must follow this order: `0x00: N; 0x01: D; 0x02: R; 0x03: P; 0x04: NONE`. No extra values need to be added.

### Gear Feedback

- Gear information: feeds back the current gear signal, in the same order as the gear control signal: `0x00: N; 0x01: D; 0x02: R; 0x03: P; 0x04: NONE`. No extra values need to be added.

- Fault information: feeds back whether the shifting system has a fault.

## Drive-by-Wire Parking

### Parking Enable

The enable is independent and not associated with lateral control. It may be associated with the longitudinal enable signal or enabled independently. The parking enable signal must be a rising-edge triggered signal, and in principle there should be 1 enable signal.

### Parking Control

When the parking signal is active, the electronic parking brake (EPB) is applied and the vehicle is parked; when the parking signal is inactive, the electronic parking brake is released and the vehicle is not parked.

### Parking Feedback

- EPB switch status: feeds back whether the electronic parking brake is currently applied or released.

- Parking status feedback: feeds back whether parking is currently in autonomous driving mode or manual control mode. At least 2 modes are required: `autonomous driving mode`, `manual driving mode`, and `manually taken-over mode`.

- Parking system fault feedback: feeds back parking system faults.

## Other Body Functions

These include drive-by-wire lighting, which mainly controls turning the high beam, low beam, turn signals and hazard warning lights on and off, together with the corresponding status feedback signals.

Wiper and horn control are also optional.

## Vehicle VIN

A VIN is generally 17 characters. In ASCII format, each ASCII character occupies 1 byte, so 3 consecutive message frames are needed to send it, but the VIN does not need to be updated in real time. Therefore, when the system requests to enter autonomous driving, the VIN is sent over the CAN bus and then kept at that value without further updates, which also reduces the bus load.

### DBC File Requirements

After becoming familiar with Apollo's requirements for vehicle chassis signals above, the second step is to edit the vehicle chassis signal database (DBC) file, setting the communication network structure and the initial value, sign type, precision, size range and values of each signal, and then combining them into the corresponding CAN communication messages to communicate with Apollo. The DBC file is edited below with the CANdb++ software, which has a good visual interface and currently runs only on Windows.

The DBC file will later be converted, with Apollo's translation script tool, from the messages and signals defined for the chassis into C++ program code. Therefore, when editing the DBC, the names, comments and values of the signals must conform to C++ language definition conventions, to ensure that CANBUS communication cannot fail to work during later debugging because of problems in the DBC file. According to the Apollo code requirements, please note the following.

- The control signal name is recommended to be ACU.
  
  When defining ECU names on the network, it is recommended to name the controller on the Apollo side ACU (Apollo Control Unit).
  
  ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_6ada0dd.png)

- The CAN signal ID is recommended to be no greater than 2048.
  
  Passenger vehicle CAN communication currently recommends the standard frame format (CAN Standard 2.0), with Message ID range 0 - 2047; the extended frame is not recommended.
  
  ![message_id.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/message_id_14805ce.png)

- Comments must not contain carriage returns or line feeds, and comments must be in English.
  
  If a message has a comment, the comment must not contain line breaks or Chinese and must be in English.
  
  ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_5b1a1f9.png)

- VAL\_ (enumeration values) (Value Description) must be in English, must not have duplicate names, and must consist of letters or a combination of letters and digits, without symbols.
  
  For most status feedback signals and control signals, such as gear feedback and driving mode feedback, the signal needs to be defined in the Value Description item of the signal definition. The defined names must follow C++ naming conventions, must be in English, must not be duplicated, and must consist of letters or a combination of letters and digits, without symbols. The figure below shows the Value Description definition of the gear feedback signal.
  
  ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_f79d4ca.png)

- For double-type feedback and control signals such as vehicle speed, wheel speed, acceleration and pedal position (percentage), the Value Description item in the DBC must be empty.
  
  For real-time numeric feedback signals and numeric control signals, such as vehicle speed (actual speed), wheel speed feedback (actual wheel speed), pedal control (percentage) and steering angle control (actual angle value), nothing may be added to the Value Description item in the definition.
  
  ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_86e997c.png)

- The range of the steering signal must be filled in with an accurate value range when defined. Note that the precision of the control steering angle is generally no finer than 0.05 deg, and the pedal percentage precision (factor) is no finer than 0.1.
  
  For the Byte Order of all messages, the signals within one DBC can only be defined uniformly: either all in Motorola format or all in Intel format.
  
  ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_31ddeb2.png)
