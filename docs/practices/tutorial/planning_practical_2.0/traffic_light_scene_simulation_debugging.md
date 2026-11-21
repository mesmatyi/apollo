# Traffic Light Scenario Simulation Debugging

If the existing functionality and business logic of apollo fully fit your use case, for example you only want point-to-point driving with an adjusted cruise speed, or a lower desired speed when turning, you do not need to develop any code and can meet the scenario's functional requirements just by adjusting the planning module's configuration parameters.

## Configuration parameters overview

In Apollo's Planning 2.0, we made important updates to the configuration parameters to improve the flexibility and applicability of the system. The main updates are as follows:

- Configuration parameter split: we subdivided the configuration parameters of version 1.0, splitting the original configuration parameters into two major categories: global parameters (planning_base) and local parameters (scenarios, tasks, traffic_rules). Global parameters contain the basic parameter settings of the planning module, while local parameters provide specific configuration for different scenarios and tasks.
- Switching among multiple sets of configuration parameters: to suit different application needs, we introduced a way of switching among multiple sets of configuration parameters. Users can define one or more sets of configuration parameters to meet the requirements of different scenarios and tasks.

Planning parameter configuration is divided into global parameters and local parameters. Global parameters are the parameters used in the basic planning flow and basic algorithms; they are defined and configured in the `planning_base` folder and include two configuration file types, `gflag` and `protobuf`. Local parameters are defined and configured in the planning plugins and only affect the plugin they belong to.

The configuration parameter categories are as follows:

After the changes, the configuration parameter file directories can be divided into global configuration parameters and local configuration parameters. Whether global or local, their configuration directories can be divided into two kinds: one uses `proto` files for definition, defining the data structures of the configuration parameters; the other uses `conf` files for configuration, containing the actual configuration parameters.

![download.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/%E4%B8%8B%E8%BD%BD_114d038.png)

The structure of the planning parameter configuration is as follows:

### Global configuration parameters

<table><thead><tr><th>Global configuration parameters</th><th>File directory structure</th><th>File</th><th></th></tr></thead><tbody><tr><td rowspan="9">planning_base</td><td rowspan="3">proto</td><td>planning_config.proto</td><td>Definition of planning global configuration parameters</td></tr><tr><td>traffic_rule_config.proto</td><td>Definition of the traffic_rule enabled-list configuration</td></tr><tr><td>Other basic algorithm configuration parameters.proto</td><td>Definition of other algorithm configuration parameters</td></tr><tr><td rowspan="4">conf</td><td>planning.conf</td><td>Global flag variables</td></tr><tr><td>planning_config.pb.txt</td><td>Planning global configuration parameters</td></tr><tr><td>traffic_rule_config.pb.txt</td><td>Selection of the traffic_rule generation list</td></tr><tr><td>Other basic algorithm configuration parameters.pb.txt</td><td>Definition of other algorithm configuration parameters</td></tr><tr><td>common</td><td>planning_gflags.h</td><td>Definition of global flag variables</td></tr><tr><td>scenario_base</td><td>proto/scenario_pipeline.proto</td><td>Definition of the scenario pipeline</td></tr></tbody></table>

### Local configuration parameters: scenarios

