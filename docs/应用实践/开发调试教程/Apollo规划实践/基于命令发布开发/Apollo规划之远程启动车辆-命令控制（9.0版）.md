## Experiment Content

Monitoring and taking over autonomous vehicles through a 5G remote driving platform is an important means of ensuring safe autonomous driving. This experiment performs Sim_Control simulation based on Dreamview+ to achieve remote control of an autonomous vehicle, including operations such as start, stop and pull over.

## Experiment Objective

Learn the process of adding and using a new command-line plugin, master the steps for developing a command-line plugin, and give trainees the ability to create custom plugins.

- Method: Using the buildtool feature of Apollo 9.0, add a command-controlled plugin that implements controlling the vehicle to start, stop and pull over through the terminal.
- Result: Through command-line control, the simulation effects of the vehicle starting, stopping and pulling over can be observed in the Dreamview+ interface.

## Experiment Steps

### 1. Create the Remote Vehicle Control Plugin

1. Run the following command to create the plugin folder `remote_control_command_demo`.

   ```bash
   buildtool create --template component remote_control_command_demo
   ```

   After it succeeds, run the ls command and you can see that the `remote_control_command_demo` folder has been generated in the current directory, as shown below:

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c742c5c.png)

   View the directory structure:

   ```bash
   tree remote_control_command_demo
   ```

   Directory structure:

   ```bash
   remote_control_command_demo
    |-- BUILD
    |-- conf
    |   |-- remote_control_command_demo.conf             -----------File that loads the global parameters
    |   `-- remote_control_command_demo.pb.txt           -----------proto configuration file
    |-- cyberfile.xml                                    -----------Build loading file
    |-- dag
    |   `-- remote_control_command_demo.dag              -----------dag file for startup
    |-- launch
    |   `-- remote_control_command_demo.launch           -----------launch file for startup
    |-- proto
    |   |-- BUILD                                        -----------proto build file
    |   `-- remote_control_command_demo.proto            -----------proto definition file
    |-- remote_control_command_demo.cc                   -----------Feature implementation file
    `-- remote_control_command_demo.h                    -----------Feature definition file
   ```

2. Click the position shown in the red box to open the online editor and edit the plugin files.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_73db513.png)

3. Copy the code in the following files into the corresponding files of `remote_control_command_demo`:

   remote_control_command_demo.cc:

   ```bash
   #include "remote_control_command_demo/remote_control_command_demo.h"

    #include <poll.h>

    #include <cctype>

    #include "cyber/common/file.h"
    #include "cyber/record/record_reader.h"

    using apollo::external_command::CommandStatus;

    RemoteCotrolCommandDemo::RemoteCotrolCommandDemo() : command_id_(0), module_name_("demo") {}

    bool RemoteCotrolCommandDemo::Init() {
        action_command_client_
                = std::make_shared<apollo::common::ClientWrapper<apollo::external_command::ActionCommand, CommandStatus>>(
                        node_, "/apollo/external_command/action");
        free_space_command_client_ = std::make_shared<
                apollo::common::ClientWrapper<apollo::external_command::FreeSpaceCommand, CommandStatus>>(
                node_, "/apollo/external_command/free_space");
        lane_follow_command_client_ = std::make_shared<
                apollo::common::ClientWrapper<apollo::external_command::LaneFollowCommand, CommandStatus>>(
                node_, "/apollo/external_command/lane_follow");

        return true;
    }

    bool RemoteCotrolCommandDemo::Proc() {
        int8_t revent = 0;  // short
        struct pollfd fd = {STDIN_FILENO, POLLIN, revent};
        switch (poll(&fd, 1, 100)) {
        case -1:
            std::cout << "Failed to read keyboard" << std::endl;
            return false;
        case 0:
            return true;
        default:
            char data[50];
            std::cin.getline(data, 50);
            std::string input_command_string = data;
            if (input_command_string == "pull_over") {
                // Pull over.
                // SendActionCommand(
                //     apollo::external_command::ActionCommandType::PULL_OVER);
                auto command = std::make_shared<apollo::external_command::ActionCommand>();
                FillCommandHeader(command);
                command->set_command(apollo::external_command::ActionCommandType::PULL_OVER);
                std::cout << "Sending action command: " << command->DebugString() << std::endl;
                auto response = action_command_client_->SendRequest(command);
                if (nullptr == response) {
                    std::cout << "Command sending failed, please check the service is on!\n" << std::endl;
                } else {
                    std::cout << "******Finish sending command.******\n" << std::endl;
                }
                //
            } else if (input_command_string == "stop") {
                // Stop planning.
                // SendActionCommand(apollo::external_command::ActionCommandType::STOP);
                auto command = std::make_shared<apollo::external_command::ActionCommand>();
                FillCommandHeader(command);
                command->set_command(apollo::external_command::ActionCommandType::STOP);
                std::cout << "Sending action command: " << command->DebugString() << std::endl;
                auto response = action_command_client_->SendRequest(command);
                if (nullptr == response) {
                    std::cout << "Command sending failed, please check the service is on!\n" << std::endl;
                } else {
                    std::cout << "******Finish sending command.******\n" << std::endl;
                }
                //
            } else if (input_command_string == "start") {
                // Start planning.
                // SendActionCommand(apollo::external_command::ActionCommandType::START);
                auto command = std::make_shared<apollo::external_command::ActionCommand>();
                FillCommandHeader(command);
                command->set_command(apollo::external_command::ActionCommandType::START);
                std::cout << "Sending action command: " << command->DebugString() << std::endl;
                auto response = action_command_client_->SendRequest(command);
                if (nullptr == response) {
                    std::cout << "Command sending failed, please check the service is on!\n" << std::endl;
                } else {
                    std::cout << "******Finish sending command.******\n" << std::endl;
                }
                //
            } else if (input_command_string == "free1") {
                apollo::external_command::Pose end_pose;
                end_pose.set_x(437556.02);
                end_pose.set_y(4432540.34);
                end_pose.set_heading(1.8);
                std::vector<apollo::external_command::Point> way_points;
                apollo::external_command::Point point1;
                apollo::external_command::Point point2;
                apollo::external_command::Point point3;
                apollo::external_command::Point point4;
                point1.set_x(437536.29);
                point1.set_y(4432560.69);
                point2.set_x(437536.29);
                point2.set_y(4432510.69);
                point3.set_x(437576.29);
                point3.set_y(4432510.69);
                point4.set_x(437576.29);
                point4.set_y(4432560.69);
                way_points.emplace_back(point1);
                way_points.emplace_back(point2);
                way_points.emplace_back(point3);
                way_points.emplace_back(point4);

                SendFreespaceCommand(way_points, end_pose);
            } else {
                std::cout << "Invalid input!" << input_command_string << std::endl;
            }
        }
        return true;
    }

       void RemoteCotrolCommandDemo::SendFreespaceCommand(
            const std::vector<apollo::external_command::Point>& way_points,
            const apollo::external_command::Pose& end) {
        auto command = std::make_shared<apollo::external_command::FreeSpaceCommand>();
        FillCommandHeader(command);
        // Copy way_points
        auto roi_point = command->mutable_drivable_roi();
        for (const auto& point : way_points) {
            roi_point->add_point()->CopyFrom(point);
        }
        // Copy end point
        command->mutable_parking_spot_pose()->CopyFrom(end);
        std::cout << "Sending lane follow command: " << command->DebugString() << std::endl;
        auto response = free_space_command_client_->SendRequest(command);
        if (nullptr == response) {
            std::cout << "Command sending failed, please check the service is on!\n" << std::endl;
        } else {
            std::cout << "******Finish sending command.******\n" << std::endl;
        }
        }
   ```

   remote_control_command_demo.h:

   ```bash
    #pragma once

    #include <memory>
    #include <string>
    #include <vector>

    #include "modules/common_msgs/external_command_msgs/action_command.pb.h"
    #include "modules/common_msgs/external_command_msgs/chassis_command.pb.h"
    #include "modules/common_msgs/external_command_msgs/command_status.pb.h"
    #include "modules/common_msgs/external_command_msgs/free_space_command.pb.h"
    #include "modules/common_msgs/external_command_msgs/lane_follow_command.pb.h"
    #include "modules/common_msgs/external_command_msgs/path_follow_command.pb.h"
    #include "modules/common_msgs/external_command_msgs/speed_command.pb.h"
    #include "modules/common_msgs/external_command_msgs/valet_parking_command.pb.h"
    #include "modules/common_msgs/planning_msgs/planning.pb.h"

    #include "cyber/component/timer_component.h"
    #include "cyber/cyber.h"
    #include "modules/common/service_wrapper/client_wrapper.h"
    #include "modules/common/util/message_util.h"

    class RemoteCotrolCommandDemo final : public apollo::cyber::TimerComponent {
    public:
        RemoteCotrolCommandDemo();

        bool Init() override;

        bool Proc() override;

    private:
        template <typename T>
        void FillCommandHeader(const std::shared_ptr<T>& command);

        void SendFreespaceCommand(
                const std::vector<apollo::external_command::Point>& way_points,
                const apollo::external_command::Pose& end);

        std::shared_ptr<apollo::common::ClientWrapper<
                apollo::external_command::ActionCommand,
                apollo::external_command::CommandStatus>>
                action_command_client_;
        std::shared_ptr<apollo::common::ClientWrapper<
                apollo::external_command::FreeSpaceCommand,
                apollo::external_command::CommandStatus>>
                free_space_command_client_;
        std::shared_ptr<apollo::common::ClientWrapper<
                apollo::external_command::LaneFollowCommand,
                apollo::external_command::CommandStatus>>
                lane_follow_command_client_;

        uint64_t command_id_;
        const std::string module_name_;
    };

    template <typename T>
    void RemoteCotrolCommandDemo::FillCommandHeader(const std::shared_ptr<T>& command) {
        apollo::common::util::FillHeader(module_name_, command.get());
        command->set_command_id(++command_id_);
    }

    CYBER_REGISTER_COMPONENT(RemoteCotrolCommandDemo);
   ```

   cyberfile.xml:

   ```bash
    <package format="2">
    <name>external-command-demo</name>
    <version>local</version>
    <description> The package contains temp component for converting routing message to external command. </description>
    <maintainer email="apollo-support@baidu.com">Apollo</maintainer>
    <license>Apache License 2.0</license>
    <url type="website">https://www.apollo.auto/</url>
    <url type="repository">https://github.com/ApolloAuto/apollo</url>
    <url type="bugtracker">https://github.com/ApolloAuto/apollo/issues</url>
    <type>module</type>
    <src_path url="https://github.com/ApolloAuto/apollo">//remote_control_command_demo</src_path>
    <depend repo_name="com_github_gflags_gflags" lib_names="gflags">3rd-gflags</depend>
    <depend type="binary" repo_name="common" lib_names="common">common</depend>
    <depend repo_name="routing" type="binary">routing</depend>
   </package>
   ```

   BUILD:

   ```bash
   load("//tools:cpplint.bzl", "cpplint")
    load("//tools:apollo_package.bzl", "apollo_cc_binary", "apollo_cc_library", "apollo_cc_test", "apollo_component", "apollo_package")

    package(default_visibility = ["//visibility:public"])

    apollo_component(
        name = "libremote_control_command_demo_component.so",
        hdrs = ["remote_control_command_demo.h"],
        srcs = ["remote_control_command_demo.cc"],
        copts = ["-DMODULE_NAME=\\\"external_command_demo\\\""],
        deps = [
            "//cyber",
            "//modules/common/util:message_util",
            "//modules/common_msgs/external_command_msgs:command_status_cc_proto",
            "//modules/common_msgs/external_command_msgs:action_command_cc_proto",
            "//modules/common_msgs/external_command_msgs:chassis_command_cc_proto",
            "//modules/common_msgs/external_command_msgs:free_space_command_cc_proto",
            "//modules/common_msgs/external_command_msgs:lane_follow_command_cc_proto",
            "//modules/common_msgs/external_command_msgs:path_follow_command_cc_proto",
            "//modules/common_msgs/external_command_msgs:speed_command_cc_proto",
            "//modules/common_msgs/external_command_msgs:valet_parking_command_cc_proto",
            "//modules/common_msgs/planning_msgs:planning_cc_proto",
            "//modules/common/service_wrapper:client_wrapper",
        ],
    )

    filegroup(
        name = "conf",
        srcs = glob([
            "conf/*",
            "dag/*",
            "launch/*"
        ]),
    )

    cpplint()
    apollo_package()
   ```

   dag/remote_control_command_demo.dag:

   ```bash
   module_config {
     module_library : "remote_control_command_demo/libremote_control_command_demo_component.so"
     timer_components {
       class_name : "RemoteCotrolCommandDemo"
       config {
         name: "remote_control_command_demo_component"
         interval: 200 # milliseconds
       }
     }
   }
   ```

   launch/remote_control_command_demo.launch:

   ```bash
   <cyber>
      <module>
        <name>remote_control_command_demo</name>
        <dag_conf>remote_control_command_demo/dag/remote_control_command_demo.dag</dag_conf>
        <process_name>remote_control_command_demo</process_name>
      </module>
    </cyber>
   ```

   conf/remote_control_command_demo.conf:

   ```bash
   --flagfile=/apollo/modules/common/data/global_flagfile.txt
   ```

4. After the code files have been copied, click the upper left corner to save the code files.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_558dca9.png)

5. Click the previous web page link to return to the operation terminal.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ee67a23.png)

6. Run the following commands to perform the code build;

   Build the remote vehicle control module:

   ```bash
    buildtool init
    buildtool build -p remote_control_command_demo
   ```

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_932118c.png)

7. As shown below, the build normally succeeds. If the build fails, please check whether the replaced code content is correct (after checking and saving, simply run the build command in step 5 again).

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_4d9df4b.png)

### 2. Start the Remote Vehicle Control Module

1. In the terminal, run the following command to start the remote vehicle control module. After it succeeds, it is as shown below.

   Remote command module:

   ```bash
   mainboard -d remote_control_command_demo/dag/remote_control_command_demo.dag
   ```

2. After it succeeds, as shown below, this process is suspended on its own:

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_cc8e694.png)

### 3. Start DreamView+

In a new terminal, run `aem bootstrap start --plus` to start DreamView+. After it succeeds, click the Dreamview+ button in the menu bar to enter the Dreamview+ interface.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_8c9001f.png)

Command to start Dreamview+:

```bash
aem bootstrap start --plus
```

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_5de4f6c.png)

When the following appears, Dreamview+ has started successfully.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_ebb7089.png)

Click **Personal Center** > **Settings** > **General Settings** at the lower left to select the interface language.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a957a49.png)

### 4. Start the Vehicle's Runtime Environment

1. Click the left menu bar, select the PnC mode, and select Sim_Control as the operation (if there is an error on first entry, please click the refresh button).

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_911ffee.png)

2. Enable the Planning, Prediction and TaskManager modules, and Dreamview+ then starts these modules.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a389f11.png)

3. Click the **Routing Editing** button, and the corresponding routing information starts being built for the vehicle;

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_061d28f.png)

4. Click the **Start Position** button and select the initial pose of the vehicle (currently only positions within the map scene can be selected);

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_a6a5018.png)

5. Click the **Waypoint** button and select the end point of the vehicle's run (multiple can be selected) to build the vehicle's destination position (currently only positions within the map scene can be selected). After the selection is complete, click the **Save Editing** button, and the built scenario information is saved.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_e00dd23.png)

6. After the scenario information is built, click the run button at the lower left. The vehicle builds a trajectory line from the start position to the end point, and the vehicle starts driving normally along the trajectory line.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_2116e2e.png)

7. If you need to pause the current scenario while it is running, you can click the pause button at the lower left. The vehicle then pauses at its current position, and clicking the run button again makes the vehicle continue driving along the trajectory line.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_2841cba.png)

### 5. Send Remote Control Commands

1. Click the run button at the lower left of Dreamview+ to make sure the vehicle drives along the current trajectory line, switch to the remote control terminal, and enter the **STOP** command in the terminal. The vehicle goes from the **LANE_FOLLOW** scenario into the **EMERGENCY_STOP** scenario (as shown in Dreamview+), and the vehicle slowly stops at its current position.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_2f3a594.png)

   Switch to the Dreamview+ interface and you can see that the vehicle has executed the emergency braking command;

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_87e76c9.png)

2. Continue by entering the START command in the terminal. The vehicle enters the **LANE_FOLLOW** scenario again and continues driving along the current routing line, as shown in the Dreamview+ interface below;

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_c873f7b.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_777d320.png)

3. Enter the **pull_over** command in the terminal. The vehicle enters the **PULL_OVER** scenario and executes the pull-over command;

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_3797af1.png)

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_38e0af0.png)

4. Continue by entering the **START** command in the terminal. The vehicle goes from the **PULL_OVER** scenario into the **LANE_FOLLOW** scenario again and continues driving along the current routing line.

5. When we send an incorrect command to the terminal, for example **aaaa**, the system cannot recognize the command and reminds us that it is an invalid command, with no impact on the vehicle's current running scenario.

   ![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_Beta_Doc/image_0d0e863.png)
