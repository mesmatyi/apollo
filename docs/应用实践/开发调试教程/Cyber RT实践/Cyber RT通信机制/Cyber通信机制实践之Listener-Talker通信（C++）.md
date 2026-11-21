## Experiment Overview

Build a simple communication flow: the sender creates a Writer through a Cyber Node, the receiver creates a Reader through a Cyber Node, the sender sends the vehicle speed in real time, and the receiver prints the received vehicle speed.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_9baa590.png)

## Experience Link

https://apollo.baidu.com/community/course/37

## Experiment Objectives

Through Cyber's publish-subscribe mechanism, implement one node that publishes messages and another node that subscribes to and receives them, in order to learn the basics of cyber communication.

## Prerequisites

This experiment requires some understanding of C++ code.

## Practice Procedure

### 1. Write the Code

1. Create the code directory structure.
   
   ```bash
   buildtool create --template component communication
   touch /apollo_workspace/communication/listener.cc
   touch /apollo_workspace/communication/talker.cc
   ```
   
   Check that the directory structure is correct.
   
   Enter the command:
   
   ```bash
   tree communication/
   ```
   
   Directory structure:
   
   ```bash
   communication
    ├── BUILD
    ├── communication.cc
    ├── communication.h
    ├── conf
    │   ├── communication.conf
    │   └── communication.pb.txt
    ├── cyberfile.xml
    ├── dag
    │   └── communication.dag
    ├── launch
    │   └── communication.launch
    ├── listener.cc <required>
    ├── proto <required>
    │   ├── BUILD <required>
    │   └── communication.proto <required>
    └── talker.cc <required>
   ```

2. Define the data structure of the communication message by writing the `proto/communication.proto` file with the following content:
   
   ```bash
   syntax = "proto2";
   
   package apollo.communication.proto;
   
    // Define a car message: car model, owner, license plate number, kilometers driven, and speed
    message Car{
        optional string plate = 1;
        optional string type = 2;
        optional string owner = 3;
        optional uint64 kilometers = 4;
        optional uint64 speed = 5;
    };
   ```

3. Write the sender talker code. The talker.cc code is as follows:
   
   ```bash
   #include "communication/proto/communication.pb.h"
    #include "cyber/cyber.h"
    #include "cyber/time/rate.h"
   
    // Reference to the car data definition; as you can see, its definition comes from a proto
    using apollo::communication::proto::Car;
   
    int main(int argc, char *argv[]) {
      // Initialize the cyber framework
      apollo::cyber::Init(argv[0]);
      // Create the talker node
      auto talker_node = apollo::cyber::CreateNode("talker");
      // Create a Topic from the node to publish the vehicle speed
      auto talker = talker_node->CreateWriter<Car>("car_speed");
      AINFO << "I'll start telling you the current speed of the car.";
   
      // Set the initial speed to 0, then increase the speed by 5 km/h every second
      uint64_t speed = 0;
      while (apollo::cyber::OK()) {
          auto msg = std::make_shared<Car>();
          msg->set_speed(speed);
          // Assume the vehicle speed keeps increasing
          speed += 5;
          talker->Write(msg);
          sleep(1);
      }
      return 0;
    }
   ```

4. Write the receiver listener code. The listener.cc code is as follows:
   
   ```bash
   #include "communication/proto/communication.pb.h"
    #include "cyber/cyber.h"
   
    using apollo::communication::proto::Car;
   
    // Callback function invoked after a message is received
    void message_callback(
            const std::shared_ptr<Car>& msg) {
        AINFO << "now speed is: " << msg->speed();
    }
   
    int main(int argc, char* argv[]) {
        // Initialize the cyber framework
        apollo::cyber::Init(argv[0]);
        // Create the listener node
        auto listener_node = apollo::cyber::CreateNode("listener");
        // Create a listener reader to read messages
        auto listener = listener_node->CreateReader<Car>(
                "car_speed", message_callback);
        apollo::cyber::WaitForShutdown();
        return 0;
    }
   ```

5. Modify the BUILD file to add the newly written code to the build. Modify the `communication/BUILD` file as follows:
   
   ```bash
       load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_cc_binary", "apollo_package", "apollo_component")
    load("//tools:cpplint.bzl", "cpplint")
   
    package(default_visibility = ["//visibility:public"])
   
    apollo_cc_binary(
        name = "talker",
        srcs = ["talker.cc"],
        deps = [
            "//cyber",
            "//communication/proto:communication_proto",
        ],
        linkstatic = True,
    )
    apollo_cc_binary(
        name = "listener",
        srcs = ["listener.cc"],
        deps = [
            "//cyber",
            "//communication/proto:communication_proto",
        ],
        linkstatic = True,
    )
   
    apollo_package()
   
    cpplint()
   ```

### 2. Compile the Program

```bash
//Go back to the /apollo_workspace directory to compile
cd /apollo_workspace
buildtool build -p communication
```

After a successful build, the following information is displayed:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ff7ffc5.png)

### 3. Run the Communication Programs

#### 1. Run the talker program

Open a new terminal and run the following commands.

```bash
// Output results to the console
export GLOG_alsologtostderr=1
// The executable generated by the build is in /opt/apollo/neo/bin/
cd /opt/apollo/neo/bin/
// Run talker
./talker
```

#### 2. Run the listener program

Open another new terminal and run the following commands.

```bash
// Output results to the console
export GLOG_alsologtostderr=1
// The executable generated by the build is in /opt/apollo/neo/bin/
cd /opt/apollo/neo/bin/
// Run listener
./listener
```

### 4. Results

The talker keeps sending real-time speed information to the listener.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_151fd6c.png)

After receiving it, the listener prints the speed to the console.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a5150d7.png)
