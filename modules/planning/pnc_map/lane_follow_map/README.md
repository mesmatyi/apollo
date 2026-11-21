planning-pnc_map-lane_follow_map
============

## Introduction

`LaneFollowMap`: a subclass of `PncMapBase`. Under the `lane_follow_command` command, it does the preparatory work in converting the routing result into a ReferenceLine. Its main functions are to process the routing result and store the routing data in map-related data structures; and, based on the routing result and the current vehicle position, to compute the drivable passage information and convert it into the RouteSegments structure, providing the input data for ReferenceLine. It is encapsulated in reference_line_provider.

### Basic structure

`LaneFollowMap` stores the path as RoadSegment, Passage, and LaneSegment as shown in the figure below

![](./images/map_road_info.png)

### Main member variables

#### Data structures
- Defines the storage of lane data, where `index` is the triple `{road_index, passage_index, lane_index}` of the current lane_segment.
```
struct RouteIndex {
    apollo::hdmap::LaneSegment segment;
    std::array<int, 3> index;
  };
```

- Defines the storage of a waypoint, along with the index of that waypoint in `route_indices_`.
```
struct WaypointIndex {
    apollo::hdmap::LaneWaypoint waypoint;
    int index;
    WaypointIndex(const apollo::hdmap::LaneWaypoint &waypoint, int index)
        : waypoint(waypoint), index(index) {}
  };
```

| Member variable                                                                | Description             |
| --------------------------------------------------------------------- | ---------------- |
| `std::vector<RouteIndex> route_indices_` | Stores the information of all roads, passages, and lanes of the routing   |
| `int range_start_`      | Start index of the search range that the ego vehicle updates according to the routing result |
| `int range_end_`      | End index of the search range that the ego vehicle updates according to the routing result |
| `std::unordered_set<std::string> range_lane_ids_` | IDs of all lanes within the search range  |
| `std::unordered_set<std::string> all_lane_ids_`  | IDs of all lanes in the current routing result  |
| `std::vector<WaypointIndex> routing_waypoint_index_` | All waypoints of the current routing result and their corresponding indices |
| `std::size_t next_routing_waypoint_index_` | The next waypoint to pass |
| `common::VehicleState adc_state_` | Ego vehicle state |
| `int adc_route_index_` | Index of the route where the ego vehicle is located |
| `apollo::hdmap::LaneWaypoint adc_waypoint_` | Waypoint where the ego vehicle is located |

### Main member functions

#### 1. UpdatePlanningCommand
- The input parameter is `planning::PlanningCommand &command`. Whenever there is a new routing result, the data in `LaneFollowMap` is updated with the latest RoutingResponse. Three for loops separately extract the roads, passages, and lanes from the routing result, store the id of each lane in the `all_lane_ids_` array, and store each `lane_segment` together with `{road_index, passage_index, lane_index}` in `route_indices_`.
- Reset `range_start_`, `range_end_`, `adc_route_index_`, and `next_routing_waypoint_index_`, and call the member function `UpdateRoutingRange` to store the lane_ids of the routing result in `range_lane_ids_`.
- Store the waypoint information in routing_request in `routing_waypoint_index_`, which completes the conversion of the routing information.

#### 2. UpdateVehicleState
- The input parameter is `VehicleState &vehicle_state`. Based on the ego vehicle state, the member function `GetNearestPointFromRouting` is called to find the nearest road point `adc_waypoint_` from the routing.
- Based on `adc_waypoint_`, the member function `GetWaypointIndex` is called to find the index `route_index` in `route_indices_` of the LaneSegment currently being driven, and `adc_route_index_` is updated with it.
- The member function `UpdateNextRoutingWaypointIndex` is called with the parameter `route_index` to compute the next waypoint the ego vehicle must pass, and the result is assigned to `next_routing_waypoint_index_`.
- The member function `UpdateRoutingRange` is called with the parameter `adc_route_index_` to update the remaining route not yet passed by the ego vehicle and store it in `range_lane_ids_`.

