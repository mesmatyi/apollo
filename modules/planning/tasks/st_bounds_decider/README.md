planning-task-st-bounds-decider
==============

## Introduction
`ST BOUNDS DECIDER` is the ST boundary decider. This module consists of 4 parts of code: the basic function st_bounds_decider, and three utilities, st_driving_limits, st_guide_line, and st_obstacle_processor.
It mainly builds the ST graph for dynamic obstacles and the nearest static obstacle that blocks the current guide path. It ignores obstacles that have no effect on the current longitudinal planning and, following the set trajectory, gives the optimal decision for each obstacle's boundary, overtake or yield (follow behind the obstacle), finally deciding the optimal Drivable_st_boundary;


1) st_bounds_decider
### Process
The main logic function of the st bounds decider module.

1.1 Initialize STBoundsDecider

1.2 Call the GenerateRegularSTBound method to generate regular_st_bound and regular_vt_bound

1.3 Call the SetSTDrivableBoundary method to set t, s_lower, and S_upper for st_drivable_boundary_, and set v_obs_lower and v_obs_upper as needed

1.4 Call the GetAllSTBoundaries method to obtain all boundary information and store it in all_st_boundaries.

1.5 Traverse all st_boundary, record debug information, and print logs


### InitSTBoundsDecider
Initializes st_obstacles_processor_ and calls the MapObstaclesToSTBoundaries method to draw obstacles into the ST graph.
Sets some speed limits, initializes st_guide_line_ and st_driving_limits_, and sets some vehicle dynamics limits.

### GenerateFallbackSTBound
Finds the safest decision method for the ego vehicle over all times (with the largest s from obstacles)

### GenerateRegularSTBound
First generates the st boundary and the vt boundary and fills them with extreme values for later use.

Then it traverses every time instant in all st bounds. It first sets s_lower and s_upper once according to the vehicle dynamics limits,

According to the obstacle st_boundary constraints, it calls GetBoundsFromDecisions to update the passable s_bounds and the corresponding decisions.
There may be several decisions for the st_boundary of the same obstacle, such as overtake or yield,

According to the ego vehicle kinematic constraints, it calls RemoveInvalidDecisions to remove unreachable obstacles

Sorts avaliable_choices iteratively by rule

Finds the passable space with the highest priority and assigns it to obstacleDecision()

Updates s_guide_line/st_driving_limits/v_limts

### RemoveInvalidDecisions
According to the ego vehicle kinematic constraints, removes the decisions of unreachable obstacles

### RankDecisions
Sorts avaliable_choices according to rules.


### RecordSTGraphDebug
Prints some debug information

2) st_obstacle_processor utility class

### MapObstaclesToSTBoundaries
Builds the ST graph of obstacles on the path.
This function contains the following parts

2.1 Update the low-right-of-way road segments.

By looping, all low-right-of-way road segments are joined into whole segments and updated in the adc_low_road_right_segments_ container.
For example, suppose l and h denote low and high right of way, and the data in multiple path_pt_info is
1 l
2 l
3 l
4 l
5 h
6 h
7 h
8 l
9 l
10 l,
then after the update, adc_low_road_right_segments_ stores (1,4) (8,10)

Set the set non_ignore_obstacles of non-ignored obstacles

Set the nearest static obstacle closest_stop_obstacle, which will be updated continuously afterwards

2.2 Traverse all obstacles and perform the following operations:
2.2.1 
Draw the obstacle into the ST graph.
Obstacles not in the ST graph are ignored directly; for the other obstacles, the upper and lower boundary points are not trimmed and keep their original values.
For obstacles marked is_caution_obstacle, set the right-of-way end time road_right_ending_t.
For obstacles that are not low-right-of-way, delete the upper and lower boundaries.
Obstacles marked keep clear are added to the candidate_clear_zones_ list and the subsequent operations are skipped.