<table><thead><tr><th>Local configuration parameters</th><th>Scenario</th><th>File directory structure</th><th></th><th></th><th></th></tr></thead><tbody><tr><td rowspan="11">scenarios</td><td rowspan="6">park_and_go</td><td>proto</td><td>park_and_go.proto</td><td>Scenario: definition of the configuration parameters of the park_and_go scenario</td><td></td></tr><tr><td rowspan="5">conf</td><td>pipeline.pb.txt</td><td>Scenario Pipeline: Pipeline parameter configuration of the park_and_go scenario</td><td></td></tr><tr><td>scenario_conf.pb.txt</td><td>Scenario: configuration parameters of the park_and_go scenario</td><td></td></tr><tr><td rowspan="2">park_and_go_adjust<br>Stage directory:<br>task configuration parameters of the park_and_go just&nbsp;&nbsp;stage</td><td>open_space_roi_decider.pb.txt</td><td>Task: configuration parameters of the open_space_roi_decider&nbsp;&nbsp;task</td></tr><tr><td>open_space_trajectory_provider.pb.txt</td><td>Task: configuration parameters of open_space_trajectory_provider</td></tr><tr><td>park_and_go_check<br>Stage directory: task configuration parameters of the park_and_go_check stage</td><td>open_space_roi_decider.pb.txt</td><td>Task: configuration parameters of the open_space_roi_decider task</td></tr><tr><td rowspan="2">lane_follow</td><td>proto</td><td>Scenario: definition of the configuration parameters of lane_follow</td><td></td><td></td></tr><tr><td>conf</td><td>Scenario: configuration parameters of lane_follow</td><td></td><td></td></tr><tr><td rowspan="2">bare_intersection_unprotected</td><td>proto</td><td>Scenario: definition of the configuration parameters of bare_intersection_unprotected</td><td></td><td></td></tr><tr><td>conf</td><td>Scenario: configuration parameters of bare_intersection_unprotected</td><td></td><td></td></tr><tr><td>…</td><td></td><td></td><td></td><td></td></tr></tbody></table>

### Local configuration parameters: tasks

<table><thead><tr><th>Local configuration parameters</th><th>Scenario</th><th>File directory structure</th><th></th><th></th></tr></thead><tbody><tr><td rowspan="7">tasks</td><td rowspan="2">lane_borrow_path<br>task default configuration</td><td>proto</td><td>lane_borrow_path.proto</td><td>Definition of the configuration parameters of the lane_borrow_path task</td></tr><tr><td>conf</td><td>default_conf.pb.txt</td><td>Configuration parameters of the lane_borrow_path task</td></tr><tr><td rowspan="2">open_space_roi_decider<br>task default configuration</td><td>proto</td><td>open_space_roi_decider.proto</td><td>Definition of the configuration parameters of open_space_roi_decider</td></tr><tr><td>conf</td><td>default_conf.pb.txt</td><td>Configuration parameters of open_space_roi_decider</td></tr><tr><td rowspan="2">path_decider<br>task default configuration</td><td>proto</td><td>path_decider.proto</td><td>Definition of the configuration parameters of path_decider</td></tr><tr><td>conf</td><td>default_conf.pb.txt</td><td>Configuration parameters of path_decider</td></tr><tr><td>…</td><td></td><td></td><td></td></tr></tbody></table>

### Local configuration parameters: traffic_rules

<table><thead><tr><th>Local configuration parameters</th><th>Scenario</th><th>File directory structure</th><th></th><th></th></tr></thead><table><thead><tr><th>Local configuration parameters</th><th>Scenario</th><th>File directory structure</th><th></th><th></th></tr></thead><tbody><tr><td rowspan="7">traffic_rules</td><td rowspan="2">baskside_vehicle<br>task default configuration</td><td>proto</td><td>backside_vihicle.proto</td><td>Definition of the configuration parameters of backside_vihicle</td></tr><tr><td>conf</td><td>default_conf.pb.txt</td><td>Configuration parameters of backside_vihicle</td></tr><tr><td rowspan="2">crosswalk<br>traffic_rules default configuration</td><td>proto</td><td>crosswalk.proto</td><td>Definition of the configuration parameters of crosswalk</td></tr><tr><td>conf</td><td>default_conf.pb.txt</td><td>Configuration parameters of crosswalk</td></tr><tr><td rowspan="2">keepclear<br>traffic_rules default configuration</td><td>proto</td><td>backside_vihicle.proto</td><td>Definition of the configuration parameters of keepclear</td></tr><tr><td>conf</td><td>keepclearpb.txt</td><td>Configuration parameters of keepclear</td></tr><tr><td>…</td><td></td><td></td><td></td></tr></tbody></table>

