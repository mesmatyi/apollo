# planning-planner-public-road

## Introduction

`planning-planner-public-road` is a plugin package containing the PublicRoadPlanner. PublicRoadPlanner is the open-road planner and the planner used by default in planning; it plans based on the scenario mechanism.

### Scenario Mechanism

Starting with Apollo 3.5, the planning module uses a two-layer state machine scenario mechanism. Compared with versions before Apollo 3.0, where every driving use case was treated as a different driving scenario, issues reported in a specific scenario can now be fixed without affecting the work of other scenarios, and the fix does not affect other driving use cases.

![](./docs/images/state_machine.png)

The Top Layer of the two-layer state machine is the Scenario state machine, and the Bottom Layer is the Stage state machine. Scenario switching happens in the Top Layer: the ScenarioManager decides which scenario to switch to based on the surrounding environment and map information, such as the LaneFollow scenario for driving along the road and the PullOver scenario for pulling over. Stage switching happens in the Bottom Layer: if the previous Stage has finished, the next Stage is executed. When all Stages have finished, the current Scenario is considered finished.

Planning currently supports the following scenarios:

- **apollo::planning::LaneFollowScenario**: The lane-following scenario, which is the default autonomous driving scenario. In this scenario the vehicle drives along the routing path. When it encounters obstacles, it, depending on the situation, bypasses them within the current lane or borrows the adjacent lane to bypass them, and changes lanes according to the route information in routing. For traffic signs on the road, such as stop signs, yield signs, crosswalks, and speed bumps, it slows down or stops to yield according to traffic rules.

![](./docs/images/lane_follow_scenario.png)

- **apollo::planning::PullOverScenario**: The pull-over scenario. If the parameter `enable_pull_over_at_destination` is set to `true`, the vehicle automatically switches into `PullOverScenario` when it approaches the destination and completes the pull-over.

![](./docs/images/pull_over_scenario.png)

- **apollo::planning::BareIntersectionUnprotectedScenario**: The unprotected bare intersection scenario. At such an intersection there is neither a stop sign nor a traffic light, and the vehicle switches to this scenario within a certain distance before the intersection.

![](./docs/images/bare_intersection_scenario.png)

- **apollo::planning::TrafficLightProtectedScenario**: The protected traffic light intersection scenario. At such intersections there are explicit traffic light indications for going straight, turning left, and turning right.

![](./docs/images/traffic_light_protected_scenario.png)

- **apollo::planning::TrafficLightUnprotectedLeftTurnScenario**: The unprotected traffic light left turn scenario. In this scenario, when the vehicle turns left at a traffic light intersection, oncoming vehicles may still pass through the intersection, and the vehicle must yield to them. The design of TrafficLightUnprotectedLeftTurnScenario therefore differs from that of the protected traffic light scenario in that, when passing through the intersection, a slow-moving stage (Creep) is added so that the vehicle can observe oncoming vehicles and yield as appropriate.

![](./docs/images/traffic_light_unprotected_left_scenario.png)

- **apollo::planning::TrafficLightUnprotectedRightTurnScenario**: The unprotected traffic light right turn scenario. In this scenario, when the vehicle turns right at a traffic light, vehicles may pass from other directions, so the vehicle must slow down, observe the traffic light status, and turn right only when it is safe.

![](./docs/images/traffic_light_unprotected_right_scenario.png)

- **apollo::planning::StopSignUnprotectedScenario**: The unprotected stop sign intersection scenario. At such an intersection there is only a two-way stop sign, and before passing through the intersection the vehicle must observe the vehicles coming and going and proceed only when the traffic density at the intersection is low.

![](./docs/images/stop_sign_unprotected_scenario.png)

![](./docs/images/stop_sign_unprotected_1.png)

<center>Two-way stop sign intersection</center>

![](./docs/images/stop_sign_unprotected_2.png)

<center>Four-way stop sign intersection</center>

- **apollo::planning::YieldSignScenario**: The scenario of an intersection with a yield sign. When the vehicle encounters a yield sign and there are oncoming vehicles, it must let the oncoming vehicles go first.

![](./docs/images/yield_sign_scenario.png)

- **apollo::planning::ValetParkingScenario**: The valet parking scenario. When the planning input command RoutingResponse contains parking_id information, the vehicle is to be parked in the parking spot corresponding to that parking_id in the map.

![](./docs/images/valet_parking_scenario.png)

- **apollo::planning::EmergencyPullOverScenario**: The emergency pull-over scenario. If the vehicle receives the PadMessage command "PULL_OVER" while driving, it finds a suitable position nearby and stops within the current lane. Compared with stopping directly, this ensures stopping safety during driving.

![](./docs/images/emergency_pull_over_scenario.png)

- **apollo::planning::ParkAndGoScenario**: The command for the vehicle, after pulling over, to start from its current position and drive toward the specified next target. This scenario is suitable for shuttle-bus situations. The scenario uses the Open Space algorithm to ensure that the vehicle can safely pull out of the specified position.

![](./docs/images/park_and_go_scenario.png)

## Directory Structure

```shell

modules/planning/planner/public_road
├── public_road
    ├── conf                        // Configuration files
    ├── docs                        // Documentation
    ├── proto                       // Configuration proto files
    ├── public_road_planner.h       // PublicRoadPlanner header file
    ├── public_road_planner.cc      // PublicRoadPlanner source file
    ├── scenario_manager.h          // Scenario manager class header file
    ├── scenario_manager.cc         // Scenario manager class source file
    ├── public_road_planner_test.cc // Unit test file
    ├── BUILD                       // Build rules file
    ├── cyberfile.xml               // Package management configuration file
    ├── plugins.xml                 // Plugin description file
    └── README.md                   // Documentation
```
## Plugin

### PublicRoadPlanner

#### Configuration

| File path                                                                     | Type/Structure                                       | <div style="width: 300pt">Description</div> |
| ---------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
| `modules/planning/planners/public_road/conf/planner_config.pb.txt`                 | `apollo::planning::PlannerPublicRoadConfig`              | Configuration file of PublicRoadPlanner               |
