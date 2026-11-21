## Experiment Content

Component is the base class provided by Cyber RT for building functional modules. There are two types: Component and TimerComponent. Component provides a message fusion mechanism that supports fusing up to 4 message channels. When reading data from multiple Channels, the first Channel is the primary Channel. When a message arrives on the primary Channel, Cyber RT calls the Component's Proc() once to process the data. In this experiment, you will write a Component message fusion example whose input messages are the data produced in the previous TimerComponent experiment and whose output is the fused data.

## Experience Link

https://apollo.baidu.com/community/course/42

## Experiment Objectives

- Gain a deep understanding of the characteristics of Component.

- Master the method and workflow for implementing a Component.

## Experiment Procedure

### 1. Preparation

This experiment builds on the previous TimerComponent experiment and uses its output data for data fusion. The source code of the previous experiment has been prepared for you. Please follow the steps below to get ready before the experiment:

1. Use the following command to extract the TimerComponent source archive

   ```bash
   tar -zxvf component.tar.gz
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_58eeec9.png)

2. Use the following command to compile the extracted TimerComponent experiment source code:

   ```bash
   buildtool build -p component/timer_component_sensor/
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_d222781.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_8c56f76.png)

3. Modify the `timer_component_sensor` configuration files to produce a new message channel:

1) Add the `timer_component_sensor_second.pb.txt` file. Copy the `timer_component_sensor.pb.txt` file, rename it to `timer_component_sensor_second.pb.txt`, and modify the configuration items in it.

```bash
name: "sensor_second"
sensor_topic:"/sensor/second"
```

Change the value of name to sensor_second and the value of sensor_topic to /sensor/second.

> Note: The timer_component_sensor_second.pb.txt file is located in the same folder as the timer_component_sensor.pb.txt file.

2) Add the `timer_component_sensor_second.dag` file. Copy the `timer_component_sensor.dag` file, rename it to `timer_component_sensor_second.dag`, and modify the configuration items in it.

```bash
module_config {
  module_library : "component/timer_component_sensor/libtimer_component_sensor_component.so"
  timer_components {
    class_name : "TimerComponentSensor"
    config {
      name: "timer_component_sensor_second"
      config_file_path:  "component/timer_component_sensor/conf/timer_component_sensor_second.pb.txt"
      flag_file_path:  "component/timer_component_sensor/conf/timer_component_sensor.conf"
      interval: 800
    }
  }
}
```

Change the name value in config to `timer_component_sensor_second`;

Change the config_file_path value to the path of the `timer_component_sensor_second.pb.txt` file;

Change the interval value to 800.

> Note: The `timer_component_sensor_second.dag` file is located in the same folder as the `timer_component_sensor.dag` file.

3) Add the `timer_component_sensor_second.launch` file. Copy the `timer_component_sensor.launch` file, rename it to `timer_component_sensor_second.launch`, and modify the configuration items in it.

```bash
<cyber>
  <module>
    <name>timer_component_sensor</name>
    <dag_conf>./component/timer_component_sensor/dag/timer_component_sensor_second.dag</dag_conf>
    <process_name>timer_component_sensor</process_name>
  </module>
</cyber>
```

Change dag_conf to the path of the `timer_component_sensor_second.dag` file.

> Note: The `timer_component_sensor_second.launch` file is located in the same folder as the `timer_component_sensor.launch` file.

4. Use the following command to run the `timer_component_sensor_second.launch` file.

   ```bash
   cyber_launch start component/timer_component_sensor/launch/timer_component_sensor_second.launch
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_2e0d265.png)

5. Use the cyber_monitor tool to verify whether the configuration has taken effect.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_bf22ba3.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_d2cf9b1.png)

   If the /sensor/second channel has data, the data frame rate is 1.25, and the data content matches the configuration, the configuration is successful.

6. Finally, in the terminal, press **Ctrl+C** to terminate the program.

### 2. Create a Component Template Instance

1. In the terminal, run the following command to generate the component_fusion_message template instance (package) under the component folder.

   ```bash
   buildtool create --template component component/component_fusion_message
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_d97f121.png)

2. Change to the generated `component_fusion_message` directory and use the tree command to view the directory structure of the generated component package.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_e5320f8.png)

   Among them, `BUILD`, `component_fusion_message.cc` and `component_fusion_message.h` are the source build rules file, source file and header file of the component_fusion_message package, respectively; under the conf directory, `component_fusion_message.conf` is the global variable configuration file and `component_fusion_message.pb.txt` is the configuration file for the configurable items the user defines in the `protobuffer` file; `cyberfile.xml` is the description file of the `component_fusion_message` package; the `component_fusion_message.dag` file in the dag directory describes the dependencies of the `component_fusion_message` package; `component_fusion_message.launch` in the launch folder is the launch file of the `component_fusion_message` package; BUILD in the proto folder is the build rules file for the `protobuffer` files the user defines in the proto folder, and in the `component_fusion_message.proto` file the user can define their own message structures.

### 3. Define the Component Message Structure