## Planning_base parameter configuration

Planning_base: global configuration parameters.

### Overview

Global configuration parameters are parameters that participate in everything in planning. These parameters have a global effect on the whole planning process and determine the basic behavior and strategy of the planning module. Global configuration parameters are common throughout the planning system and do not change with different scenarios or tasks, so they apply to all planning tasks.

The corresponding configuration file is in `modules/planning/planning_base/conf/planning.conf`,
and the configuration file corresponding to navi mode is in `modules/planning/planning_base/conf/planning_navi.conf`.

### Configuration process

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_2474d30.png)

If developers have specific modification requirements for the configuration parameters, they only need to enter the sync command, and the system will automatically copy the global configuration parameters into the default directory of the profile; the configuration parameters can then be easily modified in the profile directory.

```bash
buildtool profile config init --package planning --profile=default
```

Use the configuration in the default directory

```bash
# Use the configuration named default
aem profile use default
```

Directory structure of the profile plugin

```bash
profiles/default/modules/planning/planning_base/
|-- conf
|   |-- discrete_points_smoother_config.pb.txt
|   |-- planner_open_space_config.pb.txt
|   |-- planning.conf
|   |-- planning_config.pb.txt
|   |-- planning_config_navi.pb.txt
|   |-- planning_navi.conf
|   |-- planning_semantic_map_config.pb.txt
|   |-- qp_spline_smoother_config.pb.txt
|   |-- spiral_smoother_config.pb.txt
|   `-- traffic_rule_config.pb.txt
|-- dag
|-- launch
`-- testdata
```

### Practice

Impact of global configuration parameters on vehicle planning:

In this practice case, we explore the impact of global configuration parameters on vehicle speed planning, speed bump handling and detour distance planning by adjusting different configuration parameters. We use planning2.0 for parameter configuration and planning, and observe the vehicle planning results through Apollo's dreamview+.

**Practice content**:

- Global speed configuration practice:
  - Goal: adjust the global configuration parameters so that the maximum driving speed of the vehicle does not exceed 10 m/s.
  - Method: modify the global speed configuration parameter in planning2.0.
  - Result: view the vehicle's planned speed in real time on dreamview+ to verify whether it meets the requirement.
- Speed bump configuration practice:
  - Goal: adjust the global configuration parameters so that the maximum driving speed of the vehicle when passing a speed bump does not exceed 4 m/s.
  - Method: set different speed bump configuration parameters in planning2.0.
  - Result: observe the vehicle's planned path and speed in real time through dreamview+ to verify whether the requirement is met.
- Detour distance configuration practice:
  - Goal: adjust the global configuration parameters so that the vehicle keeps a distance of 1.0 m from an obstacle when it encounters one.
  - Method: change the detour distance configuration parameter in planning2.0.
  - Result: view the vehicle's detour path and the distance to the obstacle in real time on dreamview+ to verify whether it meets the requirement.

**Practice purpose**:

Explore the impact of global configuration parameters on vehicle planning. We focus on the global speed configuration, speed bump configuration and detour distance configuration, and observe the effects on the planned vehicle speed, speed bump handling and detour path planning respectively.

**Practice process**:

1. Planning results with the default configuration parameters.

   Start dreamview and observe the simulation results of vehicle speed planning, speed bump handling and detour distance planning under the default configuration parameters.

2. Global speed configuration practice:

   a. Find the files related to the global speed configuration in planning2.0.

   b. According to the requirements, adjust the global speed configuration parameter to ensure that the maximum driving speed of the vehicle does not exceed 10 m/s.

   c. Save the modified configuration parameters and restart the planning module.

   d. Run the vehicle planning module and observe the change in the vehicle's planned speed on dreamview+.

