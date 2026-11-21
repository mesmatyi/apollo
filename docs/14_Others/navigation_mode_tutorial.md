### Apollo Navigation Mode Tutorial

**Warning**
> This document applies only to Apollo 2.5 and 3.0; Apollo 3.5 and later versions do not support it.

#### 1. Tutorial introduction

The autonomous driving system uses real-time perception information and static map information to build a complete driving environment, and within the constructed environment, plans a driving trajectory based on routing data, which the control module then executes. Within this framework, Apollo navigation mode makes the following improvements for simple road scenarios such as highways and rural roads:

1. To improve safety and stability, Apollo navigation mode adds static guidance information to the driving environment, which guides trajectory planning in the driving environment, making it safer and more comfortable. At the same time, the guidance information also lowers the requirement on the completeness of the driving environment -- that is, it lowers the requirement on map information.
2. Apollo navigation mode uses a relative (vehicle body) coordinate system, which reduces the transformation of sensor data. It also supports switching among various driving modes to cope with different driving scenarios and conditions.
3. Apollo navigation mode introduces the Routing function of Baidu Maps, taking real-time traffic information into account, which makes the Routing results more practical, accurate and stable. It also makes the Apollo system easier to deploy and commercialize.



##### In this tutorial, you will accomplish the following

After completing this tutorial, you will be able to debug and develop the planning module offline in navigation mode.

##### In this tutorial, you will learn

- How to set up Apollo navigation mode
- How to use the cloud guider to send a guidance line
- How to generate a guidance line from a recorded ROS bag and send it with the offline guider
- How to debug the planning module



##### In this tutorial, you need the following preparation

- Download and build the latest Apollo source code ([Howto](https://github.com/ApolloAuto/apollo/tree/master/docs/demo_guide))

- Download the [Apollo2.5 demo bag](https://github.com/ApolloAuto/apollo/releases/download/v2.5.0/demo_2.5.bag)


#### 2. Configuring navigation mode

In navigation mode, the following parameters need to be configured:

- Perception solution: currently the camera solution (CAMERA) and the Mobileye-based solution (MOBILEYE) are supported
- Apollo UTM Zone
- Planner of the planning module: currently EM, LATTICE and NAVI are supported
- System speed limit: in meters per second

##### Modifying the configuration file in Docker

The configuration file is located at:

```bash
/apollo/modules/tools/navigation/config/default.ini
```

The default configuration is:

```bash
[PerceptionConf]
# three perception solutions: MOBILEYE, CAMERA, and VELODYNE64
perception = CAMERA

[LocalizationConf]
utm_zone = 10

[PlanningConf]
# three planners are available: EM, LATTICE, NAVI
planner_type = EM

# highest speed for planning algorithms, unit is meter per second
speed_limit = 5
```

This default configuration is the configuration used when the Apollo 2.5 demo bag was recorded, and we use it directly in this tutorial.

##### Making the configuration take effect

To make the configuration take effect, run the following command in the Apollo root directory inside Docker

```bash
in_dev_docker:/apollo$ cd /apollo/modules/tools/navigation/config/
in_dev_docker:/apollo/modules/tools/navigation/config$ python navi_config.py default.ini
```

#### 3. Using the cloud guider

##### Playing back the demo bag

Before entering Docker and starting Apollo, we copy the [Apollo2.5 demo bag](https://github.com/ApolloAuto/apollo/releases/download/v2.5.0/demo_2.5.bag) into the data directory under the Apollo code root directory.

After a successful build inside Docker, we start Dreamview with the following command:

```bash
in_dev_docker:/apollo$ ./scripts/bootstrap.sh start
```

and open the following in a local browser

```bash
http://localhost:8888
```

As shown in the figure below, select "Navigation" in the mode box.

![img](images/navigation_mode_tutorial/navigation_mode_1_init.png) 

Then run the following command in the apollo root directory inside Docker to play the bag

```bash
in_dev_docker:/apollo$cd data
in_dev_docker:/apollo/data$rosbag play demo_2.5.bag
```

After playback starts, you can see the Dreamview interface as follows

![img](images/navigation_mode_tutorial/navigation_mode_2_play.png)

##### Requesting a cloud guidance line

Select a destination on the map (along Canada Road) and click the red Route button in the map view. The cloud guider receives this request and returns a guidance line, which is displayed in the map view, as shown in the figure below.

![img](images/navigation_mode_tutorial/navigation_mode_3_cloud.png)

That is the invocation process of the cloud guider.

#### 4. Using the offline guider tool

Currently the cloud guider covers only a limited area. In addition to the cloud service, we also provide an offline guider tool to create and send offline guidance lines. In this tutorial, we use the [Apollo2.5 demo bag](https://github.com/ApolloAuto/apollo/releases/download/v2.5.0/demo_2.5.bag) as an example to generate a guidance line.

##### Creating a guidance line

The steps to generate a guidance line are

- Extract the path data from the bag

```bash
in_dev_docker:/apollo$cd modules/tools/navigator
in_dev_docker:/apollo/modules/tools/navigator$python extractor.py /apollo/data/demo_2.5.bag
```

The extracted path data is in the path

```bash
in_dev_docker:/apollo/modules/tools/navigator$
```

in the

```bash
path_demo_2.5.bag.txt
```

- Smooth the path data

```bash
in_dev_docker:/apollo/modules/tools/navigator$bash smooth.sh path_demo_2.5.bag.txt 200
```

The smoothed data is in

```bash
in_dev_docker:/apollo/modules/tools/navigator$path_demo_2.5.bag.txt.smoothed
```

##### Sending the guidance line

Once the smoothed data is obtained, it can be sent to the Apollo system as a guidance line. The steps are:

```bash
in_dev_docker:/apollo/modules/tools/navigator$python navigator.py path_demo_2.5.bag.txt.smoothed
```

After sending is complete, the red guidance line in the Dreamview map view is updated as shown in the figure below:

![img](images/navigation_mode_tutorial/navigation_mode_4_offline.png)

#### 5. Debugging the planning module

##### Preparing debugging data

To debug a module using a bag, first filter out the corresponding ROS messages from the bag. Suppose we want to debug the planning module; we need to filter out the message

```
/apollo/planning
```

Filter it out using the following command

```bash
in_dev_docker:/apollo$cd data
in_dev_docker:/apollo/data$rosbag filter demo_2.5.bag demo_2.5_no_planning.bag "topic != '/apollo/planning'"
```

The filtered bag is located at

```bash
in_dev_docker:/apollo/data$demo_2.5_no_planning.bag
```

##### Generating the planned trajectory

We play the bag without planning, using the following command

```bash
in_dev_docker:/apollo/data$rosbag play demo_2.5_no_planning.bag
```

In Dreamview we will see that the vehicle's planned trajectory is not output, as shown in the figure below

![img](images/navigation_mode_tutorial/navigation_mode_5_no_planning.png)

We open the Navi Planning module in Dreamview, as shown in the figure below

![img](images/navigation_mode_tutorial/navigation_mode_6_live_planning.png)

We see the real-time computed planned trajectory of the vehicle displayed in Dreamview. At this point you can try changing some of the planning module's configuration

```
in_dev_docker:/apollo/modules/planning/conf$planning_config_navi.pb.txt
```

to understand how these parameters affect the planning results, or modify the code of the planning algorithm to debug.

#### 6. Conclusion

Congratulations on completing this tutorial. You should now understand

- How to set up Apollo navigation mode
- How to use the cloud guider to send a guidance line
- How to generate a guidance line from a recorded ROS bag and send it with the offline guider
- How to debug the planning module

You can also try using the demo bag to debug some other modules.
