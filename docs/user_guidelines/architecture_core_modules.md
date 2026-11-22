# Core Modules

## Core Software Modules:

The Apollo autonomous driving application layer provides dozens of core modules, including perception, prediction, planning, control, and human-machine interaction. The interactions between these modules are shown in the figure below:

![Apollo_software_architecture.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_8_0/Apollo_software_architecture_fd24164.png)

Note: Orange arrows indicate data flow, and black arrows indicate processing flow.

| Module                  | Description                                                                                                                                       |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| Perception              | The perception module identifies the world around the autonomous vehicle. It has two important submodules: obstacle detection and traffic light detection. |
| Prediction              | The prediction module predicts the future motion trajectories of the obstacles detected by perception.                                           |
| Routing                 | The routing module tells the autonomous vehicle how to reach its destination through a series of lanes or roads.                                  |
| Planning                | The planning module plans the temporal and spatial trajectory of the autonomous vehicle.                                                          |
| Control                 | The control module executes the trajectory produced by the planning module by generating control commands such as throttle, brake, and steering. |
| CanBus                  | CanBus is the interface that passes control commands to the vehicle hardware. It also passes chassis information to the software system.          |
| HD-Map                  | This module is similar to a library. Rather than publishing and subscribing to messages, it is often used as a query engine to provide specific structured information about the road. |
| Localization            | The localization module uses information from GPS, LiDAR, and IMU devices to estimate the position of the autonomous vehicle.                     |
| HMI (Human-Machine Interaction) | The human-machine interaction module, that is, DreamView in Apollo, is a module for viewing vehicle status, testing other modules, and controlling vehicle functions in real time. |
| Monitor                 | The monitoring system for all software and hardware modules in the vehicle.                                                                      |

## Rich Autonomous Driving Algorithms

Apollo provides a rich set of autonomous driving algorithms, covering the core autonomous driving technology stack including perception, planning, control, and localization.

### Multi-Modal, Multi-View Perception Algorithms

- Vision-based traffic light recognition
- Vision-based lane line detection
- Vision-based obstacle recognition
- LiDAR point cloud obstacle recognition
- Multi-sensor fusion obstacle recognition
- Vehicle-road cooperative perception fusion

### Scenario-Based Planning Algorithms

- Public Road planner
- Lattice planner
- Navi planner
- RTK planner
- Model-based planner
- Model-based Open Space planner

### Precise Control Algorithms

- PID controller
- LQR controller
- MPC controller
- MRAC dual-loop controller

### Multi-Fusion Localization Algorithms

- RTK localization
- Multi-sensor fusion (MSF) localization

## Documentation Feedback

If you encounter any problems while using the documentation, please go to the [feedback collection Q&A page](https://studio.apollo.auto/community/article/163) we have set up in the [developer community] to report them. We will iteratively improve the documentation based on your feedback.