1. In `component_fusion_message.proto`, add the following message data structures.

   ```bash
   syntax = "proto2";

    package apollo;

    // message type of channel, just a placeholder for demo,
    // you should use `--channel_message_type` option to specify the real message type
    message ComponentFusionMessageMsg {
      optional string fusion_content = 1;
      optional uint64 fusion_msg_id = 2;
      optional uint64 timestamp = 3;

    }

    message ComponentFusionMessageConfig {
      optional string name = 1;
      optional string fusion_topic = 2;
    };
   ```

   In `ComponentFusionMessageMsg`, add three items, fusion_content, fusion_msg_id and timestamp, which represent the message content, the message number and the timestamp when the message was sent, respectively.

   In `ComponentFusionMessageConfig`, add the configurable items name and fusion_topic, which configure the name identifier of `ComponentFusionMessageConfig` and the channel for the output data.

2. In the BUILD file in the proto folder, add the build rules for the `protobuffer` file.

   ```bash
   load("//tools:apollo_package.bzl", "apollo_package")
    load("//tools/proto:proto.bzl", "proto_library")
    load("//tools:cpplint.bzl", "cpplint")

    package(default_visibility = ["//visibility:public"])

    proto_library(
        name = "component_fusion_message_proto",
        srcs = ["component_fusion_message.proto"],
    )

    apollo_package()

    cpplint()
   ```

   Since no new `protobuffer` file is added, the BUILD file does not need to be modified here. If you add a new `protobuffer` file, refer to the existing rules in the BUILD file and add the corresponding build rule for that `protobuffer` file.

### 4. Configure the Component Configuration Files

Since this experiment does not involve global variables, the component_fusion_message.conf file does not need to be modified here. In `component_fusion_message.pb.txt`, configure the items defined in the `proto` file.

```bash
name: "fusion_message"
fusion_topic: "/fusion/message"
```

Configure name in the `ComponentFusionMessageConfig` message defined in the `component_fusion_message.proto` file as fusion_message, and configure fusion_topic as /fusion/message.

### 5. Write the Component Source Files

1. Modify the `component_fusion_message.h` file.

   ```bash
   #pragma once
    #include <memory>

    #include "cyber/cyber.h"
    #include "cyber/component/component.h"
    #include "component/component_fusion_message/proto/component_fusion_message.pb.h"
    #include "component/timer_component_sensor/proto/timer_component_sensor.pb.h"

    using apollo::cyber::Time;
    using apollo::cyber::Writer;

    namespace apollo {

    class ComponentFusionMessage final
      : public cyber::Component<apollo::TimerComponentSensorMsg, apollo::TimerComponentSensorMsg> {
     public:
      bool Init() override;
      bool Proc(const std::shared_ptr<apollo::TimerComponentSensorMsg>& msg0, const std::shared_ptr<apollo::TimerComponentSensorMsg>& msg1) override;

     private:
      apollo::ComponentFusionMessageConfig config_;
      std::shared_ptr<Writer<ComponentFusionMessageMsg>> fusion_writer_ = nullptr;
    };

    CYBER_REGISTER_COMPONENT(ComponentFusionMessage)

    } // namespace apollo
   ```

   Since the message type that the component fuses is `TimerComponentSensorMsg`, you need to include the header file that defines `TimerComponentSensorMsg` (timer_component_sensor.pb.h).

   Use **using** to bring **Time** and **Writer** from the **apollo::cyber** namespace into the current scope;

   Modify the template parameters of the class that inherits from component; change the template parameters to the message types to be fused (two parameters).

   Modify the input parameters of the Proc function; change them to pointers to **TimerComponentSensorMsg**.

   Add a private member fusion*writer* to the ComponentFusionMessage class. fusion*writer* is a smart pointer to a Writer object, which can write out `ComponentFusionMessageMsg` data.

2. Modify the `component_fusion_message.cc` file.

   ```bash
   #include "component/component_fusion_message/component_fusion_message_component.h"

    namespace apollo {

    bool ComponentFusionMessage::Init() {

      ACHECK(ComponentBase::GetProtoConfig(&config_))
          << "failed to load component_fusion_message config file "
          << ComponentBase::ConfigFilePath();

      AINFO << "Load config succedded.\n" << config_.DebugString();

      fusion_writer_ = node_->CreateWriter<ComponentFusionMessageMsg>(config_.fusion_topic().c_str());

      AINFO << "Init ComponentFusionMessage succedded.";
      return true;
    }

    bool ComponentFusionMessage::Proc(const std::shared_ptr<apollo::TimerComponentSensorMsg>& msg0, const std::shared_ptr<apollo::TimerComponentSensorMsg>& msg1) {
      static int i = 0;
      auto out_msg = std::make_shared<ComponentFusionMessageMsg>();
      out_msg->set_fusion_msg_id(i++);

      std::string merged_content = msg0->content() + ":msg" + std::to_string(msg0->msg_id()) +
                                  " <---> " +
                                  msg1->content() + ":msg" + std::to_string(msg1->msg_id());

      out_msg->set_fusion_content(merged_content);
      out_msg->set_timestamp(Time::Now().ToNanosecond());
      fusion_writer_->Write(out_msg);

      return true;
    }

    } // namespace apollo
   ```

   Add initialization of the smart pointer fusion*write* in the Init function;

   Change the input parameters of the Proc function to match the definition in the header file;

   In the Proc function, create a `ComponentFusionMessageMsg` message and send it to the corresponding channel through fusion*writer*.

