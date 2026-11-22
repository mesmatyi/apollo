planning-task-rule-based-stop-decider
============

## Introduction

The `RuleBasedStopDecider` task is used to produce rule-based stopping strategies
### Module flow

1. When overtaking by borrowing the oncoming lane, check whether there is a perception blind spot, and if so, produce a stop decision
  ```c++
    StopOnSidePass(frame, reference_line_info);
  ```
  2. When changing lanes, produce a stop decision if the lane change has not succeeded when approaching the lane change end point
   ```c++
    if (config_.enable_lane_change_urgency_checking()) {
    CheckLaneChangeUrgency(frame);
  }
   ```
  3. Produce a stop decision at the path end point
   ```c++
     AddPathEndStop(frame, reference_line_info);
   ```
  
## Directory Structure

```shell

modules/planning/tasks/rule_based_stop_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── plugins.xml
├── proto
│   ├── BUILD
│   └── rule_based_stop_decider.proto
├── README.md
├── rule_based_stop_decider.cc
└── rule_based_stop_decider.h

```

## Module

### RuleBasedStopDecider plugin

apollo::planning::RuleBasedStopDecider

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/rule_based_stop_decider/conf/default_conf.pb.txt` | apollo::planning::RuleBasedStopDeciderConfig | Default configuration file of RuleBasedStopDecider |
| `modules/planning/planning_component/conf/planning_config.pb.txt`                 | `apollo::planning::PlanningConfig`              | Configuration file of the planning component               |
|`modules/common/data/vehicle_param.pb.txt`|`apollo::common::VehicleConfig`|Vehicle chassis configuration file|

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           | 
 
#### Module parameters
  
   The algorithm parameter configuration is defined in modules/planning/tasks/rule_based_stop_decider/proto/rule_based_stop_decider.proto
   
   | max_adc_stop_speed                  | Speed for judging that the ego vehicle has stopped     |
   | ----------------------------------- | -------------------- |
   | max_valid_stop_distance             | Stopping distance             |
   | search_beam_length                  | Blind spot search distance         |
   | search_beam_radius_intensity        | Blind spot search density         |
   | search_range                        | Blind spot range             |
   | is_block_angle_threshold            | Threshold for judging a blind spot         |
   | approach_distance_for_lane_change   | Distance to the lane change end point       |
   | urgent_distance_for_lane_change     | Stopping distance before the lane change end point   |
   | enable_lane_change_urgency_checking | Whether to produce a lane change stop decision |
   | short_path_length_threshold         | Path length threshold         |

#### Usage

##### Configuring the RuleBasedStopDecider Task plugin to load

    In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `RuleBasedStopDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `RuleBasedStopDecider`.
    ```
    task {
      name: "RULE_BASED_STOP_DECIDER"
      type: "RuleBasedStopDecider"
    }
      ```