3. Speed bump configuration practice:

   a. Find the files related to the speed bump configuration in planning2.0.

   b. Set different speed bump configuration parameters to ensure that the maximum driving speed of the vehicle when passing a speed bump does not exceed 4 m/s.

   c. Save the modified configuration parameters and rebuild the planning2.0 module.

   d. Run the vehicle planning module and observe in real time on dreamview the vehicle's passing speed and planned path when it encounters a speed bump.

4. Detour distance configuration practice:

   a. Find the files related to the detour distance configuration in planning2.0.

   b. Change the detour distance configuration parameter to ensure that the vehicle keeps a distance of 1.0 m from an obstacle when it encounters one.

   c. Save the modified configuration parameters and rebuild the planning2.0 module.

   d. Run the vehicle planning module and view in real time on dreamview the vehicle's detour path and the distance to the obstacle.

#### Planning results with the default configuration parameters

After starting dreamview, select these three scenarios and simulate them in turn.

![image (11).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2811%29_a2466e7.png)

#### Global speed configuration practice

In the global configuration parameters `profiles/default/modules/planning/planning_base/conf/planning.conf`, find the configuration parameter of the global speed:

```bash
--default_cruise_speed=1.5
```

Modify the value of the configuration parameter `profiles/default/modules/planning/planning_base/conf/planning.conf` to:

```bash
--default_cruise_speed=10.0
```

After modifying and saving the code, run the simulation.

**Experimental results**:

|                  | Original                                                                                                                    | Modified                                                                                                                  |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Global speed configuration practice | ![image (13).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2813%29_f54be06.png) | ![image (14).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2814%29_537512d.png) |

#### Speed bump configuration practice

In the global configuration parameters `profiles/default/modules/planning/planning_base/conf/planning.conf`, find the speed bump configuration parameters:

```bash
--speed_bump_speed_limit=1.5
```

Modify the value of the configuration parameter `profiles/default/modules/planning/planning_base/conf/planning.conf` to:

```bash
--speed_bump_speed_limit=4.0
```

After modifying and saving the code, run the simulation.

**Experimental results**:

|                | Original                                                                                                                    | Modified                                                                                                                  |
| -------------- | ----------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Speed bump configuration practice | ![image (15).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2815%29_972db7f.png) | ![image (16).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2816%29_7498852.png) |

#### Detour distance configuration practice

In the global configuration parameters `profiles/default/modules/planning/planning_base/planning.conf`, find the speed bump configuration parameters

```bash
--obstacle_lat_buffer=0.3
```

Modify the value of the configuration parameter `profiles/default/modules/planning/planning_base/planning.conf` to:

```bash
--obstacle_lat_buffer=1.0
```

After modifying and saving the code, run the simulation:

|                | Original                                                                                                                    | Modified                                                                                                                  |
| -------------- | ----------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Speed bump configuration practice | ![image (29).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2829%29_96827fd.png) | ![image (30).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2830%29_3ebcb65.png) |

## traffic rules parameter configuration

traffic rules: local configuration parameters

### Overview

Traffic rules implement rules that take effect in all scenarios. Users can decide which traffic rules to enable according to their needs. The corresponding configuration file is in `modules/planning/planning_base/conf/traffic_rule_config.pb.txt`:

```bash
rule {
  name: "BACKSIDE_VEHICLE"
  type: "BacksideVehicle"
}
rule {
  name: "CROSSWALK"
  type: "Crosswalk"
}
rule {
  name: "DESTINATION"
  type: "Destination"
}
rule {
  name: "KEEP_CLEAR"
  type: "KeepClear"
}
rule {
  name: "STOP_SIGN"
  type: "StopSign"
}
```

> Note: the "name" in a rule is an alias for the scenario, which users can customize and name in uppercase letters; "type" is the class name of the corresponding plugin class, omitting `namespace apollo::planning`.

