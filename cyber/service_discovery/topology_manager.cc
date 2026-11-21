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

#include "cyber/service_discovery/topology_manager.h"

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "cyber/common/global_data.h"
#include "cyber/common/log.h"
#include "cyber/message/protobuf_factory.h"
#include "cyber/ros2/naming.h"
#include "cyber/ros2/native_types.h"
#include "cyber/ros2/runtime.h"
#include "cyber/ros2/transport.h"
#include "cyber/state.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {
namespace service_discovery {

namespace {

rclcpp::Node::SharedPtr GraphNode() {
  return ros2::AuxNodeHandle()->ros_node();
}

std::string QualifiedNodeName(const std::string& ns, const std::string& name) {
  if (ns.empty() || ns == "/") {
    return name;
  }
  return (ns.front() == '/' ? ns.substr(1) : ns) + "/" + name;
}

RoleAttributes MakeAttr(const rclcpp::TopicEndpointInfo& info,
                        const std::string& channel) {
  RoleAttributes attr;
  attr.set_node_name(QualifiedNodeName(info.node_namespace(), info.node_name()));
  attr.set_channel_name(channel);
  // A native ROS 2 type names its protobuf type; for the protobuf wire
  // format the type is whatever this process learned about the channel.
  std::string type;
  if (const auto* native =
          ros2::NativeTypes::Instance()->FindByRosType(info.topic_type())) {
    attr.set_message_type(native->proto_name);
  } else if (ros2::LookupLocalChannelType(channel, &type)) {
    attr.set_message_type(type);
  }
  return attr;
}

/// Apollo channels currently present in the ROS 2 graph.
std::vector<std::string> ApolloChannels(const rclcpp::Node::SharedPtr& node) {
  std::vector<std::string> channels;
  for (auto& item : node->get_topic_names_and_types()) {
    for (auto& type : item.second) {
      if (ros2::IsApolloTopicType(type)) {
        channels.push_back(item.first);
        break;
      }
    }
  }
  return channels;
}

void Endpoints(const std::string& channel, bool writers, Manager::RoleAttrVec* out) {
  if (out == nullptr) {
    return;
  }
  auto node = GraphNode();
  auto topic = ros2::ChannelToTopic(channel);
  auto infos = writers ? node->get_publishers_info_by_topic(topic)
                       : node->get_subscriptions_info_by_topic(topic);
  for (auto& info : infos) {
    out->push_back(MakeAttr(info, channel));
  }
}

void AllEndpoints(bool writers, Manager::RoleAttrVec* out) {
  if (out == nullptr) {
    return;
  }
  for (auto& channel : ApolloChannels(GraphNode())) {
    Endpoints(channel, writers, out);
  }
}

bool NodeMatches(const RoleAttributes& attr, const std::string& node_name) {
  return attr.node_name() == node_name ||
         attr.node_name() == ros2::ToNodeName(node_name);
}

ChangeMsg MakeChange(ChangeType change, OperateType op, RoleType role,
                     const RoleAttributes& attr) {
  ChangeMsg msg;
  msg.set_timestamp(Time::Now().ToNanosecond());
  msg.set_change_type(change);
  msg.set_operate_type(op);
  msg.set_role_type(role);
  msg.mutable_role_attr()->CopyFrom(attr);
  return msg;
}

/// Snapshot of the graph used to derive Join/Leave notifications.
struct GraphSnapshot {
  std::set<std::string> nodes;
  // (channel, node) pairs
  std::set<std::pair<std::string, std::string>> writers;
  std::set<std::pair<std::string, std::string>> readers;
  std::set<std::string> services;
};

GraphSnapshot TakeSnapshot(const rclcpp::Node::SharedPtr& node) {
  GraphSnapshot snap;
  for (auto& name : node->get_node_names()) {
    std::string n = name;
    if (!n.empty() && n.front() == '/') {
      n = n.substr(1);
    }
    snap.nodes.insert(n);
  }
  for (auto& channel : ApolloChannels(node)) {
    for (auto& info : node->get_publishers_info_by_topic(channel)) {
      snap.writers.emplace(
          channel, QualifiedNodeName(info.node_namespace(), info.node_name()));
    }
    for (auto& info : node->get_subscriptions_info_by_topic(channel)) {
      snap.readers.emplace(
          channel, QualifiedNodeName(info.node_namespace(), info.node_name()));
    }
  }
  for (auto& item : node->get_service_names_and_types()) {
    for (auto& type : item.second) {
      if (ros2::IsApolloServiceType(type)) {
        snap.services.insert(item.first);
      }
    }
  }
  return snap;
}

template <typename T, typename F>
void Diff(const std::set<T>& before, const std::set<T>& after, F on_change) {
  for (auto& x : after) {
    if (!before.count(x)) {
      on_change(x, proto::OPT_JOIN);
    }
  }
  for (auto& x : before) {
    if (!after.count(x)) {
      on_change(x, proto::OPT_LEAVE);
    }
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Manager
// ---------------------------------------------------------------------------

Manager::ChangeConnection Manager::AddChangeListener(const ChangeFunc& func) {
  TopologyManager::Instance()->EnsureWatcher();
  return signal_.Connect(func);
}

void Manager::RemoveChangeListener(const ChangeConnection& conn) {
  auto local_conn = conn;
  local_conn.Disconnect();
}

// ---------------------------------------------------------------------------
// ChannelManager
// ---------------------------------------------------------------------------

void ChannelManager::GetChannelNames(std::vector<std::string>* channels) {
  if (channels == nullptr) {
    return;
  }
  auto names = ApolloChannels(GraphNode());
  channels->insert(channels->end(), names.begin(), names.end());
}

void ChannelManager::GetProtoDesc(const std::string& channel_name,
                                  std::string* proto_desc) {
  if (proto_desc == nullptr) {
    return;
  }
  const std::string type = ros2::ChannelProtoType(channel_name);
  if (type.empty()) {
    return;
  }
  message::ProtobufFactory::Instance()->GetDescriptorString(type, proto_desc);
  if (proto_desc->empty()) {
    // Not compiled into this process (tools): use the descriptors installed
    // with the interface packages.
    const auto* descriptor = ros2::NativeTypes::Instance()->FindDescriptor(type);
    if (descriptor != nullptr) {
      message::ProtobufFactory::GetDescriptorString(descriptor, proto_desc);
    }
  }
}

void ChannelManager::GetMsgType(const std::string& channel_name,
                                std::string* msg_type) {
  if (msg_type == nullptr) {
    return;
  }
  const std::string type = ros2::ChannelProtoType(channel_name);
  if (!type.empty()) {
    *msg_type = type;
  }
}

bool ChannelManager::HasWriter(const std::string& channel_name) {
  return GraphNode()->count_publishers(ros2::ChannelToTopic(channel_name)) > 0;
}

void ChannelManager::GetWriters(RoleAttrVec* writers) {
  AllEndpoints(true, writers);
}

void ChannelManager::GetWritersOfNode(const std::string& node_name,
                                      RoleAttrVec* writers) {
  if (writers == nullptr) {
    return;
  }
  RoleAttrVec all;
  AllEndpoints(true, &all);
  for (auto& attr : all) {
    if (NodeMatches(attr, node_name)) {
      writers->push_back(attr);
    }
  }
}

void ChannelManager::GetWritersOfChannel(const std::string& channel_name,
                                         RoleAttrVec* writers) {
  Endpoints(channel_name, true, writers);
}

bool ChannelManager::HasReader(const std::string& channel_name) {
  return GraphNode()->count_subscribers(ros2::ChannelToTopic(channel_name)) >
         0;
}

void ChannelManager::GetReaders(RoleAttrVec* readers) {
  AllEndpoints(false, readers);
}

void ChannelManager::GetReadersOfNode(const std::string& node_name,
                                      RoleAttrVec* readers) {
  if (readers == nullptr) {
    return;
  }
  RoleAttrVec all;
  AllEndpoints(false, &all);
  for (auto& attr : all) {
    if (NodeMatches(attr, node_name)) {
      readers->push_back(attr);
    }
  }
}

void ChannelManager::GetReadersOfChannel(const std::string& channel_name,
                                         RoleAttrVec* readers) {
  Endpoints(channel_name, false, readers);
}

void ChannelManager::GetUpstreamOfNode(const std::string& node_name,
                                       RoleAttrVec* upstream) {
  if (upstream == nullptr) {
    return;
  }
  RoleAttrVec readers;
  GetReadersOfNode(node_name, &readers);
  for (auto& reader : readers) {
    GetWritersOfChannel(reader.channel_name(), upstream);
  }
}

void ChannelManager::GetDownstreamOfNode(const std::string& node_name,
                                         RoleAttrVec* downstream) {
  if (downstream == nullptr) {
    return;
  }
  RoleAttrVec writers;
  GetWritersOfNode(node_name, &writers);
  for (auto& writer : writers) {
    GetReadersOfChannel(writer.channel_name(), downstream);
  }
}

// ---------------------------------------------------------------------------
// NodeManager / ServiceManager
// ---------------------------------------------------------------------------

bool NodeManager::HasNode(const std::string& node_name) {
  const std::string ros_name = ros2::ToNodeName(node_name);
  for (auto& name : GraphNode()->get_node_names()) {
    auto pos = name.rfind('/');
    std::string base = pos == std::string::npos ? name : name.substr(pos + 1);
    if (base == ros_name || base == node_name) {
      return true;
    }
  }
  return false;
}

void NodeManager::GetNodes(RoleAttrVec* nodes) {
  if (nodes == nullptr) {
    return;
  }
  for (auto& name : GraphNode()->get_node_names()) {
    RoleAttributes attr;
    attr.set_node_name(!name.empty() && name.front() == '/' ? name.substr(1)
                                                            : name);
    nodes->push_back(attr);
  }
}

bool ServiceManager::HasService(const std::string& service_name) {
  return GraphNode()->count_services(ros2::ChannelToTopic(service_name)) > 0;
}

void ServiceManager::GetServers(RoleAttrVec* servers) {
  if (servers == nullptr) {
    return;
  }
  for (auto& item : GraphNode()->get_service_names_and_types()) {
    for (auto& type : item.second) {
      if (ros2::IsApolloServiceType(type)) {
        RoleAttributes attr;
        attr.set_service_name(item.first);
        servers->push_back(attr);
      }
    }
  }
}

void ServiceManager::GetClients(const std::string& service_name,
                                RoleAttrVec* clients) {
  if (clients == nullptr) {
    return;
  }
  auto node = GraphNode();
  const auto topic = ros2::ChannelToTopic(service_name);
  for (auto& name_ns : node->get_node_graph_interface()
                           ->get_node_names_and_namespaces()) {
    auto client_services =
        node->get_node_graph_interface()->get_client_names_and_types_by_node(
            name_ns.first, name_ns.second);
    if (client_services.count(topic)) {
      RoleAttributes attr;
      attr.set_node_name(QualifiedNodeName(name_ns.second, name_ns.first));
      attr.set_service_name(service_name);
      clients->push_back(attr);
    }
  }
}

// ---------------------------------------------------------------------------
// TopologyManager
// ---------------------------------------------------------------------------

TopologyManager::TopologyManager()
    : node_manager_(std::make_shared<NodeManager>()),
      channel_manager_(std::make_shared<ChannelManager>()),
      service_manager_(std::make_shared<ServiceManager>()) {}

TopologyManager::~TopologyManager() { Shutdown(); }

void TopologyManager::Shutdown() {
  running_.store(false);
  if (watcher_.joinable()) {
    if (watcher_.get_id() == std::this_thread::get_id()) {
      watcher_.detach();
    } else {
      watcher_.join();
    }
  }
  node_manager_->Shutdown();
  channel_manager_->Shutdown();
  service_manager_->Shutdown();
  change_signal_.DisconnectAllSlots();
}

TopologyManager::ChangeConnection TopologyManager::AddChangeListener(
    const ChangeFunc& func) {
  EnsureWatcher();
  return change_signal_.Connect(func);
}

void TopologyManager::RemoveChangeListener(const ChangeConnection& conn) {
  auto local_conn = conn;
  local_conn.Disconnect();
}

void TopologyManager::EnsureWatcher() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_.exchange(true)) {
    return;
  }
  watcher_ = std::thread([this]() { Watch(); });
}

void TopologyManager::Watch() {
  auto node = GraphNode();
  auto event = node->get_graph_event();
  GraphSnapshot previous;
  while (running_.load() && rclcpp::ok() && !cyber::IsShutdown()) {
    GraphSnapshot current;
    try {
      current = TakeSnapshot(node);
    } catch (const std::exception& e) {
      ADEBUG << "graph snapshot failed: " << e.what();
      break;
    }
    Diff(previous.nodes, current.nodes,
         [this](const std::string& name, OperateType op) {
           RoleAttributes attr;
           attr.set_node_name(name);
           auto msg = MakeChange(proto::CHANGE_NODE, op, proto::ROLE_NODE, attr);
           node_manager_->Notify(msg);
           change_signal_(msg);
         });
    auto endpoint_change = [this](RoleType role) {
      return [this, role](const std::pair<std::string, std::string>& key,
                          OperateType op) {
        RoleAttributes attr;
        attr.set_channel_name(key.first);
        attr.set_node_name(key.second);
        const std::string type = ros2::ChannelProtoType(key.first);
        if (!type.empty()) {
          attr.set_message_type(type);
        }
        attr.set_channel_id(cyber::common::GlobalData::RegisterChannel(key.first));
        auto msg = MakeChange(proto::CHANGE_CHANNEL, op, role, attr);
        channel_manager_->Notify(msg);
        change_signal_(msg);
      };
    };
    Diff(previous.writers, current.writers, endpoint_change(proto::ROLE_WRITER));
    Diff(previous.readers, current.readers, endpoint_change(proto::ROLE_READER));
    Diff(previous.services, current.services,
         [this](const std::string& name, OperateType op) {
           RoleAttributes attr;
           attr.set_service_name(name);
           auto msg =
               MakeChange(proto::CHANGE_SERVICE, op, proto::ROLE_SERVER, attr);
           service_manager_->Notify(msg);
           change_signal_(msg);
         });
    previous = std::move(current);
    try {
      node->wait_for_graph_change(event, std::chrono::milliseconds(500));
    } catch (const std::exception& e) {
      ADEBUG << "wait_for_graph_change: " << e.what();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    event->check_and_clear();
  }
}

}  // namespace service_discovery
}  // namespace cyber
}  // namespace apollo
