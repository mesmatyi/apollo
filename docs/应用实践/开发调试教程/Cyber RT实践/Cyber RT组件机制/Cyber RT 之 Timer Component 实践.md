## Experiment Content

Component is the base class provided by Cyber RT for building functional modules. There are two types: Component and TimerComponent. Compared with Component, TimerComponent provides no message fusion and is not triggered by messages; instead it is called periodically by the system and can be used to perform routine tasks. In this experiment, you will write a TimerComponent example that simulates a sensor producing data.

## Experience Link

https://apollo.baidu.com/community/course/36

## Experiment Objectives

- Gain a deep understanding of the characteristics of TimerComponent.

- Master the method and workflow for implementing a TimerComponent.

## Experiment Procedure

### 1. Create a TimerComponent Template Instance

1. In the terminal, run the following command to generate the timer_component_sensor template instance (package) under the component folder.

   ```bash
   buildtool create --template timer_component component/timer_component_sensor
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3bc1b05.png)

2. Use the cd command to change to the generated `timer_component_sensor` directory, and use the tree command to view the directory structure of the generated component package.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_b89bc55.png)

   Among them, the BUILD file is the build rules file for the TimerComponent source code; under the conf directory, `timer_component_sensor.conf` is the global variable configuration file and `timer_component_sensor.pb.txt` is the configuration file for the configurable items the user defines in the proto file; `cyberfile.xml` is the description file of the `timer_component_sensor` package; the `timer_component_sensor.dag` file in the dag directory describes the dependencies of the `timer_component_sensor` package; `timer_component_sensor.launch` in the launch folder is the launch file of the `timer_component_sensor` package; BUILD in the proto folder is the build rules file for the protobuffer files the user defines in the proto folder, and in the `timer_component_sensor.proto` file the user can define their own message structures; `timer_component_sensor.cc` and `timer_component_sensor.h` are the source file and header file of the TimerComponent.

### 2. Define the TimerComponent Message Structure

1. In `timer_component_sensor.proto`, add the following message data structures.

   ```bash
   syntax = "proto2";

    package apollo;

    // message type of channel, just a placeholder for demo,
    // you should use `--channel_message_type` option to specify the real message type
    message TimerComponentSensorMsg {
      optional string content = 1;
      optional uint64 msg_id = 2;
      optional uint64 timestamp = 3;

    }

    message TimerComponentSensorConfig {
      optional string name = 1;
      optional string sensor_topic = 2;
    };
   ```

   In **TimerComponentSensorMsg**, add three items, content, msg_id and timestamp, which represent the message content, the message number and the timestamp when the message was sent, respectively.

   In **TimerComponentSensorConfig**, add the configurable items name and sensor_topic, which configure the name of `timer_component_sensor` and the channel for the output data.

2. In the BUILD file in the proto folder, add the build rules for the protobuffer file.

   ```bash
       load("//tools:apollo_package.bzl", "apollo_package")
    load("//tools/proto:proto.bzl", "proto_library")
    load("//tools:cpplint.bzl", "cpplint")

    package(default_visibility = ["//visibility:public"])

    proto_library(
        name = "timer_component_sensor_proto",
        srcs = ["timer_component_sensor.proto"],
    )

    apollo_package()

    cpplint()
   ```

   Since no new proto file is added, the BUILD file does not need to be modified here. If you add a new proto file, refer to the existing rules in the BUILD file and add the corresponding build rule for that proto file.

### 3. Configure the TimerComponent Configuration Files

Since this experiment does not involve global variables, the `timer_component_sensor.conf` file does not need to be modified here. In `timer_component_sensor.pb.txt`, configure the items defined in the `timer_component_sensor.proto` file.

```bash
name: "sensor_first"
sensor_topic:"/sensor/first"
```

Configure name in the **TimerComponentSensorConfig** message defined in the `timer_component_sensor.proto` file as `sensor_first`, and configure `sensor_topic` as `/sensor/first`.

### 4. Write the TimerComponent Source Files

1.  Modify the `timer_component_sensor.h` file:

    ```bash
    #pragma once
     #include <memory>

     #include "cyber/common/macros.h"
     #include "cyber/component/component.h"
     #include "cyber/component/timer_component.h"
     #include "cyber/cyber.h"
     #include "component/timer_component_sensor/proto/timer_component_sensor.pb.h"

     using apollo::cyber::Time;
     using apollo::cyber::Writer;

     namespace apollo {

     class TimerComponentSensor final
       : public apollo::cyber::TimerComponent {
      public:
       bool Init() override;
       bool Proc() override;

      private:
       apollo::TimerComponentSensorConfig config_;
       std::shared_ptr<Writer<TimerComponentSensorMsg>> sensor_writer_ = nullptr;
     };

     CYBER_REGISTER_COMPONENT(TimerComponentSensor)

     } // namespace apollo
    ```

        In the `timer_component_sensor.h` file, use **using** to bring **Time** and **Writer** from the **apollo::cyber** namespace into the current scope, and add a private member **sensor_writer_** to the **TimerComponentSensor** class. **sensor_writer_** is a smart pointer to a **Writer** object, which can write out **TimerComponentSensorMsg** data.

2.  Modify the `timer_component_sensor.cc` file.

    ```bash
    #include "component/timer_component_sensor/timer_component_sensor_component.h"

     namespace apollo {

     bool TimerComponentSensor::Init() {

       ACHECK(ComponentBase::GetProtoConfig(&config_))
           << "failed to load timer_component_sensor config file "
           << ComponentBase::ConfigFilePath();

       AINFO << "Load config succedded.\n" << config_.DebugString();
       // TODO: add your own init code here.
       sensor_writer_ = node_->CreateWriter<TimerComponentSensorMsg>(config_.sensor_topic().c_str());

       AINFO << "Init TimerComponentSensor succedded.";
       return true;
     }

     bool TimerComponentSensor::Proc() {
       AINFO << "Proc TimerComponentSensor triggered.";

       // TODO: add your own proc code here.
       static int i = 0;
       auto out_msg = std::make_shared<TimerComponentSensorMsg>();
       out_msg->set_msg_id(i++);
       out_msg->set_content(config_.name());
       out_msg->set_timestamp(Time::Now().ToNanosecond());
       sensor_writer_->Write(out_msg);

       return true;
     }

     } // namespace apollo
    ```

    In the Init function, add initialization of the smart pointer **sensor_writer_**,

    In the Proc function, create a **TimerComponentSensorMsg** message and send it to the corresponding channel through **sensor_writer_**.

3.  Modify the BUILD file.

    ```bash
        load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_cc_binary", "apollo_package", "apollo_component")
     load("//tools:cpplint.bzl", "cpplint")

     package(default_visibility = ["//visibility:public"])

     filegroup(
         name = "timer_component_sensor_files",
         srcs = glob([
             "dag/**",
             "launch/**",
             "conf/**",
         ]),
     )

     apollo_component(
         name = "libtimer_component_sensor_component.so",
         srcs = [
             "timer_component_sensor_component.cc",
         ],
         hdrs = [
             "timer_component_sensor_component.h",
         ],
         linkstatic = True,
         deps = [
             "//cyber",
             "//component/timer_component_sensor/proto:timer_component_sensor_proto",

         ],
     )

     apollo_package()

     cpplint()
    ```

    Since no other proto files or header files are included, the BUILD file does not need to be modified here.

### 5. Compile the TimerComponent Package

In the terminal, run the following command to compile the **timer_component_sensor** package.

```bash
buildtool build -p  component/timer_component_sensor
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_6d9a3ac.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_acc4b93.png)

