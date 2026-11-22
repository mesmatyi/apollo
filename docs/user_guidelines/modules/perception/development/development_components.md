# Component Development

Component development means developing an independent component to implement an independent function. For example, to add a semantic segmentation model to point cloud perception, you can develop an independent component to implement it.

## Create a Folder

Create a folder named after the component. In the example below, pointcloud_ground_detection, pointcloud_map_based_roi, and pointcloud_preprocess each represent one component.

```bash
├── perception
    ├── pointcloud_ground_detection  // component folder
    ├── pointcloud_map_based_roi
    ├── pointcloud_preprocess
```

## Component Code

First, create the necessary code for the component, as shown below.

```bash
├── component_name
    ├── component_name.cc  // component definition and entry point
    ├── component_name.h
    └── BUILD
```

Then, define the component class interface according to the component's input and output data.

> Note: If the received data type is a custom class or struct, the new component must be started in the same process as the upstream and downstream components for messages to be sent and received properly. If the received message is a data type defined by proto, the upstream and downstream components can be started in different processes and still receive messages properly.

Finally, implement the Init method to initialize and load parameters, and implement the Proc function to implement the component's function.

```bash
class ComponentName
    : public cyber::Component<LidarFrameMessage> {
 public:
  ComponentName() = default;
  virtual ~ComponentName() = default;
  /**
   * @brief Init component
   *
   * @return true
   * @return false
   */
  bool Init() override;
  /**
   * @brief Process of component
   *
   * @param lidar frame message
   * @return true
   * @return false
   */
  bool Proc(const std::shared_ptr<LidarFrameMessage>& message) override;
 };
```

## Configuration File

In the proto folder, define the name of the channel the component sends messages to, and the configuration of the functions the component needs to perform. The code structure is as follows:

```bash
├── component_name
    ├── conf               // component configuration files
    ├── proto              // proto definitions
    ├── component_name.cc
    ├── component_name.h
    └── BUILD
```

A typical configuration definition example is as follows. The component's configuration files are placed in conf.

```bash
message LidarDetectionComponentConfig {
  optional string output_channel_name = 1;  // output channel name
  optional PluginParam plugin_param = 2;    // function configuration, such as ground point segmentation, preprocessing, etc.
}
// PluginParam definition. The PluginParam here is the configuration of a specific algorithm or function. Do not confuse it with a "plugin".
// message PluginParam {
//   optional string name = 1;  // function name
//   optional string config_path = 2;  // path of the function configuration
//   optional string config_file = 3;  // file of the function configuration, containing the specific parameters
// }
```

## Function Definition

Declare the function's base class in the component, which is used to call the function. The base class is defined in interface, and the function code is defined in the function folder.

```bash
├── component_name
    ├── conf
    ├── proto
    ├── data               // function configuration parameters
    ├── function           // actual function definitions of the component
        ├── function.h
        ├── function.cc
    ├── interface          // function interfaces
        ├── base_function.h
        ├── base_function.cc
    ├── component_name.cc
    ├── component_name.h
    └── BUILD
```

## Using the Component

Once the component's code structure and functions are defined, the next step is to use the component. Configure the component's startup items through a dag file, and start the component through launch.

```bash
├── component_name
    ├── conf
    ├── dag     // dag file
    ├── launch  // launch startup file
    ├── proto
    ├── data
    ├── function
    ├── interface
    ├── component_name.cc
    ├── component_name.h
    └── BUILD
```

The dag file is defined as follows:

```bash
module_config {
  module_library : "modules/perception/component_name/libcomponent_name.so"
  components {
    class_name : "ComponentClassName"
    config {
      name : "ComponentName"
      # Define the type and name of the output message in the conf file, and define the configuration path of the specific function (i.e. plugin_param)
      config_file_path : "/apollo/modules/perception/component_name/conf/component_name.pb.txt"
      flag_file_path: "/apollo/modules/perception/data/flag/perception_common.flag"
      readers {
        channel: "/perception/reader_name"  # name of the upstream component's output channel
      }
    }
  }
}
```

The launch file usually specifies the dag to be started, as shown below:

```bash
<cyber>
    <desc>cyber modules list config</desc>
    <version>1.0.0</version>
    <module>
        <name>component_name</name>
        <dag_conf>/apollo/modules/perception/component_name/dag/component_name.dag</dag_conf>
        <!-- if not set, use default process -->
        <process_name>component_name</process_name>
        <version>1.0.0</version>
    </module>
</cyber>
```

When debugging the function of a single component, that component's launch is started. To start the whole perception, all dags that need to be started must be defined in the launch, and the upstream and downstream channels of each dag must correspond one to one.

Command to start a launch:

```bash
cyber_launch launch_file_name
```
