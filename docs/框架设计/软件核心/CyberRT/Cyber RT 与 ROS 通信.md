## Overview

The new version of cyber has reworked the framework to support communication between cyber and ros. It has the following functional highlights:

**Conversion of commonly used messages, ready to use out of the box at low cost:**

The new version of cyber has opened up message communication for some commonly used sensors and implemented the corresponding message conversion functions, including message formats such as point clouds and IMU. Users who care about this part of message communication only need to align the channels at both ends in the configuration and start it to use it

**A plugin-based message conversion framework to improve development efficiency:**

If users also want to implement communication for messages other than those above, the new version of cyber also provides a plugin-based message conversion framework. Users do not need to care about the communication interfaces of cyber and ros; they only need to implement the message conversion function and configure the corresponding channels, and they can receive messages sent from ros in the Apollo modules, further improving development efficiency

## Technical Solution

### Flattening the Communication Barrier

ros uses the rclcpp communication interface, while cyberRT has its own set of communication interfaces. Because of the huge differences in the underlying versions of the message middleware and the application-layer interfaces, the two are incompatible with each other, and the migration and adaptation costs would be quite high.

Therefore, in the new cyberRT version, we aligned the versions of the communication middleware and developed ros-bridge to adapt to the communication interface of ros 2, implementing a path for messages from cyber to ros_bridge to ros, and a path from ros to ros_bridge to cyber:

![cyber_ros_bridge_1](./images/cyber_ros_bridge_1.png)]

### Plugin-Based Message Conversion Framework, Lower Development Cost

Besides dependency alignment and interface adaptation, because Apollo's messages and ros messages are defined differently, users of ros bridge cannot avoid the message conversion logic between them. This message conversion code is tightly coupled with cyber's communication interface and ros's rclcpp interface, which makes it costly to learn, hard to use, and difficult to maintain. Therefore, based on the cyber plugin mechanism in Apollo 9.0, we decoupled the message conversion logic from the internal running logic of ros_bridge:

![cyber_ros_bridge_2](./images/cyber_ros_bridge_2.png)

## Usage

### 1. Prerequisite: Deploy Apollo

To use the performance analysis functions of the new version of cyber, you only need to deploy the latest version of Apollo

#### Source Code

Clone the latest Apollo code

```bash
git clone https://github.com/ApolloAuto/apollo.git
```

Use the script to start and enter the container

```bash
bash docker/scripts/dev_start.sh
```

Compile Apollo

```bash
bash apollo.sh build
```

Configure the ros environment

```bash
buildtool rosenv -t # -t means using the Tsinghua mirror
```

> If you need to implement communication for messages that are not official ros messages, you need to use the -p parameter to specify the source path of the non-standard messages, which is generally the src path in the user's existing ros project
> To specify the ros distribution, you can use the -c parameter, for example buildtool rosenv -c humble -t. Currently ubuntu 22 supports iron and humble, and ubuntu 20 supports foxy and galactic 

Source the ros script

```bash
source /opt/ros/iron/setup.sh    # If your ubuntu is 22.04
source /opt/ros/galactic/setup.sh # If your ubuntu is 20.04
```

> Be sure to source ros's setup.sh script before compiling ros_bridge, otherwise the compilation will fail

Compile ros bridge

```bash
bash apollo.sh build cyber
```

#### Package Management

Clone the latest Apollo project

```bash
git clone https://github.com/ApolloAuto/application-core.git
```

Install aem, then start and enter the container

```bash
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://apollo-pkg-beta.cdn.bcebos.com/neo/beta/key/deb.gpg.key | sudo gpg --dearmor -o /etc/apt/keyrings/apolloauto.gpg
sudo chmod a+r /etc/apt/keyrings/apolloauto.gpg
echo \
    "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/apolloauto.gpg] https://apollo-pkg-beta.cdn.bcebos.com/apollo/core"\
    $(. /etc/os-release && echo "$VERSION_CODENAME") "main" | \
    sudo tee /etc/apt/sources.list.d/apolloauto.list
sudo apt-get update
sudo apt install apollo-neo-env-manager-dev --reinstall
aem start
```

Install the Apollo packages

```bash
buildtool build
```

Install the cyber source code

```bash
buildtool install cyber
```

Configure the ros environment

```bash
buildtool rosenv -t # -t means using the Tsinghua mirror
```

> If you need to implement communication for messages that are not official ros messages, you need to use the -p parameter to specify the source path of the non-standard messages, which is generally the src path in the user's existing ros project

Source the ros script

```bash
source /opt/ros/iron/setup.sh    # If your ubuntu is 22.04
source /opt/ros/galactic/setup.sh # If your ubuntu is 20.04
```

Compile ros bridge

```bash
buildtool build -p cyber
```

### 2. Using the ros_bridge Program in cyber by Scenario