2.2.2
For static obstacles, compare the static obstacle's boundary in the ST graph with that of the previous obstacle; if it is closer to the ego vehicle, update the nearest static obstacle closest_stop_obstacle.

For dynamic obstacles, backward dynamic obstacles are ignored; forward dynamic obstacles are added to the obs_id_to_st_boundary_ list with their st_boundary set,
and the obstacle id is added to non_ignore_obstacles

2.3
Store the boundary of the nearest static obstacle in the obs_id_boundary_map container.
If the static obstacle's boundary overlaps a keepclear area, update the corresponding id/boundary/obs_ptr

2.4 Obstacles not in the ST graph are set to ignore

2.5 For each obstacle, store the corresponding start/end, upper/lower bound s, and id information

2.6 Sort the edges

### GetLimitingSpeedInfo
First checks whether obs_id_to_decision_ is empty. If it is, there are no obstacles, so it returns false.

Initializes s_min and s_max to the start and end of the planning distance.

Traverses each obstacle in obs_id_to_decision_. For each obstacle:

From the time t and the obstacle boundary information, it computes the obstacle's boundary range and boundary slopes (upper and lower) at time t.
If the obstacle's decision is yield or stop, and the obstacle's minimum distance at time t is less than or equal to s_max, then s_max is updated to the obstacle's minimum distance,
and the obstacle's lower-bound slope is assigned to limiting_speed_info->second

If the obstacle's decision is overtake, and the obstacle's maximum distance at time t is greater than or equal to s_min, then s_min is updated to the obstacle's maximum distance, and the obstacle's upper-bound slope is assigned to limiting_speed_info->first.

Finally it checks whether s_min is less than or equal to s_max. If so, it means limiting speed information exists, and the function returns true; otherwise it returns false.

### GetSBoundsFromDecisions
First draws the t boundary in the ST graph, then deletes boundaries that disappear before t.

For overtaken obstacles, if their high-right-of-way end time is less than or equal to our current time minus the caution time kOvertakenObsCautionTime, they are deleted from obs_id_to_decision_.

It traverses the obs_id_to_decision_ vector and checks whether each decision is an overtake and whether the obstacle's minimum t and maximum t are both less than or equal to the current time minus the caution time.
If so, their IDs are added to the list of obs_ids to delete, and they are deleted from the obs_id_to_decision_ vector.

Gets the s ranges from the existing decisions. It traverses the obs_id_to_decision_ vector and, for each decision, gets its corresponding ST boundary and the s range of that boundary.
If the decision is yield or stop, the minimum of the s range is set to the maximum of s. If the decision is overtake, the maximum of the s range is set to the minimum of s.

If the computed minimum of the s range is greater than the maximum, it returns false, indicating that no valid s range can be obtained.

For newly entered ST boundaries, it determines the possible new ST boundaries. For obstacles that clearly need to be yielded to or stopped for, a decision is made for them directly.

For ambiguous cases, it first looks for the corresponding passable upper and lower boundaries and then makes the decision; when no decision can be made, a default decision is added


### SetObstacleDecision 
(const std::string& obs_id, const ObjectDecisionType& obs_decision)
### SetObstacleDecision
(const std::vector<std::pair<std::string, ObjectDecisionType>>&
        obstacle_decisions)
The two methods above are ultimately implemented in the first one: it sets the obstacle information into the map obs_id_to_decision_, and sets the boundaryType for the obstacle corresponding to each obsid,



### ComputeObstacleSTBoundary
Computes the st_boundary of an obstacle

For static obstacles, that is, those without trajectory_point(), the GetOverlappingS method is used to compute the start and end points of the obstacle's overlap on the path,
and this information is updated in lower_points and upper_points, with is_caution_obstacle set to true and the caution end time being the planning_time_ time

For dynamic obstacles, it traverses every time point, takes the obstacle's OverlappingS, and updates the results into lowerpoints and upperpoints,
then updates iscautiontime and obs_caution_end_t according to the previously configured low-right-of-way segments

