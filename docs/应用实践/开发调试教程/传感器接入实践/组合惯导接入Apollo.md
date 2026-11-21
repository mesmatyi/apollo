## GNSS/INS Integration Requirements

The Apollo autonomous driving open platform mainly provides complete solutions for campus, education and low-speed scenarios. Although Apollo's SLAM support continues to grow, its main scenarios are still based on HD maps and high-precision localization. Apollo currently supports some GNSS/INS devices from brands such as Huace, Daoyuan, Broadgnss (Bosheng Shang), Starneto and Novatel.

GNSS/INS units currently used for L4 autonomous driving are still costly and their products iterate frequently. To help manufacturers apply navigation devices to autonomous driving scenarios more quickly, this guide describes in detail how to develop, test and contribute Apollo GNSS/INS drivers.

## GNSS/INS Integration Process

### Driver Development and Testing

- Following this document and the reference code, complete the driver development;
- Schedule and run the code, and confirm that the driver output meets Apollo's requirements.
  
  ### Becoming a Contributor
- Connect a real device, run the code, and confirm that the output information is correct;
- Record a test video and the driver output information;
- Provide the test process data and developer information to the Apollo team;
- Submit the code to Github and wait for the Apollo team to review it.
  
  ### Contributor Promotion
- The hardware device is added to the Apollo ecosystem support list;
- As a contributor, your information is displayed on the Apollo official website.
  
  ## GNSS/INS Device Integration Requirements
* [Recommended] The GNSS/INS can stably output IMU-related information at 100 Hz;
* [Recommended] The GNSS/INS supports RTK positioning enhancement;
* [Mandatory] The GNSS/INS supports PPS time synchronization output;
* [Mandatory] The GNSS/INS requires dual-antenna heading support;
* [Mandatory] If it relies on external RTK differential information as input, it must support outputting $GPGGA data, and the GPGGA data and the pose data must be output through the same data line. [Technical]

## GNSS/INS Device Output Data Requirements

