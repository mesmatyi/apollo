This document describes how to use Apollo's tools to generate CANBUS code adapted to Apollo.

## Prerequisites

You need to read the following first:

- [Overview](../应用实践/车辆集成教程/准备符合Apollo协议的车辆/概览.md)

- [Vehicles Conforming to the Apollo Drive-by-Wire Standard](../应用实践/车辆集成教程/准备符合Apollo协议的车辆/符合Apollo线控标准的车辆.md)

## Adapting CANBUS to Apollo

### 1. Convert the DBC File into canbus Template Code

The Canbus adaptation code can be generated with Apollo's tools. Before converting the code, make sure the DBC has been completed according to the DBC file requirements above, then open the DBC file with gedit and save it again, converting the encoding to UTF-8.

1. Place the DBC file in the specified directory `apollo/modules/tools/gen_vehicle_protocol`.

2. Modify the configuration file of the DBC conversion script.

   Taking the addition of the GE3 vehicle model as an example, in the `apollo/modules/tools/gen_vehicle_protocol` directory, copy the existing `mkz_conf.yml` file, rename it `ge3_conf.yml`, and modify this configuration file as shown in the figure below:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_7fbaf21.png)

- `dbc_file`: Fill in the name of your DBC file. The DBC file is generally named after the vehicle model and ends with `.dbc`.

- `protocol_conf`: Named the same as the DBC file above; fill in `ge3.yml`.

- `car_type`: Fill in the vehicle model name.

- `sender_list: [ ] `: The sender list, empty by default here.

- `sender`: Change this to match the name of Apollo defined in the DBC. In the ge3 DBC, the Apollo name is defined as SCU.

3. After finishing the `ge3_conf.yml` configuration file, start docker and enter the Apollo container. In the `apollo/modules/tools/gen_vehicle_protocol` directory, find the DBC conversion tool `gen.py` and run:

   ```
   cd modules/tools/gen_vehicle_protocol
   python gen.py ge3_conf.ymal
   ```

   After the above script finishes, the terminal shows that 5 control protocols and 11 feedback protocols were generated.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_f1f6517.png)

   An `output` folder is now generated under the `apollo/modules/tools/gen_vehicle_protocol` directory, containing 2 folders: `proto` and `vehicle`. The code in these two folders is the basic code template to adapt canbus. **Copy** the code in them into Apollo's canbus layer to carry out the code adaptation.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_e0260ae.png)

> Note: After copying the code templates generated in this output folder into the corresponding Apollo directories, delete the folder. If you do not delete it, Apollo will report errors when it is compiled later. The folder has protected permissions, so please run the delete command inside Apollo's docker.

```
rm -rf output/
```

### 2. Merge the Adaptation Code into Apollo

Taking the addition of the ge3 vehicle model as an example, add the code to Apollo as follows:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_7f49ca5.png)

1. Copy the `ge3.proto` file in the `apollo/modules/tools/gen_vehicle_protocol/output/proto` folder into the `apollo/modules/canbus/proto` folder, then modify `chassis_detail.proto` in that folder and add the header import `import "modules/canbus/proto/ge3.proto"` at the top of the file.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_7d03448.png)

In the `message ChassisDetail{}` structure, add the definition of the new vehicle model variable on the last line: `Ge3 ge3 = 21`:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_ce56d91.png)

In the `BUILD` file in the `pollo/modules/canbus/proto` directory, add the dependency on the new `proto` above: `"ge3.proto"`:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_ccdf17d.png)

2. Copy the ge3 folder in `apollo/modules/tools/gen_vehicle_protocol/output/vehicle/` into the `apollo/modules/canbus/vehicle/` folder.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_20984df.png)

### 3. Implement the New Vehicle Control Logic

To implement the new vehicle control logic, write the control logic code in the `apollo/modules/canbus/vehicle/ge3/ge3_controller.cc` file.

The control code mainly consists of broadcasting the vehicle chassis information through `chassis` and `chassis_detail`, using the information parsed from the chassis feedback messages.

`chassis` mainly includes information obtained from the chassis such as vehicle speed, wheel speed, engine speed, pedal feedback and steering angle feedback, while `chassis_detail` is the actual information of each message frame. The code written for this part is as follows:

```
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

To set the autonomous driving mode and edit the related enable logic: in Apollo, the vehicle's driving modes mainly include:

- Complete autonomous driving mode (`COMPLETE_AUTO_DRIVE`): both lateral and longitudinal are enabled,

- Lateral autonomous driving mode (`AUTO_STEER_ONLY`): lateral is enabled, longitudinal is not,

- Longitudinal autonomous driving mode (`AUTO_SPEED_ONLY`): lateral is not enabled, longitudinal is enabled.

The control logic for the vehicle enable control signals is as follows:

```
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

