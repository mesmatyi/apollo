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
 * RosToApollo: feeds standard ROS 2 sensor topics into Apollo channels, so
 * ROS 2 lidar / camera drivers can drive Apollo perception and localization.
 *
 * Parameters are lists of "ros_topic=apollo_channel" pairs:
 *   pointcloud_inputs:        sensor_msgs/PointCloud2  -> apollo.drivers.PointCloud
 *   image_inputs:             sensor_msgs/Image        -> apollo.drivers.Image
 *   compressed_image_inputs:  sensor_msgs/CompressedImage
 *                                                      -> apollo.drivers.CompressedImage
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

#include "apollo_ros2_bridge/conversions.h"
#include "cyber/init.h"
#include "cyber/node/node.h"
#include "cyber/state.h"

namespace apollo_ros2_bridge {

class RosToApollo : public rclcpp::Node {
 public:
  explicit RosToApollo(const rclcpp::NodeOptions& options)
      : rclcpp::Node("ros_to_apollo", options) {
    if (!apollo::cyber::OK()) {
      apollo::cyber::Init("apollo_ros2_bridge");
    }
    cyber_node_ = std::make_shared<apollo::cyber::Node>(
        std::shared_ptr<rclcpp::Node>(this, [](rclcpp::Node*) {}));

    auto clouds = declare_parameter<std::vector<std::string>>(
        "pointcloud_inputs", std::vector<std::string>{});
    auto images = declare_parameter<std::vector<std::string>>(
        "image_inputs", std::vector<std::string>{});
    auto compressed = declare_parameter<std::vector<std::string>>(
        "compressed_image_inputs", std::vector<std::string>{});

    for (const auto& pair : clouds) {
      std::string topic, channel;
      if (Split(pair, &topic, &channel)) {
        BridgePointCloud(topic, channel);
      }
    }
    for (const auto& pair : images) {
      std::string topic, channel;
      if (Split(pair, &topic, &channel)) {
        Bridge<sensor_msgs::msg::Image, apollo::drivers::Image>(topic, channel);
      }
    }
    for (const auto& pair : compressed) {
      std::string topic, channel;
      if (Split(pair, &topic, &channel)) {
        Bridge<sensor_msgs::msg::CompressedImage,
               apollo::drivers::CompressedImage>(topic, channel);
      }
    }
    RCLCPP_INFO(get_logger(), "bridging %zu ROS 2 topics into Apollo",
                subscriptions_.size());
  }

  ~RosToApollo() override {
    subscriptions_.clear();
    writers_.clear();
    cyber_node_.reset();
  }

 private:
  bool Split(const std::string& pair, std::string* topic,
             std::string* channel) {
    if (pair.empty()) {
      return false;  // YAML needs [""] for an empty string list
    }
    auto pos = pair.find('=');
    if (pos == std::string::npos || pos == 0 || pos + 1 == pair.size()) {
      RCLCPP_ERROR(get_logger(),
                   "invalid mapping '%s', expected ros_topic=apollo_channel",
                   pair.c_str());
      return false;
    }
    *topic = pair.substr(0, pos);
    *channel = pair.substr(pos + 1);
    return true;
  }

  void BridgePointCloud(const std::string& topic, const std::string& channel) {
    auto writer =
        cyber_node_->CreateWriter<apollo::drivers::PointCloud>(channel);
    if (!writer) {
      return;
    }
    auto sequence = std::make_shared<uint32_t>(0);
    auto logger = get_logger();
    auto sub = create_subscription<sensor_msgs::msg::PointCloud2>(
        topic, rclcpp::SensorDataQoS(),
        [writer, sequence, logger](
            const sensor_msgs::msg::PointCloud2::ConstSharedPtr& msg) {
          auto out = std::make_shared<apollo::drivers::PointCloud>();
          if (!FromRos(*msg, out.get())) {
            RCLCPP_WARN_ONCE(logger, "PointCloud2 without x/y/z fields");
            return;
          }
          out->mutable_header()->set_sequence_num((*sequence)++);
          writer->Write(out);
        });
    subscriptions_.push_back(sub);
    writers_.push_back(writer);
  }

  template <typename RosT, typename ApolloT>
  void Bridge(const std::string& topic, const std::string& channel) {
    auto writer = cyber_node_->CreateWriter<ApolloT>(channel);
    if (!writer) {
      return;
    }
    auto sequence = std::make_shared<uint32_t>(0);
    auto sub = create_subscription<RosT>(
        topic, rclcpp::SensorDataQoS(),
        [writer, sequence](const typename RosT::ConstSharedPtr& msg) {
          auto out = std::make_shared<ApolloT>();
          FromRos(*msg, out.get());
          out->mutable_header()->set_sequence_num((*sequence)++);
          writer->Write(out);
        });
    subscriptions_.push_back(sub);
    writers_.push_back(writer);
  }

  std::shared_ptr<apollo::cyber::Node> cyber_node_;
  std::vector<rclcpp::SubscriptionBase::SharedPtr> subscriptions_;
  std::vector<std::shared_ptr<apollo::cyber::WriterBase>> writers_;
};

}  // namespace apollo_ros2_bridge

RCLCPP_COMPONENTS_REGISTER_NODE(apollo_ros2_bridge::RosToApollo)