> Notes:
>
> 1.  ros packages can run in the apollo environment or in a non-apollo environment on the same domain controller:
>
>     1. (Recommended) If ros_bridge and the ros program are not in the same environment, DOMAIN_ID needs to be configured. Please set the following environment variable in the shell window of the ros project and in the window running ros_bridge, so that ros can communicate across environments:
>
>        export ROS_DOMAIN_ID=0
>
>     2. If ros_bridge and the ros program are both in the Apollo environment, you can directly copy the source code of the ros program to the ros_ws path of the Apollo project directory, and then use rosdep and colcon to install dependencies and compile

#### 2.1. Basic Driver Message Communication

Suppose a ROS project already exists on your domain controller. If your requirement is simply to connect the lidar driver, gnss driver, or localization algorithm in ros with the Apollo modules, the ros_bridge program already has the conversion logic for these basic messages built in, and you only need to modify the channels of both sides in the corresponding configuration files:

##### lidar Message Conversion Plugin

Configuration file:

```text
/apollo/cyber/ros_bridge/converters/common_plugins/pointcloud_msg_converter/conf/default.pb.txt
```

```text
name: "lidar"
apollo_channel_name_0: "/apollo/lidar/pointcloud2"
ros_topic_name_0: "/ros/lidar/pointcloud2"
```

Here, apollo_channel_name_0 refers to the channel from which apollo reads lidar messages, and ros_topic_name_0 refers to the topic to which the ros program sends lidar messages. After setting them, enter:

```bash
ros_bridge
```

to start the ros_bridge program. ros_bridge reads lidar messages from the ros topic "/ros/lidar/pointcloud2", converts them according to certain rules, and sends them to the apollo channel "/apollo/lidar/pointcloud2" for use by downstream Apollo modules

##### gnss Message Conversion Plugin

The lidar message conversion is a one-to-one relationship, while the gnss part is relatively complex and involves many-to-many conversion to complete the process of converting ros's gnss messages to apollo

Configuration file:

```text
/apollo/cyber/ros_bridge/converters/common_plugins/gnss_msg_converter/conf/heading_msg_fusion.pb.txt      # Message conversion plugin that fuses ros's nav_sat_fix and odometry and outputs Apollo Heading
/apollo/cyber/ros_bridge/converters/common_plugins/gnss_msg_converter/conf/imu_msg_converter.pb.txt       # Message conversion plugin that converts ros's imu and outputs Apollo imu
/apollo/cyber/ros_bridge/converters/common_plugins/gnss_msg_converter/conf/nav_msg_converter.pb.txt       # Message conversion plugin that converts ros's nav_sat_fix and outputs Apollo gnss best pose
/apollo/cyber/ros_bridge/converters/common_plugins/gnss_msg_converter/conf/odometry_msg_converter.pb.txt  # If ros's gnss driver does not output odometry itself, this plugin fuses imu and nav_sat_fix based on a Kalman filter and outputs ros odometry
/apollo/cyber/ros_bridge/converters/common_plugins/gnss_msg_converter/conf/odometry_parser.pb.txt         # Message conversion plugin that converts ros's odometry and outputs Apollo odometry
```

The channel configuration in the configuration files is similar to that of the lidar message conversion plugin and is not elaborated here

##### Localization Message Conversion Plugin

The localization message plugin converts the ros odometry message type into apollo's localization_estimate and transform. The localization output messages here must be messages in the world coordinate system, or coordinates that can correspond to the apollo map

#### 2.2. Developing Your Own Message Conversion Plugin

Besides the messages above, if you want to implement the conversion of other messages, you can implement a message conversion plugin yourself. For the specific logic, refer to the lidar message conversion plugin:

```text
.
├── BUILD
├── conf
│   └── default.pb.txt    # Configuration file
├── lidar_pointcloud.cc   # Implementation file of the conversion plugin
├── lidar_pointcloud.h    # Header file of the conversion plugin
└── plugins.xml           # Plugin description file
```

`lidar_pointcloud.h`

