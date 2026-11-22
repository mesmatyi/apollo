planning-task-rss-decider
==============

## Introduction
`RSS Decider` is the RSS decider. RSS (responsibility sensitive safety) is used to determine whether the plan produced by planning is safe

### rss_config_default_dynamics
Sets some of the default RSS parameters needed

### rss_create_ego_object
Creates the ego vehicle object and assigns values to it

### rss_create_other_object
Creates the objects of other vehicles/traffic participants and assigns values to them

### rss_dump_world_info
Prints some RSS-related information

### Process
This function implements the RSS strategy as follows:
1) Initialize the front, rear, left, and right boundaries of the ego vehicle's bounding box, and initialize the nearest obstacle boundaries (all 0) 

2) Traverse the obstacles in the obstacle list, ignoring virtual obstacles, obstacles not on the road, and backward obstacles, and compute the nearest forward obstacle along the s direction.

3) If the distance to the nearest forward obstacle is greater than or equal to rss_max_front_obstacle_distance, it is judged RSS-safe, and the front, left, and right maximum and minimum distances in rssinfo are set.

4) If the above safe condition is not met, the following logic starts

4.1 Record some data related to the obstacle and the ego vehicle and add it to rss_info, for convenient log output later.
4.2 Add the leading Object and followingObject information to the worldModel, for later judgments.
4.3 Call RssSituationExtraction, checkSituations, provideProperResponse,
transformProperResponse, and calculateSafeLongitudinalDistanceSameDirection respectively to evaluate the result, return the result, and output logs.



## Directory Structure 
```shell
modules/planning/tasks/rss_decider/
├── BUILD
├── conf
│   └── default_conf.pb.txt
├── cyberfile.xml
├── rss_decider.cc
├── rss_decider.h
├── plugins.xml
├── proto
│   ├── BUILD
│   └── rss_decider.proto
└── README.md

```

## Module

### RssDeciderh plugin
apollo::planning::RssDecider

#### Configuration files and configuration items
| File path | Type/Structure | <div style="width: 300pt">Description</div> |
| ---- | ---- | ---- |
| `modules/planning/tasks/rss_decider/conf/default_conf.pb.txt` | apollo::planning::RssDeciderConfig | Configuration file of RssDecider |

#### Flags

| File path                                            |  <div style="width: 300pt">Description</div> |
| --------------------------------------------------- |  ------------------------------------ |
| `modules/planning/planning_component/conf/planning.conf` |  Flag configuration file of the planning module           |

#### Usage
##### Configuring the RssDecider Task plugin to load
In `modules/planning/scenarios/xxxx/conf/pipeline.pb.txt`, add the corresponding configuration in the scenario xxxx in which you want to add the `RssDecider` plugin. In the configuration parameters, `name` is the name of the task, which is user-defined and only needs to make clear which task it is, and `type` is the class name of the task, that is, `RssDecider`.
```
task {
  name: "RSS_DECIDER"
  type: "RssDecider"
}
```
##### Configuring RssDecider parameters
In `modules/planning/tasks/rss_decider/conf/default_conf.pb.txt`, configure the parameters of the `RssDecider` plugin.
In `modules/planning/planning_component/conf/planning.conf`, configure the gflag parameters that act on the `RssDecider` plugin.
##### Starting planning
```shell
mainboard -d modules/planning/planning_component/dag/planning.dag
```