### GetOverlappingS
Computes the overlap of an obstacle on the path. It first finds a rough search range through GetSBoundingPathPointIndex; if the search range is too small, it returns false, indicating that the obstacle does not overlap the path.
Then, for each path point, through two loops, one from front to back and one from back to front, it determines the start point and end point of the obstacle's overlap on the path respectively.
The start point and distance are stored in std::pair<double, double>* const overlapping_s

### GetSBoundingPathPointIndex
This is a recursively called function. By repeatedly calling IsPathPointAwayFromObstacle and GetSBoundingPathPointIndex,
it uses binary search to find the rough projection positions of the front and rear of the obstacle on the path, with precision <= sthred

### IsPathPointAwayFromObstacle
Through vector computation, it determines whether a path point is away from all corner points of the obstacle. In this function, corner_pt_s_dist is the computed distance between the obs and the normal to the ego vehicle's driving direction.
Note that the function does not simply compare the absolute value of this distance with sthred to decide whether to return true or false.
Taking is before as an example, when corner_pt_s_dist is negative (even if its absolute value is large), it also returns true; see the function code itself for details.

### IsADCOverlappingWithObstacle
Checks whether the ego vehicle's path points overlap the obstacle's bounding box

### FindSGaps
Finds s gaps between obstacles

### DetermineObstacleDecision
Determines whether to overtake; otherwise returns the default value

### IsSWithinADCLowRoadRightSegment
Determines whether a segment of s is within a low-right-of-way road segment.

3) st_driving_limits

### GetVehicleDynamicsLimits
Computes, from the current speed to time t, the shortest distance the vehicle can cover accelerating at maximum acceleration (max acc), or the longest distance it covers braking with maximum braking force (max dec).
It assumes constant vehicle acceleration, and both acceleration and deceleration processes are ideal.

### UpdateBlockingInfo
According to the dynamics formulas, it calls GetVehicleDynamicsLimits to compute the vehicle's upper and lower limits under dynamics, and then updates the vehicle's upper and lower limits according to the passed-in upper and lower limits, mainly including the maximum and minimum values of v0 and s0.

4) st_guide_line
### Init
Generates guideline_speed_data_ from the input reference trajectory and the desired speed. This reference-line speed data stores the speed information on the guideline.

### GetGuideSFromT
Computes s from the information in guideline_speed_data_. If the computed time is less than TotalTime, s is obtained directly from the information; if the computed time is greater than totaltime, s is extrapolated using v0

### UpdateBlockingInfo
Updates s0 and t0 according to the passed-in t


## Directory Structure 
```shell
modules/planning/tasks/st_bounds_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── st_bounds_decider.cc
├── st_bounds_decider.h
├── st_driving_limits.cc
├── st_driving_limits.h
├── st_guide_line.cc
├── st_guide_line.h
├── st_obstacles_processor.cc
├── st_obstacles_processor.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── st_bounds_decider.proto
└── README.md

```

## Module

### StBoundsDecider plugin
apollo::planning::StBoundsDecider

#### Configuration files and configuration items
| File path | Type/Structure | Description |
| ---- | ---- | ---- |
| `modules/planning/tasks/st_bounds_decider/conf/default_conf.pb.txt` | `apollo::planning::STBoundsDeciderConfig` | Default configuration file of StBoundsDecider |
|`modules/common/data/vehicle_param.pb.txt`|`apollo::common::VehicleConfig`|Vehicle chassis configuration file|

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the StBoundsDecider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `StBoundsDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `StBoundsDecider`.
```
task {
  name: "ST_BOUNDS_DECIDER"
  type: "StBoundsDecider"
}
```
##### Configuring StBoundsDecider parameters
In `modules/planning/tasks/st_bounds_decider/conf/default_conf.pb.txt`, configure the parameters of the `StBoundsDecider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `StBoundsDecider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
