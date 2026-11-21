# **Seyond Lidar Driver**

## 1 Project Overview

 **seyond** is the Seyond (图达通) LiDAR driver for the Apollo platform. It supports the *Falcon/Robin* series of LiDARs. 

## 2 Running

**All drivers must be run in the Apollo Docker environment**

```sh
cyber_launch start /apollo/modules/drivers/lidar/seyond/launch/seyond.launch
```

or

```sh
mainboard -d /apollo/modules/drivers/lidar/seyond/dag/seyond.dag
```

Default topic names:

- Point cloud -- /apollo/sensor/seyond/PointCloud2"
- Scan -- /apollo/sensor/seyond/Scan

## 3 Parameters
| Parameter          | Default      | Description  |
| :--------:         | :---------:  | :---------:   |
| device_ip          | 172.168.1.10 | LiDAR IP   |
| port               | 8010         | TCP port   |
| udp_port           | 8010         | UDP port   |
| reflectance_mode   | true         | false: intensity mode, true: reflectance mode   |
| multiple_return    | 1            | Return mode   |
| coordinate_mode    | 3            | Coordinate transformation mode, 0: LiDAR native coordinate system, 3: right-handed coordinate system   |
| max_range          | 2000.0       | Maximum range limit (unit: m)   |
| min_range          | 0.4          | Minimum range limit (unit: m)   |
| log_level          | "info"       | Log level limit (info warn error)    |
