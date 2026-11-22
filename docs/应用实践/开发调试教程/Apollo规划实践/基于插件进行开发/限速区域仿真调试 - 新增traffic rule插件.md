## Practice Content

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_e4e2b74.png)

Taking the example of a vehicle driving into an intersection that requires a speed limit, this tutorial introduces the Planning module plugin mechanism and shows you how to create a custom plugin for this intersection speed-limit scenario to enforce a speed limit in a specified region. The process of creating a new traffic rule plugin is shown in the figure below. It mainly consists of configuring the plugin-related files and adding the new plugin to `traffic_rule_config.pb.txt`.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_4266c80.png)

In Apollo 9.0, the traffic rule plugin configuration determines which traffic rule plugins planning enables. All business logic of the traffic rule plugins takes effect in every scenario. In this experiment, you will add a traffic rule plugin so that the autonomous vehicle drives below a specified speed when it enters the speed-limit region of an intersection.

## Practice Objectives

Learn the process of adding a new traffic rule plugin and master the steps for developing one, so that you can create custom plugins. In this experiment, you will learn how to:

- a. Create the plugin folder `region_speed_limit` at the specified location.

- b. Configure the plugin files and `traffic_rule_config.pb.txt` as required.
  
  - Configure the `RegionSpeedLimit` class code files and the corresponding BUILD file,
  
  - Configure the parameter file and its BUILD file,
  
  - Configure the plugin parameter file `default_conf.pb.txt`,
  
  - Configure `cyberfile.xml`,
  
  - Configure `plugins.xml`,
  
  - Add the new plugin to `traffic_rule_config.pb.txt`.

- c. Compile the `region_speed_limit` plugin.

- d. Run the vehicle planning module and observe the speed change as the vehicle enters the intersection in dreamview+.

## Start the Simulation Environment

1. Enter the following command in the terminal to start dreamview+.
   
   ```bash
   aem bootstrap start --plus
   ```
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_e831cb0.png)

2. After it runs successfully, click the dreamview+ button in the upper left corner.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_53acd5f.png)

3. When entering the Dreamview+ interface for the first time, check **Accept the User Agreement and Privacy Policy** and click **Enter this mode** to enter Dreamview+.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_fa209e0.png)

4. Click **Skip** in the red box to skip the introduction.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_65dbda8.png)

5. In **Mode**, select the **PnC** mode.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_6528f43.png)

6. In **Operations**, select **Sim Control**.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_bc5b24d.png)

7. In **Modules**, turn on the **Planning** module.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_1cdb2b2.png)

8. In the **Vehicle Visualization** panel, click **Routing Editing** to enter the route editing interface.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_144eb36.png)

9. Click the first red box in the upper left corner to select the vehicle's starting point, and the second red box to select the waypoints and the end point.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_938519d.png)

10. Click the red box in the lower left corner to start the simulation.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_8a83620.png)

11. As shown in the figure, the red line is the road reference line, and the light blue trajectory is the trajectory planned in real time by the planning module.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_5760e6c.png)

## Global Configuration Practice Procedure

### Synchronize Global Configuration Parameters

Enter the global configuration synchronization command. The system will automatically copy the global configuration parameters into the default directory of the profile, after which you can easily modify the configuration parameters in the profile directory.

```bash
buildtool profile config init --package planning --profile=default && aem profile use default
```

### Practice: Create a New traffic rule Plugin

#### Create the Plugin Files

Enter the following command to create the plugin folder `region_speed_limit` under `modules/planning/traffic_rules/`:

```bash
buildtool create --template plugin \
                 --namespaces planning \
                 --includes "modules/common/status/status.h" \
                     "modules/planning/planning_base/traffic_rules_base/traffic_rule.h" \
                     "modules/planning/traffic_rules/region_speed_limit/proto/default_conf.pb.h" \
                 --dependencies planning:planning \
                 --build_dependencies "//modules/planning/planning_base:apollo_planning_planning_base" \
                 --base_class_name apollo::planning::TrafficRule \
                 --config_message_name RegionSpeedLimitConfig \
                 --config_file_name default_conf \
                 modules/planning/traffic_rules/region_speed_limit
```

Enter the following command to rename the configuration file to `default_conf.pb.txt`:

```bash
mv modules/planning/traffic_rules/region_speed_limit/conf/region_speed_limit.pb.txt modules/planning/traffic_rules/region_speed_limit/conf/default_conf.pb.txt
```

#### Configure the Plugin Files

1. Open the online editor.
   
   Click the position shown in the red box to open the online editor and edit the plugin files.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_9f3bcc8.png)