> Note: To add control signal functionality, the control signals that must be added include the vehicle's throttle and brake pedal control, steering control, and gear control.

Other control signals include headlight control, horn control, turn signal control and electronic parking brake control. The code is as follows:

```
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

Add the `CheckResponse` logic. Apollo's program monitors whether the vehicle's underlying layers are in autonomous driving mode, that is, whether the driving mode feedback of the vehicle's lateral, drive and braking modules indicates the autonomous driving state.

If, within one `CheckResponse` cycle, the driving mode feedback of any module of the vehicle indicates takeover or manual driving mode, Apollo will switch the vehicle to emergency stop mode (`Emergency`), that is, all modules are controlled to manual mode, to ensure safety when controlling the vehicle.

The `CheckResponse` cycle may differ between vehicles, and developers need to set the `check` cycle through `retry_num` according to the situation.

Developers do not have to change the original check code scheme; they can map the 3 driving mode feedback messages to `chassis_detail` in Apollo:

`is_eps_online -> steering mode feedback signal`

`is_vcu_online -> drive mode feedback signal`

`is_esp_online -> braking mode feedback signal`

In the file `apollo/modules/canbus/vehicle/ge3/protocol/scu_eps_311.cc`, add the following code:

```
chassis->mutable_check_response()->set_is_eps_online(eps_drvmode(bytes, length) == 3);
```

In the file `apollo/modules/canbus/vehicle/ge3/protocol/scu_vcu_1_312.cc`, add the following code:

```
chassis->mutable_check_response()->set_is_vcu_online(vcu_drvmode(bytes, length) == 3);
```

In the file `apollo/modules/canbus/vehicle/ge3/protocol/scu_bcs_1_306.cc`, add the following code:

```
chassis->mutable_check_response()->set_is_esp_online(bcs_drvmode(bytes, length) == 3);
```

The `CheckResponse` implementation code is as follows:

```
bool Ge3Controller::CheckResponse(const int32_t flags, bool need_wait) {
  int32_t retry_num = 20;
  ChassisDetail chassis_detail;
  bool is_eps_online = false;
  bool is_vcu_online = false;
  bool is_esp_online = false;

  do {
    if (message_manager_->GetSensorData(&chassis_detail) != ErrorCode::OK) {
      AERROR_EVERY(100) << "get chassis detail failed.";
      return false;
    }
    bool check_ok = true;
    if (flags & CHECK_RESPONSE_STEER_UNIT_FLAG) {
      is_eps_online = chassis_detail.has_check_response() &&
                      chassis_detail.check_response().has_is_eps_online() &&
                      chassis_detail.check_response().is_eps_online();
      check_ok = check_ok && is_eps_online;
    }

    if (flags & CHECK_RESPONSE_SPEED_UNIT_FLAG) {
      is_vcu_online = chassis_detail.has_check_response() &&
                      chassis_detail.check_response().has_is_vcu_online() &&
                      chassis_detail.check_response().is_vcu_online();
      is_esp_online = chassis_detail.has_check_response() &&
                      chassis_detail.check_response().has_is_esp_online() &&
                      chassis_detail.check_response().is_esp_online();
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

### 4. Modify the Chassis Vehicle Speed Feedback Protocol

**Key point: convert the vehicle speed feedback unit from km/h to m/s**

By default, the Apollo system uses `m/s` for the vehicle speed feedback. The chassis vehicle speed information is very important to Apollo and is needed for vehicle calibration, control, planning and so on, so when developing the adaptation code, check the vehicle speed feedback unit with particular care. To convert vehicle speed from `km/h` to `m/s`, simply divide the vehicle speed feedback signal by `3.6`.
The message that carries the Ge3 vehicle's speed feedback is in the file `apollo/modules/canbus/vehicle/ge3/protocol/scu_bcs_2_307.cc`, and the speed feedback message is `Scubcs2307::bcs_vehspd{}`, as shown in the figure below:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_9e7ea07.png)

### 5. Register the New Vehicle

1. Register the new vehicle in `modules/canbus/vehicle/vehicle_factory.cc` by creating the following class in that file:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_2ecd09d.png)

2. Add the header file:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_3a20644.png)

3. Add the BUILD dependency: add the `ge3_vehicle_factory` dependency library in the `apollo/modules/canbus/vehicle/BUILD` file.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_45e5eb6.png)

### 6. Update the Configuration Files

1. Add the GE3 vehicle branch in the `apollo/modules/common/configs/proto/vehicle_config.proto` file.

![add_ge3_proto.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/add_ge3_proto_7a42ba6.png)

2. In `apollo/modules/canbus/conf/canbus_conf.pb.txt`, update the configuration to the ge3 canbus communication program.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_6_0/image_1baf464.png)