```cpp
#pragma once

#include <algorithm>
#include <memory>
#include <cstring>
#include <string>

// Include the necessary cyber header files
#include "cyber/cyber.h"
#include "cyber/plugin_manager/plugin_manager.h"
#include "cyber/ros_bridge/converter_base/convert_interface.h"
#include "modules/common_msgs/sensor_msgs/pointcloud.pb.h"

// Include the header file of the corresponding ros message
#include "sensor_msgs/point_cloud2_iterator.hpp"

// Define the input and output
using InputMsg = sensor_msgs::msg::PointCloud2;
using OutputMsg = apollo::drivers::PointCloud;

using InputMsgPtr = std::shared_ptr<InputMsg>;
using OutputMsgPtr = std::shared_ptr<OutputMsg>;


namespace apollo {
namespace cyber {

...

// Inherit from the apollo::cyber::RosApolloMessageConverter base class
class LidarPointcloud :
  public apollo::cyber::RosApolloMessageConverter<
    InputTypes<InputMsgPtr>, OutputTypes<OutputMsgPtr>> {
// apollo::cyber::RosApolloMessageConverter<InputTypes<InputMsgPtr>, OutputTypes<OutputMsgPtr>> refers to a 1-to-1 message conversion plugin. Currently apollo supports message conversion plugins of up to 4-to-4, for example:
// apollo::cyber::RosApolloMessageConverter<InputTypes<InputMsgPtr1, InputMsgPtr2>, OutputTypes<OutputMsgPtr>> is a message conversion plugin that converts two ROS messages into one Apollo message
// apollo::cyber::ApolloRosMessageConverter<InputTypes<InputMsgPtr1, InputMsgPtr2>, OutputTypes<OutputMsgPtr>> is a message conversion plugin that converts two Apollo messages into one ROS message
 public:
  LidarPointcloud() {}
  ~LidarPointcloud() {}

  ...

  // Define the ConvertMsg function
  virtual bool ConvertMsg(
    InputTypes<InputMsgPtr>&, OutputTypes<OutputMsgPtr>&);

    return value;
  }

  ...

};

// Register the plugin using the macro provided by cyber. The first parameter is the plugin class name, and the second parameter is fixed as apollo::cyber::MessageConverter
CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(
  apollo::cyber::LidarPointcloud, apollo::cyber::MessageConverter)

}  // namespace cyber
}  // namespace apollo
```

`lidar_pointcloud.cc`

```cpp
#include "cyber/ros_bridge/converters/common_plugins/pointcloud_msg_converter/lidar_pointcloud.h" // NOLINT

namespace apollo {
namespace cyber {

// Implement the message conversion logic
bool LidarPointcloud::ConvertMsg(
  InputTypes<InputMsgPtr>& in, OutputTypes<OutputMsgPtr>& out) {
  auto in_msg = std::get<0>(in.values);
  auto out_msg = std::get<0>(out.values);
  ...

  for (size_t i = 0; i < cloud_msg.height * cloud_msg.width; ++i) {
    auto pointcloud = out_msg -> add_point();
    pointcloud -> set_x(x);
    pointcloud -> set_y(y);
    pointcloud -> set_z(z);
    pointcloud -> set_intensity(static_cast<uint32_t>(std::round(intensity)));
    pointcloud -> set_timestamp(static_cast<uint64_t>(timestamp * 10e9));
  }

  return true;
}

}  // namespace cyber
}  // namespace apollo
```

`BUILD`

```python
load("//tools:cpplint.bzl", "cpplint")
load("//tools:apollo_package.bzl", "apollo_package", "apollo_plugin")
load("//tools/platform:build_defs.bzl", "if_aarch64", "if_gpu")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "runtime_files",
    srcs = glob([
        "conf/**",
        "data/**",
    ]),
)

apollo_plugin(
    name = "liblidar_pointcloud_plugin.so",
    srcs = ["lidar_pointcloud.cc"],
    hdrs = ["lidar_pointcloud.h"],
    description = ":plugins.xml",
    deps = [
        "//cyber",
        "//modules/common_msgs/sensor_msgs:pointcloud_proto",
        "//cyber/ros_bridge:converter_base",
        "//cyber/proto:simple_proto",
        "//cyber/ros_bridge/proto:converter_conf_proto",
        "@ros" # Indicates the dependency on ros
    ],
)

apollo_package()

cpplint()
```

`plugins.xml`

```xml
<library path="cyber/ros_bridge/converters/common_plugins/pointcloud_msg_converter/liblidar_pointcloud_plugin.so"> // Specifies the relative path of the plugin dynamic library
    <class type="apollo::cyber::LidarPointcloud" base_class="apollo::cyber::MessageConverter"></class> // Specifies the plugin class name; for the base class name, simply use apollo::cyber::MessageConverter
</library>
```

`default.pb.txt`

```text
name: "lidar"
apollo_channel_name_0: "/apollo/lidar/pointcloud2" # The channel corresponding to the first apollo message
ros_topic_name_0: "/ros/lidar/pointcloud2"         # The channel corresponding to the first ros message
```

After development is complete, you also need to specify that ros_bridge loads this new plugin:

```bash
vim /apollo/cyber/ros_bridge/conf/ros_bridge_conf.pb.txt
```

Suppose the new plugin type name is: SamplePlugin

```text
converter {
  type: "LidarPointcloud"
}

converter {
  type: "HeadingMsgFusion"
}

converter {
  type: "ImuMsgConverter"
}

converter {
  type: "NavMsgConverter"
}

converter {
  type: "OdometryMsgConverter"
}

# Add the following content
converter {
  type: "SamplePlugin"
}
```

After the modification, compile with buildtool build -p cyber, and then start ros_bridge normally