Each traffic rule also has its own traffic rule parameters, located in the `conf` directory of the traffic rule plugin. For example, the crosswalk rule parameters are located in `modules/planning/traffic_rules/crosswalk/conf/default_conf.pb.txt`.

```bash
stop_distance: 1.0
max_stop_deceleration: 4.0
min_pass_s_distance: 1.0
expand_s_distance: 2.0
stop_strict_l_distance: 4.0
stop_loose_l_distance: 5.0
start_watch_timer_distance:10
stop_timeout: 10.0
```

### Configuration process

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_4607852.png)

If developers have specific modification requirements for the configuration parameters, they only need to enter the sync command, and the system will automatically copy the global configuration parameters into the `default` directory of the `profile`; the configuration parameters can then be modified in the `profile` directory.

```bash
buildtool profile config init --package planning-traffic-rules-traffic-light --profile=default
```

View the profile plugin directory structure:

```bash
tree profiles/default/modules/planning/traffic_rules/
```

Directory structure:

```bash
profiles/default/modules/planning/traffic_rules/
`-- traffic_light
    `-- conf
        `-- default_conf.pb.txt
```

### Practice

Impact of Traffic Rules configuration parameters on vehicle planning:

In this practice case, we explore the impact of Traffic Rules configuration parameters on the vehicle's stopping distance when it encounters a traffic light by adjusting different configuration parameters. We use planning2.0 for parameter configuration and planning, and observe the vehicle planning results through Apollo's Dreamview.

**Practice content:**

Local configuration parameters: traffic light scenario configuration practice

- Goal: by adjusting the Traffic Rules configuration parameters, when the ego vehicle detects a red light ahead, it can accurately stop at a position 2 m to 2.5 m from the stop line without crossing it. After the traffic light turns green, the ego vehicle can pass smoothly through the traffic light intersection.
- Method: find the files related to the Traffic Rules configuration in planning2.0 and adjust the configuration parameters to meet the requirements of the traffic light scenario.
- Result: in Apollo's Dreamview simulation environment, observe the driving of the ego vehicle in the traffic light scenario to verify whether it can stop at a suitable position at a red light and pass through the traffic light intersection after the light turns green.

**Practice purpose:**

Explore the impact of Traffic Rules configuration parameters on vehicle planning. Observe the vehicle's stopping behavior in different situations, and explore how to optimize the traffic rule processing module to achieve more precise and safer vehicle planning.

**Practice process:**

- Planning results with the default configuration parameters.

  Start dreamview and observe the simulation results of the vehicle encountering a traffic light under the default configuration parameters.

- Traffic light scenario practice.

  a. Find the files related to the Traffic Rules configuration in planning2.0.

  b. According to the requirements, adjust the Traffic Rules configuration parameters to ensure that the vehicle stops at a position 2 m to 2.5 m from the stop line.

  c. Save the modified configuration parameters and restart the planning module.

  d. Run the vehicle planning module and observe in dreamview the change in the vehicle's stopping when it encounters the traffic light scenario.

#### Modifying the configuration parameters for the traffic light scenario practice

In the Traffic Rules parameters `profiles/default/modules/planning/planning_base/conf/planning.conf`, find the configuration parameter of the traffic light stopping distance:

```bash
stop_distance: 1.0
```

Modify the default configuration parameters `profiles/default/modules/planning/planning_base/conf/planning.conf` to

```
bash
stop_distance: 2.0
```

**Simulation comparison:**

|                  | Original                                                                                                                    | Modified                                                                                                                  |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Detour distance configuration practice | ![image (18).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2818%29_2c431fa.png) | ![image (19).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2819%29_f34c981.png) |

## planning2.0 configuration file new features

In Apollo's planning2.0, to solve the problem that different configuration parameters may be needed for the same module in different scenarios, we introduced the profiles directory and provide the ability to view and use profile configuration files with the aem tool.

In the profiles directory, users can define one or more sets of configuration parameters to suit different application scenarios. By using aem commands, we can easily view the existing profiles and switch between different configuration parameters in different scenarios. This makes it convenient to quickly switch configurations in order to verify the impact of different parameters on system performance and effect, thereby optimizing the performance of the planning module.

**How profiles work:**

In planning2.0, we divide the configuration parameters into two major categories: global configuration parameters (components) and local configuration parameters (plugins).

When reading configuration parameters, the priority is as follows:

- Components: profile > default; this will later change to workspace source code > profile > default
- Plugins: workspace source code > profile > default

**About profiles:**

By default, the profile directory is empty and contains no configuration parameters; the profile symlink preferentially uses the configuration parameters produced by the build. To add other configuration parameters to the profile directory, you need to enter the sync command to copy the configuration parameters into the profile directory.
After executing the sync command, you will see in the profile directory the same files as the default configuration parameters. In addition, if you want to switch between different configuration parameters, you can do so by entering the switch command, which changes the target of the symlink and thereby selects a different configuration file.

**About workspace source code:**

Here "workspace source code" means that the modules folder in the working directory contains the configuration parameters of that code. This means the system preferentially reads the source code configuration parameters in the working directory, ensuring greater flexibility and customizability in selecting configuration parameters. Through this mechanism, we can adapt more flexibly to different application needs, improving the adaptability and performance of the system.

The following aem commands are used to view and switch configuration parameters:

```bash
buildtool profile config init
# View the existing profiles
aem profile list

