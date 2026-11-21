planning-task-open-space-roi-decider
==============

## Introduction
`OpenSpaceRoiDecider` is used to generate the drivable boundary in the open-space algorithm. Depending on the planning scenario, such as parking, pull-over, or entering the main road, it generates different drivable boundaries and target points, with the current vehicle position as the coordinate origin.

### GetParkingSpot
The function first converts the polygon of the parking spot to clockwise order, then computes the pose of the target parking spot, determines from its angle with the road whether the spot is a parallel or perpendicular spot, and stores the parking spot information in `parking_info`.

### SetParkingSpotEndPose
Converts the target point pose into the local coordinate system whose origin is the upper-left corner of the target parking spot, and stores it in `parking_info`.

### GetParkingBoundary
This is an overloaded function. `GetParkingBoundary(const ParkingInfo &parking_info, const hdmap::Path &nearby_path, Frame *const frame, std::vector<std::vector<common::math::Vec2d>> *const roi_parking_boundary)` converts the target parking spot into the local coordinate system, then computes the road boundary through the `GetRoadBoundary` function, and then combines the parking spot boundary with the road boundary according to the positional relationship between the parking spot and the road. `GetParkingBoundary(const hdmap::Path &nearby_path, Frame *const frame, std::vector<std::vector<common::math::Vec2d>> *const roi_parking_boundary)` likewise converts the target parking spot into the local coordinate system and then computes the road boundary through the `GetRoadBoundary` function; the difference is that this function inflates the road boundary through `right_lane_boundary[i] *= (std::fabs(left_top_l))` so that it can cover the parking spot.

### GetPullOverSpot
This function computes the pose of the pull-over target point and generates the target stopping bounding box.

### SetPullOverSpotEndPose
Converts the target point pose into the coordinate system whose origin is the upper-left corner of the target parking spot, and stores the target pose in `frame`.

### GetPullOverBoundary
This function computes the left and right boundaries of the road of the target parking spot according to the position of the target parking spot on the road, and then generates the open-space algorithm boundary.

### SetOriginFromADC
Inflates the ego vehicle box and uses the upper-left corner point of the inflated box as the coordinate system origin.

### GetParkAndGoBoundary
Computes the road boundary from the vehicle's position on the road and the road information, and uses it as the open-space algorithm boundary.

### SetParkAndGoEndPose
Computes the target end point pose from the projection point of the current vehicle position onto the reference line, and converts the target point into the coordinate system whose origin is the upper-left pose of the ego vehicle.

### FormulateBoundaryConstraints
Converts the processed obstacle boundaries into vector form through the function `LoadObstacleInVertices`, then converts the obstacle vectors into hyperplanes through the function `LoadObstacleInHyperPlanes`, and saves the data in `frame`.

## Directory Structure 
```shell
modules/planning/tasks/open_space_roi_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── open_space_roi_decider.cc
├── open_space_roi_decider.h
├── open_space_roi_decider_test.cc
├── plugins.xml
├── proto
│   ├── BUILD
│   └── open_space_roi_decider.proto
└── README.md
```

## Module Input, Output, and Configuration

### planning-task-open-space-pre-stop-decider plugin

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/open_space_roi_decider/conf/default_conf.pb.txt` | apollo::planning::OpenSpaceRoiDeciderConfig | Configuration file of OpenSpaceRoiDecider |
|`modules/common/data/vehicle_param.pb.txt`|`apollo::common::VehicleConfig`|Vehicle chassis configuration file|

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the OpenSpaceRoiDecider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `OpenSpaceRoiDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `OpenSpaceRoiDecider`.
```
task {
  name: "OPEN_SPACE_ROI_DECIDER"
  type: "OpenSpaceRoiDecider"
}
```
##### Configuring OpenSpaceRoiDecider parameters
In `modules/planning/tasks/open_space_roi_decider/conf/default_conf.pb.txt`, configure the parameters of the `OpenSpaceRoiDecider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `OpenSpaceRoiDecider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
