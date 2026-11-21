# Module Overview

The Planning module plays a vital role in the autonomous driving system. While obeying traffic rules, it navigates the vehicle from its current position along the route to the destination.

When Planning plans a trajectory, there are three basic requirements to satisfy:

- Obey road traffic rules, such as stopping to yield before a crosswalk, stopping when the traffic light turns red and proceeding when it turns green,
- Do not collide with other traffic participants on the road, such as pedestrians, vehicles, and fixed roadside obstacles,
- The planned output trajectory, including path curvature, speed, and acceleration, must not exceed the vehicle's execution capability limits.

In addition, the trajectory planned by Planning must also consider some other factors, such as comfort: the rate of change of acceleration and deceleration should not be too large, and the rate of change of the steering wheel angle should not be too large.

## Running Flow

Based on the perceived surrounding environment information, map localization and navigation information, and global path information input by upstream modules, the planning module plans a motion trajectory for the autonomous vehicle (including coordinates, speed, acceleration, jerk, time, and other information), and then passes this information to the control module.

![image (31).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2831%29_509d61d.png)

## Module Principles

The Apollo planning module is implemented on a **scenario-based** approach. For different scenarios, the planning module completes trajectory planning through a combination of a series of independent **tasks**. Developers can adjust the list of supported scenarios, or the task types supported within a scenario, according to their own needs.

![image (8).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%288%29_2cf19d6.png)

The Apollo planning architecture diagram is shown above, and some of the important modules are as follows:

- Public Road Planner: implements a finite state machine that determines the driving scenario of the autonomous vehicle by combining navigation, environment, and other information,
- Scenario Manager: calls the Scenario suited to the current scenario according to the state machine and vehicle-related information,
- Scenario: combines information from upstream modules and completes the trajectory planning of the autonomous vehicle through a combination of a series of tasks,
- Task combination: a set of tasks that implement decision-making and optimization. Optimizers are used to optimize the vehicle's trajectory and speed. Deciders, based on rules, determine driving behaviors of the autonomous vehicle such as when to change lanes, when to stop, when to creep (move slowly), or when creeping is complete.
