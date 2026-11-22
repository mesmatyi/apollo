# Speed Limit Area Scenario Simulation Debugging

## Cloud experiment

This practice is now available in the Apollo Cloud Lab. Click to quick start and try it out: [Adding a traffic rule plugin](https://apollo.baidu.com/community/course/24).

## Overview of the traffic rule plugin

Planning plugins fall into three categories: scenario, task and traffic rule. The usage scenarios of each category of plugin are shown in the figure below. In this section we explain how to add a traffic rule plugin.

The roles of the three categories of plugins:

- Traffic rule handles various traffic rules and converts them into two types of output: stopping (implemented by generating virtual obstacles) and speed limiting.
- Scenario determines the scenario the vehicle is in, and then invokes a predefined combination of Tasks according to the scenario.
- Task performs specific tasks, such as generating lane borrow paths, processing obstacle boundaries and trajectory optimization.
  The invocation logic of the three is shown in the figure below.

![Screenshot 2023-08-17 21.11.56.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/%E6%88%AA%E5%B1%8F2023-08-17%2021.11.56_32fca1a.png)

The generation and invocation logic of traffic rule plugins is shown in the figure below. A traffic rule plugin inherits from the traffic rule base class, and the `traffic_decider` in `planning_base` then generates and invokes each plugin. Each time planning performs a planning task, it invokes each traffic rule through `traffic_decider`, which makes the traffic rule plugins take effect.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_e1d84b3.png)

## Process for adding and configuring a plugin

Taking the `region_speed_limit` plugin as an example, the directory structure of a complete traffic rule plugin folder and the function of each file are as follows.

Run the following command to generate the initial plugin folder at the specified location.

```bash
buildtool create --template plugin \
                 --namespaces planning \
                 --name region-speed-limit \
                 --base_class_name TrafficRule modules/planning/traffic_rules/region_speed_limit \
                 --config_message_name RegionSpeedLimitConfig
buildtool profile config init --package planning --profile=default && aem profile use default
```

The file structure under the generated plugin folder is as follows.

```bash
└── region_speed_limit
    ├── region_speed_limit.cc  //region_speed_limit plugin source code
    ├── region_speed_limit.h  //region_speed_limit plugin source code
    ├── conf
    │   └── default_conf.pb.txt  //region_speed_limit plugin parameter configuration file
    ├── proto
    │   ├── region_speed_limit.proto  //declaration file of the data structure of the region_speed_limit plugin parameter configuration
    │   └── BUILD  //declaration file of the build rules for the proto file
    ├── BUILD  //declaration file of the build rules of the region_speed_limit plugin
    ├── plugin_region_speed_limit_description.xml  //declaration file of the region_speed_limit plugin class
    └── cyberfile.xml  //declaration file of the version information of the region_speed_limit plugin
```

In the plugin directory above, the process of creating a new traffic rule plugin is shown in the figure below. It mainly consists of configuring the plugin-related files and adding the new plugin in `traffic_rule_config.pb.txt`.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image_a014ac9.png)

## Traffic rule plugin practice

**Practice content:**

Add an intersection speed limit traffic rule plugin: taking the case where the vehicle drives into an intersection that requires a speed limit while driving as an example, add an intersection speed limit traffic rule plugin to implement speed limiting in a specified area.

**Practice purpose:**

Learn the process of adding a traffic rule plugin and master the steps of developing a traffic rule plugin, so that developers have the ability to create custom plugins.

**Practice process:**

Practice of creating the intersection speed limit plugin:

a. Create the plugin folder `region_speed_limit` at the specified location.

b. According to the requirements, configure the corresponding plugin files and `traffic_rule_config.pb.txt`.

- Configure the `RegionSpeedLimit` class code files and the corresponding BUILD file

- Configure the parameter file and the BUILD file,
- Configure the plugin parameter file `default_conf.pb.txt`,
- Configure `cyberfile.xml`,
- Configure `plugins.xml`,
- Add the new plugin in `traffic_rule_config.pb.txt`.

c. Build the `region_speed_limit` plugin.