3. Modify the `BUILD` file.

   ```bash
    load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_cc_binary", "apollo_package", "apollo_component")
    load("//tools:cpplint.bzl", "cpplint")

    package(default_visibility = ["//visibility:public"])

    filegroup(
        name = "component_fusion_message_files",
        srcs = glob([
            "dag/**",
            "launch/**",
            "conf/**",
        ]),
    )

    apollo_component(
        name = "libcomponent_fusion_message_component.so",
        srcs = [
            "component_fusion_message_component.cc",
        ],
        hdrs = [
            "component_fusion_message_component.h",
        ],
        linkstatic = True,
        deps = [
            "//cyber",
            "//component/component_fusion_message/proto:component_fusion_message_proto",
            "//component/timer_component_sensor/proto:timer_component_sensor_proto",

        ],
    )

    apollo_package()

    cpplint()
   ```

   Since the source file includes `timer_component_sensor.pb.h`, the corresponding dependency must be added to the build dependencies.

### 6. Compile the Component Package

In the terminal, run the following command to compile the `component_fusion_message` package.

```bash
buildtool build -p  component/component_fusion_message/
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_f489afb.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_5187f81.png)

The build succeeds without errors. If the build reports errors, modify the code according to the error messages and recompile until it succeeds.

### 7. Modify the Component dag File

In the `component_fusion_message.dag` file, change the channel value in the first readers entry to the primary channel fused in the component, add the other readers to be fused below the primary channel's readers, and configure the correct channels.

```bash
module_config {
  module_library : "component/component_fusion_message/libcomponent_fusion_message_component.so"
  components {
    class_name : "ComponentFusionMessage"
    config {
      name : "component_fusion_message"
      config_file_path: "component/component_fusion_message/conf/component_fusion_message.pb.txt"
      flag_file_path: "component/component_fusion_message/conf/component_fusion_message.conf"
      readers {
        channel: "/sensor/first"
      }
      readers {
        channel: "/sensor/second"
      }
    }
  }
}
```

Here the channel value in readers is changed to /sensor/first, a new readers entry is added below it, and its channel value is configured as /sensor/second.

### 8. Modify the Component launch File

```bash
<cyber>
  <module>
    <name>component_fusion_message</name>
    <dag_conf>./component/component_fusion_message/dag/component_fusion_message.dag</dag_conf>
    <process_name>component_fusion_message</process_name>
  </module>
</cyber>
```

Here "./" is added before the dag file path inside the <dag_conf> tag. Because cyber is currently tightly coupled with apollo, after compilation the system copies the build outputs and configuration files into the corresponding apollo directories, and when executing files it gives priority to the files in the apollo directory, so the configuration here would not take effect. Adding "./" tells the system to use the dag file here to run the component.

### 9. Run the Component

1. Use two terminals to start the two TimerComponents separately to produce two message channels.

   Produce the /sensor/first messages

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3f455b3.png)

   Produce the /sensor/second messages

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_4d1d207.png)

2. Open a new terminal and start `component_fusion_message` in it to fuse the two message channels.

   After /sensor/first and /sensor/second are fused, the /fusion/message messages are produced.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_81ebe14.png)

### 10. View the Component Results

1. Use the cyber_monitor tool to view the results of `component_fusion_message`.

   You can see that the frame rate of /fusion/message is the same as that of the primary channel message /sensor/first.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_1774b35.png)

2. Use the cyber_channel tool to print the /fusion/message messages.

   You can see that the msg_id of the primary channel's message changes while that of the other channel's message does not, which also verifies that the component is triggered by the primary channel's messages.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ed1ff48.png)

### 11. Start Multiple Components with One launch File

Copy the `component_fusion_message.launch` file and rename it to `component_fusion_message_all.launch`, then copy the <module> tag contents of the `timer_component_sensor.launch` file and the `timer_component_sensor_second.launch` file into the <cyber> tag of `component_fusion_message_all.launch`. In this way, you only need to start this single `component_fusion_message_all.launch` file to start all 3 components at once.

```bash
<cyber>
  <module>
    <name>timer_component_sensor</name>
    <dag_conf>./component/timer_component_sensor/dag/timer_component_sensor.dag</dag_conf>
    <process_name>timer_component_sensor</process_name>
  </module>

  <module>
    <name>timer_component_sensor</name>
    <dag_conf>./component/timer_component_sensor/dag/timer_component_sensor_second.dag</dag_conf>
    <process_name>timer_component_sensor</process_name>
  </module>

  <module>
    <name>component_fusion_message</name>
    <dag_conf>./component/component_fusion_message/dag/component_fusion_message.dag</dag_conf>
    <process_name>component_fusion_message</process_name>
  </module>
</cyber>
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_fb11746.png)

Well, this concludes the experiment.
