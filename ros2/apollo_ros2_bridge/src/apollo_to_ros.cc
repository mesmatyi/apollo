/******************************************************************************
 * Copyright 2026 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * ApolloToRos: subscribes to Apollo channels with the Cyber API and
 * republishes them as standard ROS 2 messages.
 *
 * Loaded in the same component container as the Apollo components it
 * bridges, it receives their messages without serialization (Cyber
 * intra-process path).
 *
 * Output topics: "<output_prefix><channel>" (default prefix "/ros"), except
 * transforms, which go to the standard /tf and /tf_static topics.
 */

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

#include "apollo_ros2_bridge/conversions.h"
// Only the Cyber node API: cyber/cyber.h would pull in Cyber's class loader,
// whose CLASS_LOADER_REGISTER_CLASS macro clashes with ROS 2 class_loader.
#include "cyber/init.h"
#include "cyber/node/node.h"
#include "cyber/ros2/naming.h"
#include "cyber/state.h"
#include "cyber/transport/qos/qos_profile_conf.h"

namespace apollo_ros2_bridge {

class ApolloToRos : public rclcpp::Node {
 public:
  explicit ApolloToRos(const rclcpp::NodeOptions& options)
      : rclcpp::Node("apollo_to_ros", options) {
    if (!apollo::cyber::OK()) {
      apollo::cyber::Init("apollo_ros2_bridge");
    }
    // Non-owning: the Cyber node and its readers are members of this node
    // and are destroyed (and shut down) before it.
    cyber_node_ = std::make_shared<apollo::cyber::Node>(
        std::shared_ptr<rclcpp::Node>(this, [](rclcpp::Node*) {}));

    prefix_ = declare_parameter<std::string>("output_prefix", "/ros");
    publish_localization_tf_ =
        declare_parameter<bool>("publish_localization_tf", true);

    auto pointclouds = declare_parameter<std::vector<std::string>>(
        "pointcloud_channels",
        {"/apollo/sensor/lidar128/compensator/PointCloud2",
         "/apollo/sensor/velodyne64/compensator/PointCloud2",
         "/apollo/sensor/lidar16/compensator/PointCloud2"});
    auto images = declare_parameter<std::vector<std::string>>(
        "image_channels", {"/apollo/sensor/camera/front_6mm/image",
                           "/apollo/sensor/camera/front_12mm/image"});
    auto compressed = declare_parameter<std::vector<std::string>>(
        "compressed_image_channels",
        {"/apollo/sensor/camera/front_6mm/image/compressed",
         "/apollo/sensor/camera/front_12mm/image/compressed"});
    auto localization = declare_parameter<std::string>(
        "localization_channel", "/apollo/localization/pose");
    auto tf = declare_parameter<std::string>("tf_channel", "/tf");
    auto tf_static =
        declare_parameter<std::string>("tf_static_channel", "/tf_static");
    auto imu = declare_parameter<std::string>(
        "imu_channel", "/apollo/sensor/gnss/corrected_imu");
    auto gnss = declare_parameter<std::string>(
        "gnss_best_pose_channel", "/apollo/sensor/gnss/best_pose");
    auto planning =
        declare_parameter<std::string>("planning_channel", "/apollo/planning");
    auto obstacles = declare_parameter<std::string>(
        "perception_obstacles_channel", "/apollo/perception/obstacles");

    for (const auto& channel : pointclouds) {
      Bridge<apollo::drivers::PointCloud, sensor_msgs::msg::PointCloud2>(
          channel, rclcpp::SensorDataQoS());
    }
    for (const auto& channel : images) {
      Bridge<apollo::drivers::Image, sensor_msgs::msg::Image>(
          channel, rclcpp::SensorDataQoS());
    }
    for (const auto& channel : compressed) {
      Bridge<apollo::drivers::CompressedImage,
             sensor_msgs::msg::CompressedImage>(channel,
                                                rclcpp::SensorDataQoS());
    }
    Bridge<apollo::localization::CorrectedImu, sensor_msgs::msg::Imu>(
        imu, rclcpp::SensorDataQoS());
    Bridge<apollo::drivers::gnss::GnssBestPose, sensor_msgs::msg::NavSatFix>(
        gnss, rclcpp::SensorDataQoS());
    Bridge<apollo::planning::ADCTrajectory, nav_msgs::msg::Path>(
        planning, rclcpp::QoS(10));
    Bridge<apollo::perception::PerceptionObstacles,
           visualization_msgs::msg::MarkerArray>(obstacles, rclcpp::QoS(10));
    BridgeLocalization(localization);
    BridgeTf(tf, false);
    BridgeTf(tf_static, true);

    RCLCPP_INFO(get_logger(), "bridging %zu Apollo channels to ROS 2",
                readers_.size());
  }