2. Configure the editor.
   
   Click in order from left to right as shown in the red boxes, select the `/apollo_workspace/` directory, and click **OK**.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_2561ad7.png)
   
   After entering, the folder in the red box below is the newly created plugin template.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_df89756.png)
   
   region_speed_limit.h
   
   Location of the region_speed_limit.h file:
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3d035e7.png)
   
   Content of the region_speed_limit.h file:
   
   ```bash
   #pragma once
   
    #include <memory>
    #include "cyber/plugin_manager/plugin_manager.h"
   
    /* Added the corresponding header files */
    #include "modules/common/status/status.h"
    #include "modules/planning/traffic_rules/region_speed_limit/proto/region_speed_limit.pb.h"
    #include "modules/planning/planning_base/traffic_rules_base/traffic_rule.h"
   
    namespace apollo {
    namespace planning {
   
    class RegionSpeedLimit : public TrafficRule {
        /* Declare member functions */
        public:
            bool Init(const std::string& name, const std::shared_ptr<DependencyInjector>& injector) override;
            virtual ~RegionSpeedLimit() = default;
   
            common::Status ApplyRule(Frame* const frame, ReferenceLineInfo* const reference_line_info);
   
            void Reset() override {}
   
        private:
            RegionSpeedLimitConfig config_;
    };
   
    CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::planning::RegionSpeedLimit, apollo::planning::TrafficRule)
   
    }  // namespace planning
    }  // namespace apollo
   ```
   
   region_speed_limit.cc
   
   Location of the region_speed_limit.cc file:
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a6f122e.png)
   
   Content of the region_speed_limit.cc file:
   
   ```bash
   #include <memory>
    #include "modules/planning/traffic_rules/region_speed_limit/region_speed_limit.h"
   
    namespace apollo {
    namespace planning {
   
    /* Define member functions */
   
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
        const std::vector<PathOverlap>& junction_overlaps
                = reference_line_info->reference_line().map_path().junction_overlaps();
        for (const auto& junction_overlap : junction_overlaps) {
            reference_line->AddSpeedLimit(
                    junction_overlap.start_s - config_.forward_buffer(),
                    junction_overlap.end_s + config_.backward_buffer(),
                    config_.limit_speed());
        }
        return Status::OK();
    }
   
    }  // namespace planning
    }  // namespace apollo
   ```
   
   plugin_region_speed_limit_description.xml
   
   Location of the plugin_region_speed_limit_description.xml file:
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_dde873a.png)
   
   Content of the plugin_region_speed_limit_description.xml file
   
   ```bash
   <library path="modules/planning/traffic_rules/region_speed_limit/libregion_speed_limit.so">
        <class type="apollo::planning::RegionSpeedLimit" base_class="apollo::planning::TrafficRule">    </class>
    </library>
   ```
   
   cyberfile.xml
   
   Location of the cyberfile.xml file
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_56c1a2e.png)
   
   Content of the cyberfile.xml file
   
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
   
   BUILD
   
   Location of the BUILD file
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_b4ab4cd.png)
   
   Content of the BUILD file
   
   ```bash
   load("//tools:apollo_package.bzl", "apollo_cc_test", "apollo_package", "apollo_plugin")
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
            "//modules/planning/planning_base:apollo_planning_planning_base",
            "//modules/planning/traffic_rules/region_speed_limit/proto:region_speed_limit_cc_proto",
        ],
    )
   
    apollo_package()
   
    cpplint()
   ```
   
   proto/BUILD
   
   Location of the proto/BUILD file
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ee6dea6.png)
   
   Content of the proto/BUILD file
   
   ```bash
   load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
   load("//tools:apollo_package.bzl", "apollo_cc_library", "apollo_package", "apollo_plugin")
   load("//tools/proto:proto.bzl", "proto_library")
    load("//tools:cpplint.bzl", "cpplint")
   
    package(default_visibility = ["//visibility:public"])
   
    proto_library(
        name = "region_speed_limit_proto",
        srcs = ["region_speed_limit.proto"],
    )
   
    apollo_package()
   
    cpplint()
   ```
   
   proto/region_speed_limit.proto
   
   Location of the proto/region_speed_limit.proto file
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_423f795.png)
   
   Content of the proto/region_speed_limit.proto file
   
   ```bash
   syntax = "proto2";
   
    package apollo.planning;
   
    message RegionSpeedLimitConfig {
      // Declare the data structures in RegionSpeedLimitConfig
      optional double forward_buffer = 1 [default = 3];
      optional double backward_buffer = 2 [default = 2];
      optional double limit_speed = 3 [default = 5];
    }
   ```
   
   conf/default_conf.pb.txt
   
   Location of the conf/default_conf.pb.txt file
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_de53185.png)
   
   Modify the content of the `conf/default_conf.pb.txt` file as follows:
   
   ```bash
   forward_buffer: 3.0
   backward_buffer: 2.0
   limit_speed: 10.0
   ```

3. Configure the plugin pipeline file.
   
   Location of the configuration file
   
   ![Add traffic rule plugin.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/%E6%96%B0%E5%A2%9Etraffic%20rule%E6%8F%92%E4%BB%B6_bcf2f04.png)
   
   Content of the configuration file
   
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
      name: "REGION_SPEED_LIMIT"
      type: "RegionSpeedLimit"
    }
   ```

#### Compile the Plugin

Enter the following command to compile:

```bash
buildtool build -p modules/planning/traffic_rules/region_speed_limit/
```

#### Restart planning

In **Modules**, click the Planning module to turn Planning off, then click the Planning module again to turn it back on.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c59210d.png)

Open the **Routing Editing** panel and set the start and end points:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_cdebe8b.png)

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c6a39be.png)

Simulation with the original configuration:

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_d24e1f6.png)

Modify the plugin configuration parameters:

Find the configuration file `modules/planning/traffic_rules/region_speed_limit/conf/default_conf.pb.txt` in the `apollo_workspace` working directory and adjust the speed through the intersection.

1. Use the online editing tool to modify the configuration file `modules/planning/traffic_rules/region_speed_limit/conf/default_conf.pb.txt`, changing the `limit_speed` parameter
   
   ```bash
   limit_speed: 10.0
   ```
   
   Change the value of the parameter to:
   
   ```bash
   limit_speed: 3.0
   ```
   
   After modifying, save the code, then recompile the plugin:
   
   ```bash
   buildtool build -p modules/planning/traffic_rules/region_speed_limit/
   ```

2. After modifying the parameters and recompiling, restart the Planning module in **Modules** (a required step).

3. Adjust the plugin parameters again and observe the vehicle's deceleration effect and the start and end points of the deceleration.
   
   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_fd85ca7.png)