# Use the configuration named default
aem profile use default
```

In the profiles directory, users can define one or more sets of configuration parameters. The advantages of this are:

- Scenario adaptability: by setting different configuration parameters for different scenarios, we can easily adapt to various application scenarios. Different tasks, environments or needs may require different configurations, and this flexibility improves the adaptability and performance of the system.
- Parameter saving and reuse: during parameter tuning, we can save the parameters we consider good in a folder. This lets us not only conveniently reuse previously optimized parameters, but also avoid forgetting or losing important settings in later tuning, improving work efficiency.
- Quick rollback: by saving each version of the configuration files, if later parameter adjustments turn out worse than before, we can easily switch the code back to the previously saved, better configuration files. This allows a quick rollback to the previous state and avoids unnecessary errors or performance degradation.
- Team collaboration: when multiple team members participate in the project, an independent configuration file can be created for each member, making it easy to debug and optimize individually without disturbing others' work. This helps improve team collaboration efficiency, and each member can focus on their own tasks without worrying about affecting others.

### Switching among multiple sets of configuration parameters

In Apollo's planning2.0, we introduced a way of switching among multiple sets of configuration parameters, making it possible to use different configurations for different scenarios to suit various application needs. The following describes in detail how to set up the parameter directory:

#### Parameter directory setup

First, enter the docker workspace and enter the following configuration parameter command under the `/apollo_workspace` path to copy the configuration parameters into the `profile` plugin directory.

Note: by default, the configuration parameters use the example command. The command to view the existing profile configuration:

```bash
# View the existing profiles
aem profile list
```

Configuration file directory of the profile plugin

```bash
default demo_1 demo_2
```

In this example, by executing the following command, we can see that the configuration currently in use is the `default` configuration file, and view the structure of the profiles directory under the `/apollo_workspace` path:

```bash
tree profiles
```

Structure of the profile directory under the `/apollo_workspace` path:

```bash
profiles/
├── current -> default #symlink
├── default
├── demo_1
└── demo_2
```

By using the following command, we can easily switch to the configuration parameters named `demo_1`:

```bash
# Use the configuration named default
aem profile use demo_1
```

> Note: this approach is very convenient. There is no need to handle all the configuration files manually; you only need to focus on the parts to modify, and the system intelligently handles the rest, ensuring that we can quickly adjust parameters and keep the system running normally. This configuration management approach effectively improves work efficiency and code maintainability, making the parameter tuning process more concise and flexible.
