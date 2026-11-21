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

#include "cyber/node/node.h"

#include "rclcpp/rclcpp.hpp"

namespace apollo {
namespace cyber {

Node::Node(const std::string& node_name, const std::string& name_space)
    : node_name_(node_name), name_space_(name_space) {
  handle_ = ros2::NodeHandle::Create(node_name, name_space);
}

Node::Node(const std::shared_ptr<rclcpp::Node>& ros_node) {
  handle_ = ros2::NodeHandle::Wrap(ros_node);
  node_name_ = ros_node->get_name();
  name_space_ = ros_node->get_namespace();
  if (name_space_ == "/") {
    name_space_.clear();
  }
}

Node::~Node() {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  readers_.clear();
}

const std::string& Node::Name() const { return node_name_; }

std::shared_ptr<rclcpp::Node> Node::RosNode() const {
  return handle_ ? handle_->ros_node() : nullptr;
}

void Node::Observe() {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  for (auto& reader : readers_) {
    reader.second->Observe();
  }
}

void Node::ClearData() {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  for (auto& reader : readers_) {
    reader.second->ClearData();
  }
}

bool Node::DeleteReader(const std::string& channel_name) {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  int result = static_cast<int>(readers_.erase(channel_name));
  return result == 1;
}

bool Node::DeleteReader(const ReaderConfig& config) {
  return DeleteReader(config.channel_name);
}

bool Node::DeleteReader(const proto::RoleAttributes& role_attr) {
  return DeleteReader(role_attr.channel_name());
}

}  // namespace cyber
}  // namespace apollo