d. Run the vehicle planning module and observe in dreamview the speed change when the vehicle drives into the intersection.

### Generate the plugin file template

Enter the following command to create the plugin folder `region_speed_limit` under `modules/planning/traffic_rules/`.

```bash
buildtool create --template plugin \
                 --namespaces planning \
                 --name region-speed-limit \
                 --base_class_name TrafficRule modules/planning/traffic_rules/region_speed_limit \
                 --config_message_name RegionSpeedLimitConfig
```

### Write the RegionSpeedLimit class code files and configure the corresponding BUILD file

region_speed_limit.cc:

```bash
#include <memory>
#include "modules/planning/traffic_rules/region_speed_limit/region_speed_limit.h"

namespace apollo {
namespace planning {

/* Define member functions*/

using apollo::common::Status;
using apollo::hdmap::PathOverlap;

bool RegionSpeedLimit::Init(const std::string& name, const std::shared_ptr<DependencyInjector>& injector) {
    if (!TrafficRule::Init(name, injector)) {
        return false;
    }
    // Load the config this task.
    return TrafficRule::LoadConfig<RegionSpeedLimitConfig>(&config_);
}

Status RegionSpeedLimit::ApplyRule(Frame* const frame, ReferenceLineInfo* const reference_line_info) {
    ReferenceLine* reference_line = reference_line_info->mutable_reference_line();
    const std::vector<PathOverlap>& pnc_junction_overlaps
            = reference_line_info->reference_line().map_path().pnc_junction_overlaps();
    for (const auto& pnc_junction_overlap : pnc_junction_overlaps) {
        reference_line->AddSpeedLimit(
                pnc_junction_overlap.start_s - config_.forward_buffer(),
                pnc_junction_overlap.end_s + config_.backward_buffer(),
                config_.limit_speed());
    }
    return Status::OK();
}

}  // namespace planning
}  // namespace apollo
```

> Note:
>
> - `Init()` function: initializes the `RegionSpeedLimit` class and reads the configuration file information into `config_`;
> - `ApplyRule()` function: the interface called by the traffic rule class, the function actually called at runtime;
> - `reference_line_info->reference_line().map_path()`: obtains road information; this plugin obtains the intersection information `pnc_junction_overlaps()`;
> - For the intersection speed limit function, `ReferenceLine::AddSpeedLimit(double start_s, double end_s, double speed_limit)` is called, implementing the constraint that the maximum speed from `start_s` to `end_s` is `speed_limit`.

region_speed_limit.h:

```bash
#pragma once

#include <memory>
#include "cyber/plugin_manager/plugin_manager.h"

/* Added the corresponding header files*/
#include "modules/common/status/status.h"
#include "modules/planning/traffic_rules/region_speed_limit/proto/region_speed_limit.pb.h"
#include "modules/planning/planning_base/traffic_rules_base/traffic_rule.h"

namespace apollo {
namespace planning {

class RegionSpeedLimit : public TrafficRule {
    /* Declare member functions*/
    public:
        bool Init(const std::string& name, const std::shared_ptr<DependencyInjector>& injector) override;
        virtual ~RegionSpeedLimit() = default;

        common::Status ApplyRule(Frame* const frame, ReferenceLineInfo* const reference_line_info);

        void Reset() override {}

    private:
        RegionSpeedLimitConfig config_;
};

CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::planning::RegionSpeedLimit, TrafficRule)
}  // namespace planning
}  // namespace apollo
```

> Note: `Reset()` function: the entry for resetting plugin variables, clearing the changes made to the variables in the plugin by the previous decision.
> `CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::planning::RegionSpeedLimit,TrafficRule)`: declares the class as a plugin.

The BUILD file describes the build rules of the source code and its dependencies.

Build:

```bash
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools:apollo.bzl", "cyber_plugin_description")
load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package", "apollo_plugin")
load("//tools/proto:proto.bzl", "proto_library")
load("//tools:cpplint.bzl", "cpplint")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "region_speed_limit_files",
    srcs = glob([
        "conf/**",
    ]),
)

apollo_plugin(
    name = "libregion_speed_limit.so",
    srcs = [
        "region_speed_limit.cc",
    ],
    hdrs = [
        "region_speed_limit.h",
    ],
    description = ":plugin_region_speed_limit_description.xml",
    deps = [
        "//cyber",
        # Add the dependencies required by this plugin
        "//modules/map/pnc_map:path",
        "//modules/planning/planning_base/common:frame",
        "//modules/planning/planning_base/common:reference_line_info",
        "//modules/planning/planning_base/traffic_rules_base:traffic_rules",
        "//modules/planning/traffic_rules/region_speed_limit/proto:region_speed_limit_proto",

    ],
)

apollo_package()

cpplint()
```

> Note:
>
> - load(): declares the dependencies used in the build,
> - filegroup(): declares a file group, which is installed to the corresponding location after the build.
> - apollo_plugin(): declares the dynamic library to build, the source files to build, the plugin description file and the dependencies.

### Configure the parameter files

region_speed_limit.proto:

```bash
syntax = "proto2";

package apollo.planning;

message RegionSpeedLimitConfig {
  // Declare the data structure in RegionSpeedLimitConfig
  optional double forward_buffer = 1 [default = 3];
  optional double backward_buffer = 2 [default = 2];
  optional double limit_speed = 3 [default = 5];
}
```

> Note:
>
> - The region_speed_limit.proto file declares the data structure of this plugin's configuration file.
> - The BUILD file declares the build rules of this proto file.

### Write the plugin parameter file default_conf.pb.txt

```bash
forward_buffer: 3.0
backward_buffer: 2.0
limit_speed: 15.0
```

### Configure cyberfile.xml

```bash
<package format="2">
  <name>region-speed-limit</name>
  <version>local</version>
  <description>
    This is a demo package
  </description>

  <maintainer email="sample@sample.com">Apollo Developer</maintainer>
  <license>Apache License 2.0</license>
  <url type="website">https://www.apollo.auto/</url>
  <url type="repository">https://github.com/ApolloAuto/apollo</url>
  <url type="bugtracker">https://github.com/ApolloAuto/apollo/issues</url>

  <type>module</type>
  <src_path>//modules/planning/traffic_rules/region_speed_limit</src_path>
  <builder>bazel</builder>

  <depend type="binary" repo_name="cyber">cyber</depend>
  <!-- add new dependency-->
  <depend type="binary" repo_name="planning">planning</depend>

  <depend>bazel-extend-tools</depend>
</package>
```

> Note: declares information such as the file version.

### Add the new plugin in traffic_rule_config.pb.txt

Add the newly created plugin to the traffic rule configuration file, so that planning invokes this traffic rule.

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
  name: "REFERENCE_LINE_END"
  type: "ReferenceLineEnd"
}
rule {
  name: "REROUTING"
  type: "Rerouting"
}
rule {
  name: "STOP_SIGN"
  type: "StopSign"
}
rule {
  name: "TRAFFIC_LIGHT"
  type: "TrafficLight"
}
rule {
  name: "YIELD_SIGN"
  type: "YieldSign"
}
rule {
  name: "SPEED_SETTING"
  type: "SpeedSetting"
}
rule {
  name: "REGION_SPEED_SETTING"
  type: "RegionSpeedLimit"
}
```

### Build the plugin

Run the following command to build the plugin:

```bash
buildtool build -p modules/planning/traffic_rules/region_speed_limit/
```

### Run the scenario and debug parameters

Run the following command in the terminal to run dreamview.

```bash
aem bootstrap start
```

Set `limit_speed` in `default_conf.pb.txt` to 3, so that the speed at the intersection is limited to 3 m/s.

As shown in the figure, before reaching the intersection the speed is 23 km/h and the vehicle drives normally,

![image (21).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2821%29_394b62a.png)

After reaching the intersection, as shown in the right figure, the speed drops to 10 km/h, implementing the intersection speed limit function.
![image (22).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Doc_CN_9_0/image%20%2822%29_8703823.png)
