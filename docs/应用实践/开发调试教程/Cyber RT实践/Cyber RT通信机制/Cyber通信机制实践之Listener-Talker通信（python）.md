## Experiment Overview

Build a simple communication flow: the sender creates a Writer through a Cyber Node, the receiver creates a Reader through a Cyber Node, the sender sends the vehicle speed in real time, and the receiver prints the received vehicle speed.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_9d42b48.png)

## Experience Link

https://apollo.baidu.com/community/course/38

## Experiment Objectives

Through Cyber's publish-subscribe mechanism, implement one node that publishes messages and another node that subscribes to and receives them, in order to learn the basics of cyber communication.

## Prerequisites

This experiment requires some understanding of python code.

## Practice Procedure

### 1. Write the Code

1. Create the code directory structure.
   
   ```bash
    buildtool create --template component communication
    touch /apollo_workspace/communication/listener.py
    touch /apollo_workspace/communication/talker.py
   ```
   
   Check that the directory structure is correct:
   
   ```bash
   communication
    ├── BUILD <required>
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
    ├── listener.py <required>
    ├── proto <required>
    │   ├── BUILD <required>
    │   └── communication.proto <required>
    └── talker.py <required>
   ```

2. Define the data structure of the communication message by writing the proto/communication.proto file with the following content:
   
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

3. Write the sender talker code. The talker.py code is as follows:
   
   ```bash
   import time
    from cyber.python.cyber_py3 import cyber
    from communication.proto.communication_pb2 import Car
   
    def main():
        # Initialize Cyber
        cyber.init()
   
        # Create the talker node
        talker_node = cyber.Node("talker")
   
        # Create a Topic from the node to send the vehicle speed data
        talker = talker_node.create_writer("/car_speed", Car)
        print("I'll start telling you the current speed of the car.")
   
        # Set the initial speed to 0, then increase the speed by 5 km/h every second
        speed = 0
        while not cyber.is_shutdown():
            msg = Car()
            msg.speed = speed
            # Assume the vehicle speed keeps increasing
            speed += 5
            talker.write(msg)
            time.sleep(1)
   
    if __name__ == '__main__':
        main()
   ```

4. Write the receiver listener code. The listener.py code is as follows:
   
   ```bash
   import time
   from cyber.python.cyber_py3 import cyber
   from communication.proto.communication_pb2 import Car
   
   # Callback function invoked after a message is received
   def message_callback(msg):
       print("now speed is:", msg.speed)
   
   def main():
       # Initialize Cyber
       cyber.init()
   
       # Create the listener node
       listener_node = cyber.Node("listener")
   
       # Create a message listener and specify the callback function
       listener = listener_node.create_reader("/car_speed", Car, message_callback)
   
       while not cyber.is_shutdown():
           time.sleep(0.2)
   
   if __name__ == '__main__':
       main()
   ```

5. Change `communication/BUILD` to the following:
   
   ```bash
    load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_cc_binary", "apollo_package", "apollo_component")
    load("//tools:cpplint.bzl", "cpplint")
   
    package(default_visibility = ["//visibility:public"])
   
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

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_1b9204c.png)

### 3. Run the Communication Programs

#### 1. Run the talker program

Open a new terminal and run the following commands.

```bash
//Output results to the console
export GLOG_alsologtostderr=1
//Run talker
python communication/talker.py
```

#### 2. Run the listener program

Open another new terminal and run the following commands:

```bash
//Output results to the console
export GLOG_alsologtostderr=1
//Run listener
python communication/listener.py
```

### 4. Results

The talker keeps sending real-time speed information to the listener.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_dc88dab.png)

After receiving it, the listener prints the speed to the console:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_6ac4c0f.png)