* [Mandatory] GPSWeek: the number of weeks from 1980/1/6 to the present (Greenwich time);
* [Mandatory] GPSTime: the number of seconds from 0:00:00 on the Sunday of the current week to the present;
* [Mandatory] Latitude: latitude (-90° to 90°);
* [Mandatory] Longitude: longitude (-180° to 180°);
* [Mandatory] Altitude: altitude, in meters;
* [Mandatory] Heading (Yaw): heading angle, 0 to 359.99, positive clockwise from north (if not, convert it yourself) (output at least one of the raw IMU value and the corrected value);
* [Mandatory] Pitch: pitch angle (-90 to 90), positive when the vehicle nose points up (output at least one of the raw IMU value and the corrected value);
* [Mandatory] Roll: roll angle (-180 to 180), positive when the vehicle body leans right (output at least one of the raw IMU value and the corrected value);
* [Mandatory] Gyro X/Y/Z: gyroscope X, Y and Z axes, in °/s;
* [Mandatory] Acc X/Y/Z: acceleration along the X, Y and Z axes (output at least one of the raw IMU value and the corrected value);
* [Mandatory] Ve, Vn, Vu: east, north and up velocities, in m/s;
* [Mandatory] SolutionStatus: system status, defined by reference to [Solution Status](https://docs.novatel.com/OEM7/Content/Logs/BESTPOS.htm#SolutionStatus); it must at least distinguish whether it is integrated navigation;
* [Mandatory] PositionType: satellite status, by reference to [Position or Velocity Type](https://docs.novatel.com/OEM7/Content/Logs/BESTPOS.htm#Position_VelocityType); it must at least include no heading, RTK float solution and stable solution;
* [Recommended] Latitude_std, Longitude_std, Altitude_std: standard deviations of latitude, longitude and altitude;
* [Recommended] Heading_std, Pitch_std, Roll_std: standard deviations of heading, pitch and roll angles;
* [Recommended] Ve_std, Vn_std, Vu_std: standard deviations of east, north and up velocities, in m/s;
* [Recommended] SatelliteNum: number of satellites used in the solution;
* [Recommended] Age: differential delay.

## Development Process

### Preparation

1. Learn the [Apollo component development process](https://apollo.baidu.com/community/Apollo-Homepage-Document?doc=BYFxAcGcC4HpYIbgPYBtXIHQCMEEsATAV0wGNkBbWA5UyRFdZWVBEAU0hFjwDsD2AD0ygKqIA);
2. Learn about github pull requests: [Pull request documentation](https://docs.github.com/zh/pull-requests);
3. Become familiar with the Huace and Daoyuan drivers under `modules/drivers/gnss`.

### Development

1. Fork the Apollo repository: go to https://github.com/ApolloAuto and click the Fork button in the upper left corner;
2. Using the novatel, Huace, Daoyuan and Broadgnss (Bosheng Shang) drivers as templates, develop the new GNSS/INS driver;
3. View the driver output information with cyber tools;
4. Submit the code, pass the review, and get the code merged.
   
   ## Code Development Notes
   
   ### Driver Development Output Targets

The output information and frequency required of the driver code are as follows:

| Channel                           | Format  | Description        | Required | Frequency Requirement              |
| --------------------------------- | --- | --------- | ---- | ----------------- |
| /apollo/sensor/gnss/best_pose     |     | RTK positioning information   | Yes    | Not less than 1 Hz            |
| /apollo/sensor/gnss/corrected_imu |     | Corrected IMU information    | Yes    | 100 Hz (fluctuation within 30%) |
| /apollo/sensor/gnss/odometry      |     | INS information     | Yes    | 100 Hz (fluctuation within 30%) |
| /apollo/sensor/gnss/ins_stat      |     | Status information      | Yes    | Not less than 1 Hz            |
| /apollo/sensor/gnss/heading       |     | Heading information | No    | Not less than 1 Hz            |
| /apollo/sensor/gnss/imu           |     | Raw IMU information   | No    | 100 Hz (fluctuation within 30%) |

### GNSS Driver Development Notes

The gnss driver directory is placed uniformly under the `drivers/gnss` directory of the apollo source code, with the following structure:

```bash
- conf:  # gnss configuration; all vendors share one configuration
- dag:  # dag file; all vendors share one DAG
- launch:  # launch file; all vendors share one Launch file
- proto:  # stores the conf format definition; adding a new gnss requires changes to this proto configuration
- parse:  # data parsing code; adding a new gnss requires adding a new parser type
- stream:  # base classes for data stream input, supporting serial, tcp, udp and can protocols; users usually do not need to modify it
```

#### gnss Driver Code Framework

The stream directory is a base library with built-in support for the can, ntrip, serial, tcp and udp protocols. Each stream needs to implement the Connect and read methods. At runtime the read method is called continuously to obtain data, which is written into data_ of the Parser class object defined in parser/parser.h.

The parser is responsible for parsing the gnss data of the various vendors. Integrating a new INS requires adding a new parser type, which must inherit from the Parser base class and implement the following functions:

* CreateXX function: creates a custom-type Parser, and then adds a call for that type in parser.h;
* GetMessages function: this is the key function for integrating a new gnss. The method takes a message array as input. For devices compatible with the novatel protocol, the data can be parsed and returned directly; for a custom single-record data output or CAN protocol output, you need to maintain the GnssBestPose, Imu, Heading, Ins and InsStat objects yourself and update the latest data values into them. The gnss driver library already has built-in support for some INS devices, whose implementations you can refer to.

#### Code Development Notes

Integrating a new gnss driver usually requires the following modifications:

* Add the gnss type in Stream.Format in proto/config.proto;

* Implement the data parsing library for your own type under the parser path: refer to the existing code according to your data protocol
  
  * If the output is typed binary data, refer to the implementation of novatel_parser.cc;
  * If the output is one complete binary record, refer to the implementation of broadgnss_parser/broadgnss_binary_parser.cc;
  * If the output is one complete text record, refer to the implementation of huace_parser/huace_text_parser.cc;
  * If the output is via the CAN protocol, refer to the implementation of huace_parser/huace_can_parser.cc;

* Add your CreateParser function in parser/parser.h;

* Modify the conf/gnss_conf.pb.txt configuration and add usage notes for the new gnss type. Note that they must be added as comments; do not change the default effective values;
  
  > Note: Try not to modify other code, especially parser/data_parser.cc. If you must modify it, contact the Apollo team, otherwise the PR will not be merged.

### Run and Debug

```bash
# Build
bash apollo.sh build_opt_gpu drivers/gnss

# Change the data configuration in modules/drivers/gnss/conf/gnss_conf.pb.txt to your own INS configuration
vim modules/drivers/gnss/conf/gnss_conf.pb.txt

# Run the gnss driver
mainboard -d /apollo/modules/drivers/gnss/dag/gnss.dag

# Start cyber_monitor to check whether the output is normal and whether the frequency meets the above requirements
cyber_monitor
```

Mainly check whether the frequencies of the 4 channels in the red box meet the requirements, and check whether the position, time and other fields in them are correct. If you have an apollo map, it is also recommended to start the localization module to check whether the vehicle's position and heading match those in the map, and to verify through acceleration and deceleration control that the acceleration and deceleration are consistent with expectations.

![image \(1\).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image%20%281%29_2cb50b4.png)
