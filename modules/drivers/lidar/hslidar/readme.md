# hesai 2.0 sdk Apollo adapted driver

## Project Overview

An Apollo driver adapted from the Hesai open-source SDK 2.0 driver.

## Configuration

The default configuration file is the one for AT128, located at `/apollo/modules/drivers/lidar/hslidar/conf/hslidar.pb.txt`

1. Change the output configuration, including frame_id and the point cloud and scan data output channels
2. Change the input configuration, including the device address `device_ip`, UDP port `udp_port`, and PTC port `ptc_port`.
3. Change the paths of the two correction files, `correction_file_path` and `firetimes_path`. The correction files need to be downloaded from the official website.
4. Change the LiDAR model `lidar_type`; the default is "AT128"
5. Change `source_type`: 1 corresponds to normal LiDAR input, 2 to pcap input, and 3 to scan data input. To use scan data input, first set source_type in config_base to RAW_PACKET, then set source_type in the configuration file to 3

## Running

**All drivers must be run in the Apollo Docker environment**

```sh
mainboard -d /apollo/modules/drivers/lidar/hslidar/dag/hslidar.dag
```

or

```sh
cyber_launch start /apollo/modules/drivers/lidar/hslidar/launch/hslidar.launch
```