The build succeeds without errors. If the build reports errors, modify the code according to the error messages and recompile until it succeeds.

### 6. Modify the TimerComponent dag File

In the `timer_component_sensor.dag` file, change the value of the interval item to 500, i.e. run once every 500 ms. The interval value is the running time interval of the TimerComponent.

```bash
module_config {
  module_library : "component/timer_component_sensor/libtimer_component_sensor_component.so"
  timer_components {
    class_name : "TimerComponentSensor"
    config {
      name: "timer_component_sensor"
      config_file_path:  "component/timer_component_sensor/conf/timer_component_sensor.pb.txt"
      flag_file_path:  "component/timer_component_sensor/conf/timer_component_sensor.conf"
      interval: 500
    }
  }
}
```

### 7. Modify the TimerComponent launch File

Add "./" before the dag file path inside the <dag_conf> tag. Because cyber is currently tightly coupled with apollo, after compilation the system copies the build outputs and configuration files into the corresponding apollo directories, and when executing files it gives priority to the files in the apollo directory, so the configuration here would not take effect. Adding "./" tells the system to use the dag file here to run the component.

```bash
<cyber>
  <module>
    <name>timer_component_sensor</name>
    <dag_conf>./component/timer_component_sensor/dag/timer_component_sensor.dag</dag_conf>
    <process_name>timer_component_sensor</process_name>
  </module>
</cyber>
```

### 8. Run the TimerComponent

#### Method 1: Use mainboard to run the timer_component_sensor.dag file

In the terminal, enter the following command to use the mainboard tool to run the `timer_component_sensor.dag` file and run the `timer_component_sensor` package.

```bash
mainboard -d ./component/timer_component_sensor/dag/timer_component_sensor.dag
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_715e7a7.png)

> Note: Do not forget the "./" when executing the command.
> Reason: Because cyber is currently tightly coupled with apollo, after compilation the system copies the build outputs and configuration files into the corresponding apollo directories, and when executing files it gives priority to the files in the apollo directory, so the configuration here would not take effect. Adding "./" tells the system to use the dag file here to run the component.

#### Method 2: Use cyber_launch to run the timer_component_sensor.launch file

In the terminal, you can also use the cyber_launch tool to run the `timer_component_sensor.launch` file and run the `timer_component_sensor` package.

```bash
cyber_launch start component/timer_component_sensor/launch/timer_component_sensor.launch
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_cb25079.png)

### 9. View the TimerComponent Results

#### Method 1: Use the cyber_monitor tool to view the timer_component_sensor results.

Open a new terminal window and run the cyber_monitor command to start the cyber_monitor tool.

```bash
cyber_monitor
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_08e4142.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_6f6ff13.png)

In the cyber_monitor tool, you can see a channel named `/sensor/first` with a frame rate of 2 Hz. This message is the data sent by the `timer_component_sensor` package. Press the **Right arrow** key on the keyboard to enter this channel and view the message content.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a60877d.png)

In the `/sensor/first` channel, you can see the message type, frame rate, message data size and the specific message content.

#### Method 2: Use the cyber_channel tool to view the timer_component_sensor results.

In the terminal, you can also use the cyber_channel command to view the output messages.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_dab2f2c.png)

In the figure above, the `cyber_channel list` command is used to list the existing channels; `cyber_channel hz /sensor/first` is used to view the running time interval; and the `cyber_channel echo /sensor/first` command is used to print the message content. For other cyber_channel commands, use `cyber_channel -h` to see the usage instructions.

Well, this concludes the experiment.