  ~ApolloToRos() override {
    for (auto& reader : readers_) {
      reader->Shutdown();
    }
    readers_.clear();
    cyber_node_.reset();
  }

 private:
  std::string OutputTopic(const std::string& channel) const {
    // ChannelToTopic() maps names reserved by ROS 2 (/tf, ...) under /apollo;
    // apply the prefix to the plain channel path.
    std::string topic = apollo::cyber::ros2::ChannelToTopic(channel);
    return prefix_.empty() ? topic : prefix_ + topic;
  }

  template <typename ApolloT, typename RosT>
  void Bridge(const std::string& channel, const rclcpp::QoS& qos) {
    if (channel.empty()) {
      return;
    }
    auto publisher = create_publisher<RosT>(OutputTopic(channel), qos);
    auto reader = cyber_node_->CreateReader<ApolloT>(
        channel, [publisher](const std::shared_ptr<ApolloT>& msg) {
          if (publisher->get_subscription_count() == 0 &&
              publisher->get_intra_process_subscription_count() == 0) {
            return;
          }
          auto out = std::make_unique<RosT>();
          ToRos(*msg, out.get());
          publisher->publish(std::move(out));
        });
    if (reader) {
      readers_.push_back(reader);
      publishers_.push_back(publisher);
    }
  }

  void BridgeLocalization(const std::string& channel) {
    if (channel.empty()) {
      return;
    }
    auto odom = create_publisher<nav_msgs::msg::Odometry>(OutputTopic(channel),
                                                          rclcpp::QoS(10));
    auto tf = create_publisher<tf2_msgs::msg::TFMessage>("/tf", rclcpp::QoS(100));
    const bool publish_tf = publish_localization_tf_;
    auto reader = cyber_node_->CreateReader<
        apollo::localization::LocalizationEstimate>(
        channel,
        [odom, tf, publish_tf](
            const std::shared_ptr<apollo::localization::LocalizationEstimate>&
                msg) {
          auto out = std::make_unique<nav_msgs::msg::Odometry>();
          ToRos(*msg, out.get());
          odom->publish(std::move(out));
          if (publish_tf) {
            tf2_msgs::msg::TFMessage tf_msg;
            geometry_msgs::msg::TransformStamped t;
            ToRos(*msg, &t);
            tf_msg.transforms.push_back(t);
            tf->publish(tf_msg);
          }
        });
    if (reader) {
      readers_.push_back(reader);
      publishers_.push_back(odom);
      publishers_.push_back(tf);
    }
  }

  void BridgeTf(const std::string& channel, bool is_static) {
    if (channel.empty()) {
      return;
    }
    rclcpp::QoS qos(100);
    apollo::cyber::proto::RoleAttributes attr;
    attr.set_channel_name(channel);
    if (is_static) {
      qos = rclcpp::QoS(rclcpp::KeepLast(1)).transient_local().reliable();
      attr.mutable_qos_profile()->CopyFrom(
          apollo::cyber::transport::QosProfileConf::QOS_PROFILE_TF_STATIC);
    }
    auto publisher = create_publisher<tf2_msgs::msg::TFMessage>(
        is_static ? "/tf_static" : "/tf", qos);
    // Static transforms accumulate: ROS 2 latches only the last sample.
    auto statics = std::make_shared<std::map<std::string, geometry_msgs::msg::TransformStamped>>();
    auto reader = cyber_node_->CreateReader<apollo::transform::TransformStampeds>(
        attr,
        [publisher, is_static, statics](
            const std::shared_ptr<apollo::transform::TransformStampeds>& msg) {
          tf2_msgs::msg::TFMessage out;
          ToRos(*msg, &out);
          if (is_static) {
            for (auto& t : out.transforms) {
              (*statics)[t.child_frame_id] = t;
            }
            out.transforms.clear();
            for (auto& item : *statics) {
              out.transforms.push_back(item.second);
            }
          }
          publisher->publish(out);
        });
    if (reader) {
      readers_.push_back(reader);
      publishers_.push_back(publisher);
    }
  }

  std::string prefix_;
  bool publish_localization_tf_ = true;
  std::shared_ptr<apollo::cyber::Node> cyber_node_;
  std::vector<std::shared_ptr<apollo::cyber::ReaderBase>> readers_;
  std::vector<rclcpp::PublisherBase::SharedPtr> publishers_;
};

}  // namespace apollo_ros2_bridge

RCLCPP_COMPONENTS_REGISTER_NODE(apollo_ros2_bridge::ApolloToRos)
