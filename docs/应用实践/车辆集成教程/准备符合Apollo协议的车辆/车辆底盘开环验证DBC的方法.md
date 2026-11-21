The previous document described how to use Apollo tools to generate CANBUS adaptation code. To verify the correctness of the CANBUS adaptation code, this document describes how to verify whether the DBC runs properly by testing the vehicle chassis in open loop.

## Prerequisites

You need to read the following first:

- [Overview](../应用实践/车辆集成教程/准备符合Apollo协议的车辆/概览.md)

- [Vehicles Conforming to the Apollo Drive-by-Wire Standard](../应用实践/车辆集成教程/准备符合Apollo协议的车辆/符合Apollo线控标准的车辆.md)

- [Generating CANBUS Adaptation Code with Apollo Tools](../应用实践/车辆集成教程/准备符合Apollo协议的车辆/利用Apollo工具生成CANBUS适配代码.md)

## Verifying the DBC with Open-Loop Vehicle Chassis Testing

### Open-Loop Testing

After the vehicle chassis DBC has been determined, carry out open-loop testing of the signals defined in the DBC. The main purposes of open-loop testing are to test whether the vehicle drive-by-wire signals match the actual functions of the vehicle, whether the vehicle's steering and acceleration/deceleration performance responses meet the vehicle's drive-by-wire requirements, and whether the vehicle takeover logic meets the requirements.

In the open-loop chassis testing, developers should test against the key DBC requirements described earlier, such as the independence of the vehicle's lateral and longitudinal enables, the independence of lateral and longitudinal takeover, whether every control signal meets the control requirements and control boundaries, and whether the feedback signals are fed back correctly. In terms of performance, check whether the steering and acceleration/deceleration delays of the control signals meet Apollo's control performance requirements, and whether the overshoot error is within the range given in the drive-by-wire list. Please test the control signals according to the performance requirements in the drive-by-wire list.

### teleop Chassis Joint Debugging Test

The chassis joint debugging test establishes canbus communication between Apollo and the vehicle, and then tests whether the control signals issued by Apollo (such as accelerate/decelerate/steer/enable) can accurately control the vehicle, whether the chassis feedback signals of the vehicle (such as current pedal percentage feedback, current steering angle feedback, enable feedback and takeover feedback) reflect the actual state of the vehicle, and verifies that the vehicle chassis can accurately execute the control commands issued by Apollo.

The teleop test tool is described below.

#### teleop Test Tool

Apollo provides a teleop test tool at `apollo/modules/canbus/tools/teleop.cc`. Enter the following command in `term`:

```
bash scripts/canbus_teleop.sh
```

to enter the teleop interface, as shown in the figure below:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_531fa30.png)

Press the `h` key to bring up the help screen shown above, which tells you how to use the Teleop tool. The teleop control commands are briefly introduced below.

- **Set Action: executes Apollo's enable control of the vehicle**

  - Pressing `m` and `0` together executes the reset command, and the vehicle exits autonomous driving mode,

  - Pressing `m` and `1` together executes the start command, and the vehicle enters autonomous driving mode.

- **Set Gear sets the gear; press `g` together with a `number` to set the corresponding gear**

  - Press `g`+`0` to shift into N (neutral),

  - Press `g`+`1` to shift into D (drive),

  - Press `g`+`2` to shift into R (reverse),

  - Press `g`+`3` to shift into P (park),

  Other gear control commands are not needed for now; according to the DBC requirements, a typical vehicle's gear control commands are just these.

- **Throttle/Speed up increases the throttle pedal by 2% each time, accelerating the vehicle**

  Press the `w` key to increase the throttle pedal by 2% and accelerate the vehicle. If a brake command is currently being executed, pressing `w` reduces the brake pedal amount by 2%.

  The throttle pedal control range is 0 to 100%, where 100% is equivalent to the throttle pedal being fully depressed. By default, each press of the `w` key increases the throttle pedal by 2%; you can change this value according to real-vehicle testing. Based on experience, a change of 2% each time is appropriate.

  > Note: Please first use teleop to shift into D before executing the throttle command, test in an open area, and pay attention to safety.

