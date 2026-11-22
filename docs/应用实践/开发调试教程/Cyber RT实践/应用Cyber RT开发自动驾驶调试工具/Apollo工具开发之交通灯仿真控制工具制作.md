## Experiment Content

This experiment aims to use Apollo Cyber RT to develop a traffic light simulation control tool, with which you can control the state of the traffic lights.

## Experience Link

https://apollo.baidu.com/community/course/39

## Experiment Objectives

Through this experiment, learn how to use Apollo Cyber RT to develop a traffic light simulation control tool and use it to control the traffic light state.

- Create a cyber project,
- Become familiar with building cyber tools.

## Practice Steps

### Step 1: Start the Simulation Environment

1. In the terminal, run the DreamView+ startup command. After it succeeds, click the dreamview+ button in the menu bar to enter the dreamview+ interface.

   ```bash
   aem bootstrap start --plus
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_372016d.png)

   The following screen indicates that dreamview+ has started successfully.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_bf50bff.png)

2. Open the **Mode Settings** panel in the left navigation bar, select **PnC** Mode as the mode and **Sim_Control** as the operation to enter simulation mode.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3f6490d.png)

3. Select the map.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c93b265.png)

4. Start the Planning module from the **Modules** list on the left.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_badef52.png)

### Step 2: Edit the Code to Implement the Task

#### Prerequisites

Create a file named `cyber_trafficlight.py` in the `/apollo_workspace` directory.

```bash
touch cyber_trafficlight.py
```

1. Import the necessary libraries and modules.

   First, import the required Python libraries and Apollo Cyber RT modules. These libraries and modules help us initialize the Cyber framework and use the Cyber RT tools.

   ```bash
   import argparse
    from cyber.python.cyber_py3 import cyber
    from cyber.python.cyber_py3 import cyber_time
    import modules.common_msgs.perception_msgs.traffic_light_detection_pb2 as traffic_light_detection_pb2
    import threading
    import time
   ```

2. Define the related functions.

   In this step, we define several functions that implement the different features of the script.

   1) The function that adds traffic light information.

   ```bash
   def add_traffic_light(color, traffic_light_pb):
    light = traffic_light_pb.traffic_light.add()
    light.color = color
    light.id = "TRAFFIC_LIGHT_ID"  # Must be set by yourself; this is the unique identifier of the traffic light
    light.tracking_time = 10.0  # Set the tracking time, in seconds
   ```

   2) The function that adds message header information.

   ```bash
   seq_num = 0

   def add_header(msg):
    global seq_num
    msg.header.sequence_num = seq_num
    msg.header.timestamp_sec = cyber_time.Time.now().to_sec()
    msg.header.module_name = "manual_traffic_light"
    seq_num += 1  # Increment the sequence number
   ```

   3) The function that publishes the traffic light message.

   ```bash
   def publish_traffic_light(writer, traffic_light_msg):
    while not cyber.is_shutdown():
        add_header(traffic_light_msg)  # Add message header information
        writer.write(traffic_light_msg)  # Publish the traffic light message
        time.sleep(0.1)  # Control the message publishing rate
   ```

3. Initialize the Cyber framework and related objects.

   In the main function, we need to initialize the Cyber framework and related objects in order to use the Cyber RT tools.

   ```bash
   if __name__ == '__main__':
    # Initialize the traffic light detection message object
    traffic_light_msg = traffic_light_detection_pb2.TrafficLightDetection()

    # Initialize the Cyber framework
    cyber.init()
    node = cyber.Node("traffic_light_command")

    # Create the message writer
    writer = node.create_writer(
        "/apollo/perception/traffic_light", traffic_light_detection_pb2.TrafficLightDetection)

    # Create the thread that publishes traffic light messages
    thread = threading.Thread(target=publish_traffic_light, args=(writer, traffic_light_msg))
    thread.start()
   ```

4. Enter the user input loop.

   In an infinite loop, wait for user input. The user can enter different commands to perform the corresponding operations.

   ```bash
   while not cyber.is_shutdown():
        user_input = input(
            "1: Set traffic light to red 2: Set traffic light to green 3: Exit\n")
        traffic_light_msg.ClearField('traffic_light')  # Clear the traffic light information
        if user_input == '1':
            # User entered 1: set the traffic light to red
            add_traffic_light(
                traffic_light_detection_pb2.TrafficLight.RED, traffic_light_msg)
        elif user_input == "2":
            # User entered 2: set the traffic light to green
            add_traffic_light(
                traffic_light_detection_pb2.TrafficLight.GREEN, traffic_light_msg)
        elif user_input == "3":
            # User entered 3: exit the loop
            break

    # Shut down the Cyber framework
    cyber.shutdown()
   ```

   Complete code:

   ```bash
   from cyber.python.cyber_py3 import cyber
   from cyber.python.cyber_py3 import cyber_time
   import modules.common_msgs.perception_msgs.traffic_light_detection_pb2 as traffic_light_detection_pb2
   import threading
   import time

   def add_forward_light(color, traffic_light_pb):
       light = traffic_light_pb.traffic_light.add()
       light.color = color
       light.id = "TRAFFIC_LIGHT_ID" # Must be set by yourself
       light.tracking_time = 10.0

   seq_num = 0

   def add_header(msg):
       global seq_num
       msg.header.sequence_num = seq_num
       msg.header.timestamp_sec = cyber_time.Time.now().to_sec()
       msg.header.module_name = "manual_traffic_light"
       seq_num = seq_num + 1

   def pub_func(writer):
       while not cyber.is_shutdown():
           global traffic_light_msg
           add_header(traffic_light_msg)
           writer.write(traffic_light_msg)
           time.sleep(0.1)

   traffic_light_msg = None

   if __name__ == '__main__':
       traffic_light_msg = traffic_light_detection_pb2.TrafficLightDetection()
       cyber.init()
       node = cyber.Node("traffic_light_command")
       writer = node.create_writer(
           "/apollo/perception/traffic_light", traffic_light_detection_pb2.TrafficLightDetection)
       thread = threading.Thread(target=pub_func, args=(writer,))
       thread.start()
       while not cyber.is_shutdown():
           m = input(
               "1: forward red 2: forward green\n")
           traffic_light_msg.ClearField('traffic_light')
           print(m)
           if m == '1':
               add_forward_light(
                   traffic_light_detection_pb2.TrafficLight.RED, traffic_light_msg)
           elif m == "2":
               add_forward_light(
                   traffic_light_detection_pb2.TrafficLight.GREEN, traffic_light_msg)

       cyber.shutdown()
   ```

### Step 3: Using the Route Setting Tool

1. Open Dreamview+ routing editing.

   Select the start point and end point on the map and save.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_43361d3.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_55e489f.png)

   Click the play button to start the vehicle:

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_1b1a0ee.png)

2. View the traffic light ID.

   Go back to the terminal and open cyber_monitor

   ```bash
   cyber_monitor
   ```

   Find the planning channel.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a1fa25f.png)

   Press the PgUp key on the keyboard to page up and find the reason the vehicle stopped.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_7165b76.png)

   > Note: The number after TL\_ is the traffic light ID.

3. Modify the traffic light ID.

   Add the traffic light ID in the tool:

   ```bash
    def add_traffic_light(color, traffic_light_pb):
        light = traffic_light_pb.traffic_light.add()
        light.color = color
        #light.id = "TRAFFIC_LIGHT_ID" # Before modification
        light.id = "451089196 # After modification
        light.tracking_time = 10.0
   ```

4. Run the traffic light simulation control tool.

   ```bash
   python cyber_trafficlight.py
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_02a8989.png)

   > Note: Number 1 sets the traffic light to red; number 2 sets the traffic light to green.

Number 1:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_92bee2e.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_dc4fad6.png)

Number 2:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_f562f17.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_21fdb15.png)
