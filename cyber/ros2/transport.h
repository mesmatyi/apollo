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
 * @file transport.h
 * @brief Type-erased ROS 2 endpoints backing the Cyber Reader/Writer/Service/
 * Client/Timer templates.
 *
 * Everything that touches rclcpp lives in transport.cc, so Apollo module
 * translation units that include cyber headers do not pay for rclcpp headers.
 * Messages cross this boundary as std::shared_ptr<void> plus a TypeSupport
 * table that knows how to (de)serialize the concrete C++ type.
 */

#ifndef CYBER_ROS2_TRANSPORT_H_
#define CYBER_ROS2_TRANSPORT_H_

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include "cyber/proto/qos_profile.pb.h"
#include "cyber/proto/role_attributes.pb.h"

#include "cyber/message/message_traits.h"

namespace rclcpp {
class Node;
}  // namespace rclcpp

namespace apollo {
namespace cyber {
namespace ros2 {

/// (De)serialization table of one concrete message type.
struct TypeSupport {
  std::string type_name;
  std::type_index type;
  /// true for message::RawMessage (bytes pass through untouched)
  bool is_raw;
  std::function<bool(const void* msg, std::vector<uint8_t>* out)> serialize;
  std::function<std::shared_ptr<void>(const uint8_t* data, size_t size)> parse;
};

template <typename MessageT>
const TypeSupport* GetTypeSupport() {
  static const TypeSupport ts{
      message::MessageType<MessageT>(), std::type_index(typeid(MessageT)),
      std::is_same<MessageT, message::RawMessage>::value,
      [](const void* msg, std::vector<uint8_t>* out) -> bool {
        const auto& m = *static_cast<const MessageT*>(msg);
        int size = message::ByteSize(m);
        if (size < 0) {
          return false;
        }
        out->resize(static_cast<size_t>(size));
        return size == 0 ||
               message::SerializeToArray(m, out->data(), size);
      },
      [](const uint8_t* data, size_t size) -> std::shared_ptr<void> {
        auto m = std::make_shared<MessageT>();
        if (!message::ParseFromArray(data, static_cast<int>(size), m.get())) {
          return nullptr;
        }
        return m;
      }};
  return &ts;
}

/**
 * @class NodeHandle
 * @brief A ROS 2 node used by the Cyber API.
 *
 * Either owned (created by Cyber and spun by the background executor of
 * Runtime) or wrapped (provided by an rclcpp_components container, which
 * spins it).
 */
class NodeHandle {
 public:
  /// Creates a node spun by the Cyber background executor.
  static std::shared_ptr<NodeHandle> Create(const std::string& name,
                                            const std::string& name_space);
  /// Wraps a node spun by somebody else (e.g. a component container).
  static std::shared_ptr<NodeHandle> Wrap(std::shared_ptr<rclcpp::Node> node);

  ~NodeHandle();

  const std::shared_ptr<rclcpp::Node>& ros_node() const { return node_; }
  const std::string& name() const { return name_; }
  bool owned() const { return owned_; }

 private:
  NodeHandle() = default;
  std::shared_ptr<rclcpp::Node> node_;
  std::string name_;
  bool owned_ = false;
};

/// Common introspection of a pub/sub endpoint.
class Endpoint {
 public:
  virtual ~Endpoint() = default;
  virtual const std::string& topic() const = 0;
  /// Matched peers (local intra-process peers and remote ROS 2 peers).
  virtual size_t PeerCount() const = 0;
  /// Describes the matched peers as Cyber RoleAttributes.
  virtual void GetPeers(std::vector<proto::RoleAttributes>* peers) const = 0;
  virtual void Shutdown() = 0;
};

class PublisherEndpoint : public Endpoint {
 public:
  static std::shared_ptr<PublisherEndpoint> Create(
      const std::shared_ptr<NodeHandle>& node,
      const proto::RoleAttributes& role_attr, const TypeSupport* type_support);
  /// Publishes a message. In-process readers receive `msg` itself (zero
  /// copy); remote readers receive it through ROS 2.
  virtual bool Publish(const std::shared_ptr<const void>& msg) = 0;
};

class SubscriberEndpoint : public Endpoint {
 public:
  using Callback = std::function<void(const std::shared_ptr<void>&)>;
  static std::shared_ptr<SubscriberEndpoint> Create(
      const std::shared_ptr<NodeHandle>& node,
      const proto::RoleAttributes& role_attr, const TypeSupport* type_support,
      uint32_t pending_queue_size, Callback callback);
};

class ServiceEndpoint {
 public:
  /// Returns false when the request could not be handled.
  using Handler = std::function<bool(const uint8_t* request, size_t size,
                                     std::vector<uint8_t>* response)>;
  static std::shared_ptr<ServiceEndpoint> Create(
      const std::shared_ptr<NodeHandle>& node, const std::string& service_name,
      const std::string& request_type, const std::string& response_type,
      Handler handler);
  virtual ~ServiceEndpoint() = default;
  virtual void Shutdown() = 0;
};

class ClientEndpoint {
 public:
  using ResponseCallback =
      std::function<void(bool ok, const std::vector<uint8_t>& response)>;
  static std::shared_ptr<ClientEndpoint> Create(
      const std::shared_ptr<NodeHandle>& node, const std::string& service_name,
      const std::string& request_type);
  virtual ~ClientEndpoint() = default;
  /// Blocking call; false on timeout or failure.
  virtual bool Call(std::vector<uint8_t>&& request,
                    std::vector<uint8_t>* response,
                    std::chrono::nanoseconds timeout) = 0;
  /// Asynchronous call; `callback` runs on a background executor thread.
  virtual void AsyncCall(std::vector<uint8_t>&& request,
                         ResponseCallback callback) = 0;
  virtual bool ServiceIsReady() const = 0;
  /// Negative timeout waits forever.
  virtual bool WaitForService(std::chrono::nanoseconds timeout) = 0;
};

class TimerEndpoint {
 public:
  /// Creates a wall timer. When `node` is null the Runtime aux node is used.
  static std::shared_ptr<TimerEndpoint> Create(
      const std::shared_ptr<NodeHandle>& node, std::chrono::nanoseconds period,
      std::function<void()> callback, bool oneshot);
  virtual ~TimerEndpoint() = default;
  virtual void Cancel() = 0;
};

/// Remembers the protobuf type of a channel used in this process (the ROS 2
/// graph only knows the wire type apollo_msgs/msg/ProtoMessage).
void RegisterLocalChannelType(const std::string& channel,
                              const std::string& type_name);
bool LookupLocalChannelType(const std::string& channel, std::string* type_name);

/// The process-wide helper node (spun by the Runtime background executor).
std::shared_ptr<NodeHandle> AuxNodeHandle();

/// Number of in-process readers on a topic (used for diagnostics/tests).
size_t LocalSubscriberCount(const std::string& topic);

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_ROS2_TRANSPORT_H_
