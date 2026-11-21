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

#ifndef CYBER_NODE_NODE_H_
#define CYBER_NODE_NODE_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "cyber/common/global_data.h"
#include "cyber/message/message_traits.h"
#include "cyber/node/node_channel_impl.h"
#include "cyber/service/client.h"
#include "cyber/service/service.h"
#include "cyber/transport/qos/qos_profile_conf.h"

namespace rclcpp {
class Node;
}  // namespace rclcpp

namespace apollo {
namespace cyber {

template <typename M0, typename M1, typename M2, typename M3>
class Component;
class TimerComponent;

/**
 * @class Node
 * @brief The Cyber node, backed by an rclcpp::Node.
 *
 * Nodes created by name are spun by the Cyber background executor. A Node
 * can also wrap an rclcpp::Node owned by someone else (for example a
 * component loaded into a ROS 2 component container), in which case that
 * owner spins it.
 */
class Node {
 public:
  template <typename M0, typename M1, typename M2, typename M3>
  friend class Component;
  friend class TimerComponent;
  friend bool Init(const char*, const std::string&);
  friend std::unique_ptr<Node> CreateNode(const std::string&,
                                          const std::string&);

  explicit Node(const std::string& node_name,
                const std::string& name_space = "");
  /// Wraps an existing rclcpp node (not spun by Cyber).
  explicit Node(const std::shared_ptr<rclcpp::Node>& ros_node);
  virtual ~Node();

  /// Cyber node name (as given, before ROS 2 name sanitizing).
  const std::string& Name() const;

  /// The underlying ROS 2 node, for mixing in native rclcpp entities.
  std::shared_ptr<rclcpp::Node> RosNode() const;

  template <typename MessageT>
  auto CreateWriter(const proto::RoleAttributes& role_attr)
      -> std::shared_ptr<Writer<MessageT>>;

  template <typename MessageT>
  auto CreateWriter(const std::string& channel_name)
      -> std::shared_ptr<Writer<MessageT>>;

  template <typename MessageT>
  auto CreateReader(const std::string& channel_name,
                    const CallbackFunc<MessageT>& reader_func = nullptr)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  template <typename MessageT>
  auto CreateReader(const ReaderConfig& config,
                    const CallbackFunc<MessageT>& reader_func = nullptr)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  template <typename MessageT>
  auto CreateReader(const proto::RoleAttributes& role_attr,
                    const CallbackFunc<MessageT>& reader_func = nullptr)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  template <typename Request, typename Response>
  auto CreateService(const std::string& service_name,
                     const typename Service<Request, Response>::ServiceCallback&
                         service_callback)
      -> std::shared_ptr<Service<Request, Response>>;

  template <typename Request, typename Response>
  auto CreateClient(const std::string& service_name)
      -> std::shared_ptr<Client<Request, Response>>;

  bool DeleteReader(const std::string& channel_name);
  bool DeleteReader(const ReaderConfig& config);
  bool DeleteReader(const proto::RoleAttributes& role_attr);

  /// Copies the received history of every reader into its observed queue.
  void Observe();
  /// Clears the history of every reader.
  void ClearData();

  template <typename MessageT>
  auto GetReader(const std::string& channel_name)
      -> std::shared_ptr<Reader<MessageT>>;

  /// Internal: the ROS 2 node handle used by this node's entities.
  const std::shared_ptr<ros2::NodeHandle>& Handle() const { return handle_; }

 private:
  template <typename MessageT>
  void FillInAttr(const std::string& channel_name,
                  proto::RoleAttributes* attr) const;
  template <typename MessageT>
  void FillInAttr(proto::RoleAttributes* attr) const;

  template <typename MessageT>
  auto CreateReaderImpl(const proto::RoleAttributes& role_attr,
                        const CallbackFunc<MessageT>& reader_func,
                        uint32_t pending_queue_size)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  std::string node_name_;
  std::string name_space_;
  std::shared_ptr<ros2::NodeHandle> handle_;

