## Experiment Content

This experiment aims to use Apollo Cyber RT to develop a route setting tool, with which you can create and send a routing request message.

## Experience Link

https://apollo.baidu.com/community/course/40

## Experiment Objectives

Through this experiment, learn how to use Apollo Cyber RT to develop a route setting tool and use it to create and send a routing request so that the vehicle starts moving.

- Create a cyber project,

- Become familiar with building cyber tools.

## Practice Steps

### Step 1: Start the Simulation Environment

1. In the terminal, run the DreamView+ startup command. After it succeeds, click the dreamview+ button in the menu bar to enter the dreamview+ interface.

   ```bash
   aem bootstrap start --plus
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_9b5696d.png)

   The following screen indicates that dreamview+ has started successfully.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_204edc0.png)

2. Open the **Mode Settings** panel in the left navigation bar, select **PnC** Mode as the mode and **Sim_Control** as the operation to enter simulation mode.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c8bc6e9.png)

3. Select the map.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_984ecea.png)

4. Start the Planning module from the **Modules** list on the left.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c4c0ba6.png)

### Step 2: Edit the Code to Implement the Task

#### Prerequisites:

Create a file named `cyber_routing.py` in the `/apollo_workspace` directory:

1. Import the necessary libraries and modules.

   First, import the required Python libraries and Apollo Cyber RT modules. These libraries and modules help us initialize the Cyber framework and create the routing request message.

   ```bash
   import argparseimport time
   from cyber.python.cyber_py3 import cyber
   from cyber.python.cyber_py3 import cyber_time
   from modules.common_msgs.external_command_msgs import lane_follow_command_pb2
   from modules.common_msgs.external_command_msgs import command_status_pb2
   ```

2. Initialize the Cyber framework.

   In the main function, we need to initialize the Cyber framework in order to communicate with Cyber nodes. This is the first step toward interacting with other modules.

   ```bash
   def main():
    # Initialize Cyber
    cyber.init()
    node = cyber.Node("mock_routing_requester")
    sequence_num = 1
   ```

3. Create the routing request message.

   In this step, we create a routing request message (LaneFollowCommand) and set some necessary attributes, such as the timestamp, module name and sequence number.

   ```bash
    routing_request = lane_follow_command_pb2.LaneFollowCommand()
    routing_request.header.timestamp_sec = cyber_time.Time.now().to_sec()
    routing_request.header.module_name = 'cyber_routing'
    routing_request.header.sequence_num = sequence_num
    routing_request.is_start_pose_set = True
   ```

4. Enter the start and end point coordinates.

   Get the start and end point coordinates from user input on the command line, including the start heading, start X coordinate, start Y coordinate, end heading, end X coordinate and end Y coordinate.

   ```bash
    start_heading = float(input("start_heading : "))
    start_x = float(input("start_x : "))
    start_y = float(input("start_y : "))
    end_heading = float(input("end_heading: "))
    end_x = float(input("end_x : "))
    end_y = float(input("end_y: "))
   ```

5. Add the path information.

   In this step, the entered start and end point coordinates are added to the routing request message, including the coordinates of the start and end points.

   ```bash
    waypoint = routing_request.way_point.add()
    waypoint.heading = start_heading
    waypoint.x = start_x
    waypoint.y = start_y

    waypoint1 = routing_request.end_pose
    waypoint1.heading = end_heading
    waypoint1.x = end_x
    waypoint1.y = end_y
   ```

6. Create a Cyber Writer and send the routing request.

   Finally, create a Cyber Writer and use it to send the routing request message to "/apollo/external_command/lane_follow".

   ```bash
   writer = node.create_client('/apollo/external_command/lane_follow', lane_follow_command_pb2.LaneFollowCommand, command_status_pb2.CommandStatus)
    time.sleep(2.0)
    print("routing", routing_request)
    writer.send_request(routing_request)
   ```

   Cyber tool code:

   ```bash
   from cyber.python.cyber_py3 import cyber
    from cyber.python.cyber_py3 import cyber_time
    from modules.common_msgs.external_command_msgs import lane_follow_command_pb2
    from modules.common_msgs.external_command_msgs import command_status_pb2

    def main():
        """
        Main function
        """
        # Initialize Cyber
        cyber.init()
        node = cyber.Node("mock_routing_requester")
        sequence_num = 1

        routing_request = lane_follow_command_pb2.LaneFollowCommand()

        routing_request.header.timestamp_sec = cyber_time.Time.now().to_sec()
        routing_request.header.module_name = '111'
        routing_request.header.sequence_num = sequence_num

        routing_request.is_start_pose_set = True

        sequence_num = sequence_num + 1

        # Select the start point coordinates from the command line
        start_heading = float(input("start_heading : "))
        start_x = float(input("start_x : "))
        start_y = float(input("start_y : "))

        # Select the end point coordinates from the command line
        end_heading = float(input("end_heading: "))
        end_x = float(input("end_x : "))
        end_y = float(input("end_y: "))

        # Add the path start point
        waypoint = routing_request.way_point.add()
        waypoint.heading = start_heading
        waypoint.x = start_x
        waypoint.y = start_y

        # Set the end point coordinates
        waypoint1 = routing_request.end_pose
        waypoint1.heading = end_heading
        waypoint1.x = end_x
        waypoint1.y = end_y

        # Create the Cyber writer
        writer = node.create_client('/apollo/external_command/lane_follow', lane_follow_command_pb2.LaneFollowCommand, command_status_pb2.CommandStatus)
        time.sleep(2.0)
        print("routing", routing_request)
        writer.send_request(routing_request)

    if __name__ == '__main__':
        main()
   ```

### Step 3: Using the Route Setting Tool

1. Run the route setting tool.

   ```bash
   python cyber_routing.py
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_efc8ef9.png)

2. Open Dreamview+ routing editing.

   Pick any two points on the map as the start point and end point:

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_cccb399.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_5d65e9e.png)

3. Enter the start point coordinates, end point coordinates and headings in the tool.

   ```bash
   start_heading : 0.03
    start_x : 752517.91
    start_y : 2563962.54
    end_heading: 0.00
    end_x : 752754.52
    end_y: 2563960.03
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_d1a0429.png)

   [Vehicle running]

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3919a2f.png)

[Vehicle running]