- **Set Throttle sets the throttle pedal percentage**

  Press `t`+`number` to directly set a specific throttle pedal percentage; the throttle pedal can be set to a percentage from 0 to 100. For example, executing t20 directly sets the current throttle pedal amount to 20% and sets the brake pedal percentage to 0, which is consistent with real driving logic: if the throttle pedal is pressed, the brake pedal cannot be pressed.

  > Note: When directly setting the throttle pedal percentage, do not set too large a value each time, test in an open area, and pay attention to safety. Execute this command with the vehicle in D.

- **Brake/Speed down increases the throttle pedal amount by 2% each time, accelerating the vehicle**

  Press the `s` key to increase the brake pedal percentage and decelerate the vehicle. If a throttle acceleration command is currently being executed, pressing the `s` key reduces the throttle pedal percentage.

  > Note: Please first use teleop to shift into D before executing the throttle command, test in an open area, and pay attention to safety.

- **Set Brake sets the brake pedal percentage**

  Press `b`+`number` to directly set a specific brake pedal percentage; the brake pedal can be set to a percentage from 0 to 100. For example, executing b20 directly sets the current brake pedal amount to 20% and sets the throttle pedal percentage to 0, which is consistent with real driving logic: if the brake pedal is pressed, the throttle pedal cannot be pressed.

  > Note: When directly setting the throttle pedal percentage, do not set too large a value each time, test in an open area, and pay attention to safety. Execute this command with the vehicle in D.

- **Steer LEFT turns the steering wheel left by 2% each time**

  Press the `a` key to turn left by 2% of the maximum steering wheel angle each time; the actual rotation angle is obtained by multiplying the maximum steering wheel angle set for the vehicle by 2%.

  This command can be executed while the vehicle is stationary or after the vehicle has started.

- **Steer RIGHT turns the steering wheel right by 2% each time**

  Press the `s` key to turn right by 2% of the maximum steering wheel angle each time; the actual rotation angle is obtained by multiplying the maximum steering wheel angle set for the vehicle by 2%.

  This command can be executed while the vehicle is stationary or after the vehicle has started.

- **Parking Brake turns on the electronic parking brake**

  Press the `P` key (note the uppercase P) to manually switch the vehicle's electronic parking brake on or off. This function works depending on whether the vehicle provides a control interface for the electronic parking brake.

  > Note: When applying or releasing the electronic parking brake, use teleop to put the vehicle in P.

- **Emergency Stop**

  Press the `E` key (note the uppercase E) to perform an emergency stop of the vehicle, which applies 50% braking by default.

  It is recommended to use this function as little as possible during testing, as it feels poor. Pay attention to the surroundings when debugging the vehicle. In case of an emergency, take over the vehicle manually in time by stepping on the brake pedal with an external pedal.

#### Diagnostic Tools

After understanding the basic operation of teleop, execute specific control commands on the vehicle with the corresponding instructions, then use Apollo's visual monitoring tool `cyber_monitor` to view the vehicle's current feedback signals and confirm whether the vehicle's execution result is correct after the control command is issued.

Apollo provides a visual monitoring tool that can monitor the chassis `chassis` and `chassis_detail` information. Run:

```
cyber_monitor
```

In the file `apollo/modules/canbus/conf/canbus.conf`:
Change the configuration `--noenable_chassis_detail_pub` to `--enable_chassis_detail_pub`, which turns on `chassis_detail` chassis detail information, so that the original information of every message frame of the chassis feedback signals can be viewed.

Change the configuration `--receive_guardian` to `--noreceive_guardian`, which turns off guardian mode and enters canbus debugging mode, so that the vehicle can be controlled during teleop. The figure below shows the modified `canbus_conf` configuration file.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_0f6580b.png)

The `cyber_monitor` interface opens as follows:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_15359f3.png)

You can see that both `chassis` and `chassis_detail` have data, at a frequency of about 100 Hz. The data under `chassis` is mainly the vehicle's actual speed, the feedback of the actual pedal percentages, the actual steering wheel angle percentage, the actual gear feedback and so on, while `chassis_detail` holds all the information of the feedback messages uploaded by the chassis to Apollo (that is, all the messages Apollo receives from the chassis).
The figure below shows the `chassis` information:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_5eae9f5.png)

The figure below shows the `chassis_detail` information:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_5095afc.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_ba05bf2.png)