  std::mutex readers_mutex_;
  std::map<std::string, std::shared_ptr<ReaderBase>> readers_;
};

template <typename MessageT>
void Node::FillInAttr(proto::RoleAttributes* attr) const {
  attr->set_host_name(common::GlobalData::Instance()->HostName());
  attr->set_host_ip(common::GlobalData::Instance()->HostIp());
  attr->set_process_id(common::GlobalData::Instance()->ProcessId());
  attr->set_node_name(node_name_);
  attr->set_node_id(common::GlobalData::RegisterNode(node_name_));
  attr->set_channel_id(
      common::GlobalData::RegisterChannel(attr->channel_name()));
  if (attr->message_type().empty()) {
    attr->set_message_type(message::MessageType<MessageT>());
  }
  if (!attr->has_qos_profile()) {
    attr->mutable_qos_profile()->CopyFrom(
        transport::QosProfileConf::QOS_PROFILE_DEFAULT);
  }
}

template <typename MessageT>
void Node::FillInAttr(const std::string& channel_name,
                      proto::RoleAttributes* attr) const {
  attr->set_channel_name(channel_name);
  FillInAttr<MessageT>(attr);
}

template <typename MessageT>
auto Node::CreateWriter(const proto::RoleAttributes& role_attr)
    -> std::shared_ptr<Writer<MessageT>> {
  if (!role_attr.has_channel_name() || role_attr.channel_name().empty()) {
    AERROR << "Can't create a writer with empty channel name!";
    return nullptr;
  }
  proto::RoleAttributes attr(role_attr);
  FillInAttr<MessageT>(&attr);
  auto writer = std::make_shared<Writer<MessageT>>(attr);
  writer->SetNodeHandle(handle_);
  if (!writer->Init()) {
    return nullptr;
  }
  return writer;
}

template <typename MessageT>
auto Node::CreateWriter(const std::string& channel_name)
    -> std::shared_ptr<Writer<MessageT>> {
  proto::RoleAttributes role_attr;
  role_attr.set_channel_name(channel_name);
  return CreateWriter<MessageT>(role_attr);
}

template <typename MessageT>
auto Node::CreateReaderImpl(const proto::RoleAttributes& role_attr,
                            const CallbackFunc<MessageT>& reader_func,
                            uint32_t pending_queue_size)
    -> std::shared_ptr<cyber::Reader<MessageT>> {
  if (!role_attr.has_channel_name() || role_attr.channel_name().empty()) {
    AERROR << "Can't create a reader with empty channel name!";
    return nullptr;
  }
  std::lock_guard<std::mutex> lg(readers_mutex_);
  if (readers_.find(role_attr.channel_name()) != readers_.end()) {
    AWARN << "Failed to create reader: reader with the same channel already "
             "exists.";
    return nullptr;
  }
  proto::RoleAttributes attr(role_attr);
  FillInAttr<MessageT>(&attr);
  auto reader = std::make_shared<Reader<MessageT>>(attr, reader_func,
                                                   pending_queue_size);
  reader->SetNodeHandle(handle_);
  if (!reader->Init()) {
    return nullptr;
  }
  readers_.emplace(std::make_pair(attr.channel_name(), reader));
  return reader;
}

template <typename MessageT>
auto Node::CreateReader(const proto::RoleAttributes& role_attr,
                        const CallbackFunc<MessageT>& reader_func)
    -> std::shared_ptr<Reader<MessageT>> {
  return CreateReaderImpl<MessageT>(role_attr, reader_func,
                                    DEFAULT_PENDING_QUEUE_SIZE);
}

template <typename MessageT>
auto Node::CreateReader(const ReaderConfig& config,
                        const CallbackFunc<MessageT>& reader_func)
    -> std::shared_ptr<cyber::Reader<MessageT>> {
  proto::RoleAttributes role_attr;
  role_attr.set_channel_name(config.channel_name);
  role_attr.mutable_qos_profile()->CopyFrom(config.qos_profile);
  return CreateReaderImpl<MessageT>(role_attr, reader_func,
                                    config.pending_queue_size);
}

template <typename MessageT>
auto Node::CreateReader(const std::string& channel_name,
                        const CallbackFunc<MessageT>& reader_func)
    -> std::shared_ptr<Reader<MessageT>> {
  proto::RoleAttributes role_attr;
  role_attr.set_channel_name(channel_name);
  return CreateReaderImpl<MessageT>(role_attr, reader_func,
                                    DEFAULT_PENDING_QUEUE_SIZE);
}

template <typename Request, typename Response>
auto Node::CreateService(
    const std::string& service_name,
    const typename Service<Request, Response>::ServiceCallback&
        service_callback) -> std::shared_ptr<Service<Request, Response>> {
  auto service = std::make_shared<Service<Request, Response>>(
      node_name_, service_name, service_callback);
  service->SetNodeHandle(handle_);
  if (!service->Init()) {
    AERROR << "Create service [" << service_name << "] failed.";
    return nullptr;
  }
  return service;
}

template <typename Request, typename Response>
auto Node::CreateClient(const std::string& service_name)
    -> std::shared_ptr<Client<Request, Response>> {
  auto client =
      std::make_shared<Client<Request, Response>>(node_name_, service_name);
  client->SetNodeHandle(handle_);
  if (!client->Init()) {
    AERROR << "Create client [" << service_name << "] failed.";
    return nullptr;
  }
  return client;
}

template <typename MessageT>
auto Node::GetReader(const std::string& name)
    -> std::shared_ptr<Reader<MessageT>> {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  auto it = readers_.find(name);
  if (it != readers_.end()) {
    return std::dynamic_pointer_cast<Reader<MessageT>>(it->second);
  }
  return nullptr;
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_NODE_NODE_H_
