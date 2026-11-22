## LiDAR Integration Requirements

The Apollo autonomous driving open platform mainly provides complete solutions for campus, education and low-speed scenarios. Apollo is LiDAR-centric and supports the mainstream mechanical and semi-solid-state LiDAR solutions on the market. Apollo currently supports the integration of some LiDARs from brands such as Hesai, RoboSense, LSLIDAR, Velodyne, Valeo and LeddarTech.

LiDAR technology is developing rapidly, and device manufacturers keep developing more, better and more cost-effective LiDARs. To help manufacturers set up LiDAR test environments faster and help developers use the latest LiDARs, this guide describes in detail how to develop, test and contribute Apollo LiDAR drivers.

## LiDAR Integration Process

### Driver Development and Testing

- Following this document and the reference code, complete the LiDAR driver development;

- Schedule and run the code, and confirm that the driver output meets Apollo's requirements;
  
  ### Becoming a Contributor

- Connect a real device, run the code, and confirm that the output information is correct;

- Record a test video and the LiDAR output information;

- Provide the test process data and developer information to the Apollo team;

- Submit the code to Github and wait for the Apollo team to review it.
  
  ### Contributor Promotion

- The hardware device is featured in the hardware showcase on the official website of the Apollo development platform;

- As a contributor, your information is displayed on the Apollo official website.

## LiDAR Technical Requirements

* [Mandatory] The LiDAR supports outputting point cloud data through its Ethernet port over the TCP or UDP protocol;
* [Mandatory] The LiDAR can stably output point clouds at 10 Hz;
* [Mandatory] The x, y, z, intensity and timestamp information of every LiDAR point can be obtained;
* [Recommended] Provide a LiDAR C++ SDK through which the raw data packets can be obtained, together with an interface that converts raw data packets into the point cloud format;
* [Recommended] The LiDAR SDK can filter the LiDAR point cloud by angle and distance;
* [Recommended] The ring information of every LiDAR point can be obtained;
* [Mandatory] [Not supported] Apollo's perception algorithms do not currently support perception based on 2D LiDAR, so integration of 2D LiDARs is not supported for now. If you use a 2D LiDAR, we recommend involving experts who have a good understanding of the Apollo system.

## Development Process

### Preparation