#### 3. GetRouteSegments
- Called in `ReferenceLineProvider` to update and obtain the current `route_segments`, providing data support for reference line construction.
- The input parameters are `const VehicleState &vehicle_state, std::list<hdmap::RouteSegments> *const route_segments`; `route_segments` is passed by reference to update the route data.
- Call `LookForwardDistance` to compute the forward search distance look_forward_distance based on the current vehicle speed. The backward search distance look_backward_distance is controlled by a parameter.
- Call the overloaded function of `GetRouteSegments` to obtain `route_segments`, with the parameters `vehicle_state, look_backward_distance, look_forward_distance, route_segments`.

#### 4. `GetRouteSegments` overload
- The input parameters are `vehicle_state, look_backward_distance, look_forward_distance, route_segments`.
- Call `UpdateVehicleState` to update the route state.
- From the updated `adc_route_index_`, obtain the indices `road_index` and `passage_index` of the `RoadSegment` and `Passage` where the vehicle is currently located. Based on the current road and passage, call `GetNeighborPassages` to compute the information `drive_passages` of all passages adjacent to the current lane.
- A for loop traverses `drive_passages`. The member function `PassageToSegments` is called to convert the passage into `hdmap::RouteSegments segments`, and the projection point of the ego vehicle onto the current `segments` is computed. From this projection point, the segments are extended by the forward search distance look_forward_distance and the backward search distance look_backward_distance. This yields the `segments` of the current passage, which are saved to `route_segments`.
- 
#### 5. GetNeighborPassages
- The input parameters are `const routing::RoadSegment &road, int start_passage`. Called by `GetRouteSegments`, it obtains the neighboring passages starting from the `start_passage` where the ego vehicle currently is.
- If `source_passage.change_lane_type() == routing::FORWARD`, the current passage can reach the target point directly without changing lanes; or if `source_passage.can_exit()`, the current passage can be exited to reach the target point, then there is no need to search for neighboring passages and the function returns.
- Obtain `source_segments` from `source_passage`, and, based on the current lane_segments and the direction of the target passage, search for neighboring lanes to the left and right respectively, saving their ids to `neighbor_lanes`.
- Traverse all passages of `road` and save the `lane_segment`s whose ids are the same as those in `neighbor_lanes`.
 
#### 6. PassageToSegments
- The input parameters are `routing::Passage passage, hdmap::RouteSegments *segments`
- Traverse the `lane_segment`s in `passage`, convert them to the `RouteSegments` type, and save them to `segments`.

#### 7. UpdateRoutingRange
- The input parameter is `adc_index`
- Traverse `route_indices_` and save the route ids starting from the ego vehicle's current `adc_index` to `range_lane_ids_`.

#### 8. GetNearestPointFromRouting
- The input parameters are `const VehicleState &state, hdmap::LaneWaypoint *waypoint`; it obtains the nearest `waypoint` according to the ego vehicle position.
- Traverse `all_lane_ids_` to get all available lanes and save them to `valid_lanes`.
- Traverse `valid_lanes` to get all waypoints within the lane where the ego vehicle is longitudinally located, and save them to `valid_way_points`.
- Traverse `valid_way_points` to get all waypoints within the lane where the ego vehicle is longitudinally located, and save them to `valid_way_points`.
- Traverse `valid_way_points` to get the nearest `waypoint`.

#### 9. GetWaypointIndex
- The input parameter is `const hdmap::LaneWaypoint &waypoint`. Through forward and backward search, it returns the index of that waypoint in `route_indices_`.

#### 10. UpdateNextRoutingWaypointIndex
- The input parameter is `int cur_index`. From the current index, it computes the next waypoint that needs to be passed in the routing result and updates `next_routing_waypoint_index_`.


## Directory Structure

```shell
modules/planning/pnc_map/lane_follow_map/
├── BUILD
├── cyberfile.xml
├── images
│   └── map_road_info.png
├── lane_follow_map.cc
├── lane_follow_map.h
├── lane_follow_map_test.cc
├── plugins.xml
└── README.md
```

## Module

### LaneFollowMap

apollo::planning::LaneFollowMap

#### Usage

##### Configuring LaneFollowMap
Modify the `reference_line_config` configuration in the `modules/planning/planning_component/conf/planning_config.pb.txt` file to use `LaneFollowMap`.
``` shell
# modules/planning/planning_component/conf/planning_config.pb.txt
reference_line_config {
  pnc_map_class: "apollo::planning::LaneFollowMap"
}
```