1. Learn the [Apollo component development process](https://apollo.baidu.com/community/Apollo-Homepage-Document?doc=BYFxAcGcC4HpYIbgPYBtXIHQCMEEsATAV0wGNkBbWA5UyRFdZWVBEAU0hFjwDsD2AD0ygKqIA);
2. Learn about github pull requests: [Pull request documentation](https://docs.github.com/zh/pull-requests);
3. Prepare the LiDAR SDK driver and compile Linux dynamic libraries on the x86_64 and aarch64 architectures;
4. Become familiar with the Hesai and RoboSense LiDAR drivers under `modules/drivers/lidar`;
5. Understand the apollo lidar driver framework and choose either polling or subscription to develop the driver.

### Development

1. Fork the Apollo repository: go to [https://github.com/ApolloAuto](https://github.com/ApolloAuto) and click the Fork button in the upper left corner;
2. Using the Hesai and RoboSense drivers as templates, develop the new LiDAR driver;
3. View the driver output information with cyber tools;
4. Submit the code, pass the review, and get the code merged.

## Introduction to the LiDAR Development Framework

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a2f71a5.png)

The LiDAR driver runtime framework must meet the following two goals:

- Read data from the LiDAR device and convert the raw device data into the point cloud data format required by the perception component.
- To save as much disk space as possible when recording autonomous driving data, only the raw data is saved to disk by default.

Point cloud data takes up much more space than raw data packets, so the driver framework provides two modes, `ONLINE_LIDAR` and `RAW_PACKET`, which read binary data from the network interface connected to the lidar and from the scan channel, respectively. During autonomous driving, the `ONLINE_LIDAR` mode should be used to read from the network port connected to the lidar and output both the raw data and the point cloud data. After the data has been recorded, starting the `RAW_PACKET` mode can restore the point cloud data of that time from the recorded raw data.

## LiDAR Driver Code Development

### Driver Development Directory

The lidar driver directories are placed uniformly under the `drivers/lidar` directory of the Apollo source code. A new device needs to create a new directory structure, with the contents shown below.

```bash
xxlidar:  # create the directory named after the lidar type
    - conf:  # the conf directory stores the lidar driver configuration
    - dag:  # the dag directory stores the dag file that starts the lidar driver
    - launch:  # the launch directory stores the launch file that starts the lidar driver
    - proto:  # stores the conf format definition and other custom-type proto configurations
    - src:  # stores the source code of the lidar driver
    - README_cn.md  # Chinese description of the lidar driver
    - README_en.md  # English description of the lidar driver
```

### LiDAR SDK Preparation

Considering vendor code maintenance, and the compatibility of third-party code libraries with Apollo's code conventions and build tools, Apollo recommends speeding up driver development by introducing an SDK package.

For the SDK code, the use of cmake or other build toolchains is not restricted, but it must support compilation on the x86_64 and aarch64 architectures and installation into a specified directory, and the installation output must satisfy:

* It contains an include directory. The header file names (paths) in the include directory must not be the same as those of the system or third-party libraries, otherwise other programs will have reference errors. It is recommended to place a subdirectory named after the lidar under include, for example `include/xxlidar/point_cloud.h`.
* It contains a lib directory in which the so libraries produced by the build are stored. Ideally only one so is output, and it must be a dynamic link library, not a static .a library;

Once the above is ready, provide the modified code to the Apollo team. The Apollo team will complete the compilation (some compile parameters will be modified for compatibility with various CPU models) and the deb package creation, and after confirming that it can be released, the Apollo team will publish the deb package to the Apollo apt source.

How to reference the SDK: refer to the BUILD and cyberfile.xml configurations under the sample hesai and rslidar.

* BUILD: add the `-l` parameter in the build file to use it directly,
* cyberfile.xml: add the SDK dependency; the package name will be provided after the code is given to the Apollo team.

During your own debugging stage, simply install the build output into the `/usr/` or `/usr/local` directory.

### Configuration File Description

The driver configuration must include the basic lidar driver configuration and the custom configuration.
The user-defined configuration mainly defines items such as the lidar model, IP, port and filtering rules. A complete configuration example is as follows:

```bash
# Basic configuration
base_config {
    scan_channel: "/apollo/sensor/hesai40/Scan"
    frame_id: "hesai40"
    pointcloud_channel: "/apollo/sensor/hesai40/PointCloud2"
    source_type: ONLINE_LIDAR
}
# Custom configuration
model: HESAI40P
ip: "192.168.20.13"
lidar_recv_port: 2368
gps_recv_port: 10110
start_angle: 0
time_zone: 8
```

The basic configuration consists mainly of common settings, which are already defined in the Apollo driver. The format is:

```bash
message LidarComponentBaseConfig {
    required string scan_channel = 1;  // name of the channel that outputs the raw data
    required string point_cloud_channel = 2;  // name of the channel that outputs the point cloud data
    required string frame_id = 3;  // the frame_id of the lidar; must be globally unique, and is used by extrinsic transformation and the perception programs
    required SourceType source_type = 4; // source of the binary data; when defined as ONLINE_LIDAR, the binary data is collected from the lidar and written to scan_channel; when defined as RAW_PACKET, the binary data is collected from scan_channel.
    optional uint8_t lidar_id = 5; // records a lidar serial number; must be globally unique and is used by the perception programs; not required
    optional uint16_t buffer_size = 6 [default = 50]; // size of the buffer allocated for the point cloud; usually does not need to be set
}
```

### LiDAR Driver Development Interface

Apollo officially provides a LiDAR driver component base class. A new lidar driver must inherit from this base class. The interfaces of the base class are as follows:

**LiDAR driver component base class**

```bash
// LidarComponentBase template class; T is the type of the scan data
template <typename T>
class LidarComponentBase: public cyber::Component<T> {
public:
  // The user needs to implement the Init method and call InitBase in it to complete the initialization configuration;
  void Init() = 0; 

  // The user needs to implement ReadScanCallback. When the RAW_PACKET mode is selected, the component reads data from the scan channel. The user needs to implement the process of feeding the scan data it reads into the lidar sdk
  void ReadScanCallback(shared_ptr<T> scan_message);

protected:
   // Implemented in the base class: creates the Scan/Pcd writers, etc.
   bool InitBase(const driver::lidar::BaseConfig& base_config);

   // Implemented in the base class: writes data to the scan channel
   bool WriteScan(const std::shared_ptr<T>& scan_packet);

   // Implemented in the base class: allocates a point cloud object. Note that this method must be used to obtain the output point cloud object, otherwise performance problems may be introduced;
   std::shared_ptr<driver::PointCloud> AllocatePointCloud();

   // Implemented in the base class: writes data to the pcd channel
   bool WritePointCloud(const std::shared_ptr<driver::PointCloud>& pcd);   
}
```

The common directory of the lidar driver library provides the base class LidarComponentBase. The base class reads the basic configuration, initializes the point cloud and scan read/write channels, and provides two data input modes, `RAW_PACKET` and `ONLINE_LIDAR`. The adapter needs to pay attention to implementing the following:

* The adapter needs to implement the driver's behavior in both the `ONLINE_LIDAR` and `RAW_PACKET` input modes. In the `ONLINE_LIDAR` mode, data is read from the network card and, after being parsed by the SDK, the point cloud and scan information are output; in the `RAW_PACKET` mode, the lidar data is read from the scan channel and the binary data in the scan data must be parsed by the SDK to output the point cloud data. The difference between the two modes is that `ONLINE_LIDAR` needs to output data on the scan channel, whereas `RAW_PACKET` needs to obtain data from the scan channel and then output the point cloud according to the data obtained.

* Driver initialization: the user implements the Init function to configure the SDK's network card data listening and to set the SDK's behavior according to the two input modes above; in this process, the InitBase function is called to complete the initialization configuration.

* Calling the scan publish function: LidarComponentBase provides the function WriteScan for packaging and publishing scan data. When the adapter receives a data packet, it needs to pass in the binary data packet. After execution, the scan data is published on the configured channel.
  
  > Note: Make sure to call this function only in the `ONLINE_LIDAR` mode.

* Implementing the writing of scan channel data: the user implements the ReadScanCallback function to feed the custom-type scan data into the SDK.

* Calling the pcd publish functions: LidarComponentBase provides the functions AllocatePointCloud and WritePointCloud for packaging and publishing point cloud data. The user needs to call AllocatePointCloud to allocate the point cloud memory, and call WritePointCloud to publish after the point cloud information has been written.

Pseudocode for adapting the RoboSense driver:

```bash
// rslidar_component.h
class RslidarComponent : LidarPacketComponentBase<ScanType> {
public:
  bool Init() override; 
  // How to handle the received apollo scan data
  void ReadScanCallback(const std::shared_ptr<ScanType>& scan_message) override;

     // Adapter-defined point cloud callback function, returns point cloud data of the type corresponding to the sdk
  void RsPointCloudCallback(raw_pcd_message);
  void RsPacketCallback(raw_scan_message);
}

// ------------------------------------------------------------------------
// rslidar_component.cpp
void RslidarComponent::Init() override {
  // Read the configuration file
  // Initialize the RoboSense sdk...
  // Register the sdk scan frame callback and point cloud frame callback
  sdk->regiest(this->RsPacketCallback);
  sdk->regiest(this->RsPointCloudCallback);

  // Call the parent class method to initialize the cyber node
  InitBase(base_config);
}

int RslidarConvertCompoment::ReadScanCallback(shared_ptr<ScanType> scan_message) override {
    // When scan data is received, call the Proc function to feed the data into the sdk for parsing
    sdk->decodePacket(scan_message);
}

void RslidarPacketComponent::packetCallback(const RslidarPacket& rslidar_packet) {
      std::shared_ptr<ScanType> scan_packet;
      // convert RslidarPacket to ScanType ...

    WriteScan(scan_packet);
    // Call the method provided by the LidarComponentBase parent class to package and broadcast the scan data
}

// Adapter-defined point cloud callback function, returns point cloud data of the type corresponding to the sdk
void RslidarConvertCompoment::pointCloudPutCallback(const RslidarRawMessage& raw_message) {
  shared_ptr<ApolloMsg> pcd_message = AllocatePointCloud(); // Call the parent-class method to get an empty point cloud whose memory has already been allocated

  // The adapter writes raw_message into pcd_message
  for (int i = 0; i < raw_message.size(); ++i) {
    pcd_message.mutable_point(i).set_x(raw_message.point(i).x());
    pcd_message.mutable_point(i).set_y(raw_message.point(i).y());
    ...
  }
  WritePointCloud(pcd_message); // Call the parent-class method to send the point cloud data to the channel
}
```

### Run and Debug

```bash
# Build
bash apollo.sh build_opt_gpu drivers/lidar

# Run: change to the path of the dag you wrote
mainboard -d /apollo/modules/drivers/lidar/xxlidar/dag/xxlidar.dag

# Start cyber_monitor to check whether the custom scan channel is output normally and whether the pointcloud channel has a stable 10 Hz output
cyber_monitor

# Start the cyber_visualizer visualization tool to view the point cloud: click Show PointCloud and select the corresponding channel
cyber_visualizer
```

Check that the point cloud output matches the actual scene. The following is a 16-beam example:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_0252d80.png)