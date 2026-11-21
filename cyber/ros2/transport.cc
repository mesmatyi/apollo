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

#include "cyber/ros2/transport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#include "apollo_msgs/msg/proto_message.hpp"
#include "apollo_msgs/srv/proto_service.hpp"
#include "rcl/service.h"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/typesupport_helpers.hpp"

#include "cyber/common/log.h"
#include "cyber/ros2/naming.h"
#include "cyber/ros2/native_types.h"
#include "cyber/ros2/runtime.h"
#include "cyber/task/task_manager.h"

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

using ProtoMessage = apollo_msgs::msg::ProtoMessage;
using ProtoService = apollo_msgs::srv::ProtoService;

const std::string& RawTypeName() {
  static const std::string name =
      message::MessageType<message::RawMessage>();
  return name;
}

size_t WriterDepth(const proto::QosProfile& p) {
  size_t depth = p.depth() > 0 ? p.depth() : 1;
  if (p.durability() != proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
    // Volatile writers only use their history for retransmissions; keep a
    // small buffer so bursts are not dropped before reliable delivery.
    depth = std::max<size_t>(depth, 10);
  }
  return depth;
}

rclcpp::QoS MakeQos(const proto::QosProfile& p, size_t depth) {
  rclcpp::QoS qos{rclcpp::KeepLast(std::max<size_t>(depth, 1))};
  if (p.history() == proto::QosHistoryPolicy::HISTORY_KEEP_ALL) {
    qos.keep_all();
  }
  if (p.reliability() == proto::QosReliabilityPolicy::RELIABILITY_BEST_EFFORT) {
    qos.best_effort();
  } else {
    qos.reliable();
  }
  if (p.durability() == proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
    qos.transient_local();
  } else {
    qos.durability_volatile();
  }
  return qos;
}

constexpr char kProtoMessageType[] = "apollo_msgs/msg/ProtoMessage";
constexpr char kProtoServiceType[] = "apollo_msgs/srv/ProtoService";

/// How a channel is put on the wire: a native ROS 2 type (converter set) or
/// serialized protobuf in apollo_msgs/msg/ProtoMessage.
struct Wire {
  std::shared_ptr<const MessageConverter> converter;
  const google::protobuf::Descriptor* descriptor = nullptr;
};

Wire SelectWire(const std::string& channel, const TypeSupport* ts,
                const std::string& proto_type) {
  Wire wire;
  auto* types = NativeTypes::Instance();
  if (types->UseProtoWire(channel)) {
    return wire;
  }
  const google::protobuf::Descriptor* descriptor = ts->descriptor();
  if (descriptor == nullptr && ts->is_raw && !proto_type.empty()) {
    descriptor = types->FindDescriptor(proto_type);
  }
  if (descriptor == nullptr) {
    return wire;
  }
  wire.converter = MessageConverter::Get(descriptor);
  if (wire.converter != nullptr) {
    wire.descriptor = descriptor;
  }
  return wire;
}

/// Protobuf type carried by a ROS 2 topic type, empty when unknown.
std::string ProtoTypeOf(const std::string& ros_type,
                        const std::string& channel) {
  if (const auto* info = NativeTypes::Instance()->FindByRosType(ros_type)) {
    return info->proto_name;
  }
  std::string type;
  LookupLocalChannelType(channel, &type);
  return type;
}

proto::RoleAttributes ToRoleAttributes(const rclcpp::TopicEndpointInfo& info,
                                       const std::string& channel) {
  proto::RoleAttributes attr;
  std::string ns = info.node_namespace();
  std::string node = info.node_name();
  if (!ns.empty() && ns != "/") {
    node = ns.substr(ns.front() == '/' ? 1 : 0) + "/" + node;
  }
  attr.set_node_name(node);
  attr.set_channel_name(channel);
  const std::string proto_type = ProtoTypeOf(info.topic_type(), channel);
  attr.set_message_type(proto_type.empty() ? info.topic_type() : proto_type);
  auto* qos = attr.mutable_qos_profile();
  const auto rmw = info.qos_profile().get_rmw_qos_profile();
  qos->set_depth(static_cast<uint32_t>(rmw.depth));
  qos->set_reliability(rmw.reliability == RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT
                           ? proto::QosReliabilityPolicy::RELIABILITY_BEST_EFFORT
                           : proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
  qos->set_durability(
      rmw.durability == RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL
          ? proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL
          : proto::QosDurabilityPolicy::DURABILITY_VOLATILE);
  qos->set_history(rmw.history == RMW_QOS_POLICY_HISTORY_KEEP_ALL
                       ? proto::QosHistoryPolicy::HISTORY_KEEP_ALL
                       : proto::QosHistoryPolicy::HISTORY_KEEP_LAST);
  return attr;
}

// ---------------------------------------------------------------------------
// Intra-process delivery
// ---------------------------------------------------------------------------

/**
 * Receives messages published in this process without serialization. It is
 * an rclcpp::Waitable, so the callback runs on whatever executor spins the
 * owning node, inside the reader's mutually exclusive callback group.
 */
class IntraSubscription : public rclcpp::Waitable {
 public:
  IntraSubscription(rclcpp::Context::SharedPtr context,
                    const TypeSupport* type_support, size_t depth,
                    SubscriberEndpoint::Callback callback)
      : gc_(context),
        type_support_(type_support),
        depth_(depth),
        callback_(std::move(callback)) {}

  size_t get_number_of_ready_guard_conditions() override { return 1; }

  void add_to_wait_set(rcl_wait_set_t& wait_set) override {
    gc_.add_to_wait_set(wait_set);
  }

  bool is_ready(const rcl_wait_set_t& wait_set) override {
    (void)wait_set;
    std::lock_guard<std::mutex> lock(mutex_);
    return !queue_.empty();
  }

  std::shared_ptr<void> take_data() override {
    std::shared_ptr<void> msg;
    bool more = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) {
        return nullptr;
      }
      msg = std::move(queue_.front());
      queue_.pop_front();
      more = !queue_.empty();
    }
    if (more) {
      gc_.trigger();
    }
    return std::make_shared<std::shared_ptr<void>>(std::move(msg));
  }

  std::shared_ptr<void> take_data_by_entity_id(size_t id) override {
    (void)id;
    return take_data();
  }

  void execute(const std::shared_ptr<void>& data) override {
    if (!data || !active_.load()) {
      return;
    }
    auto msg = *std::static_pointer_cast<std::shared_ptr<void>>(data);
    if (msg) {
      callback_(msg);
    }
    // With a MultiThreadedExecutor another thread may have consumed the
    // guard-condition trigger while this reader's (mutually exclusive)
    // callback group was busy; wake the executor again if work remains.
    bool more = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      more = !queue_.empty();
    }
    if (more) {
      gc_.trigger();
    }
  }

  void set_on_ready_callback(
      std::function<void(size_t, int)> callback) override {
    std::lock_guard<std::mutex> lock(mutex_);
    on_ready_ = std::move(callback);
    if (on_ready_ && unread_ > 0) {
      on_ready_(unread_, 0);
      unread_ = 0;
    }
  }

  void clear_on_ready_callback() override {
    std::lock_guard<std::mutex> lock(mutex_);
    on_ready_ = nullptr;
  }

  /// Hands a message published in this process to this reader.
  void Deliver(const std::shared_ptr<const void>& msg,
               const TypeSupport* publisher_type, std::vector<uint8_t>* bytes,
               bool* serialized) {
    if (!active_.load()) {
      return;
    }
    std::shared_ptr<void> out;
    if (publisher_type->type == type_support_->type) {
      out = std::const_pointer_cast<void>(msg);
    } else {
      if (!*serialized) {
        if (!publisher_type->serialize(msg.get(), bytes)) {
          return;
        }
        *serialized = true;
      }
      out = type_support_->parse(bytes->data(), bytes->size());
      if (!out) {
        return;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push_back(std::move(out));
      while (depth_ > 0 && queue_.size() > depth_) {
        queue_.pop_front();
      }
      if (on_ready_) {
        on_ready_(1, 0);
      } else {
        ++unread_;
      }
    }
    gc_.trigger();
  }

  void Deactivate() {
    active_.store(false);
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
  }

  /// ROS 2 type of the reader's own subscription (empty while it has none).
  void SetRosType(const std::string& ros_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    ros_type_ = ros_type;
  }

  bool HasRosType(const std::string& ros_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    return ros_type_ == ros_type;
  }

 private:
  rclcpp::GuardCondition gc_;
  const TypeSupport* type_support_;
  size_t depth_;
  SubscriberEndpoint::Callback callback_;
  std::atomic<bool> active_{true};
  std::mutex mutex_;
  std::deque<std::shared_ptr<void>> queue_;
  std::function<void(size_t, int)> on_ready_;
  size_t unread_ = 0;
  std::string ros_type_;
};

class IntraProcessManager {
 public:
  using Subscriptions = std::vector<std::shared_ptr<IntraSubscription>>;

  static IntraProcessManager* Instance() {
    static IntraProcessManager* instance = new IntraProcessManager();
    return instance;
  }

  /// Registers a reader. A reader with transient-local durability first
  /// receives what the latched writers of the topic published before it
  /// joined, as it would from the middleware.
  void Add(const std::string& topic,
           const std::shared_ptr<IntraSubscription>& sub, bool replay_latched) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_[topic].push_back(sub);
    if (!replay_latched) {
      return;
    }
    auto it = latched_.find(topic);
    if (it == latched_.end()) {
      return;
    }
    for (auto& writer : it->second) {
      for (auto& msg : writer.second.messages) {
        std::vector<uint8_t> bytes;
        bool serialized = false;
        sub->Deliver(msg, writer.second.type_support, &bytes, &serialized);
      }
    }
  }

  void Remove(const std::string& topic, const IntraSubscription* sub) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subs_.find(topic);
    if (it == subs_.end()) {
      return;
    }
    auto& v = it->second;
    v.erase(std::remove_if(v.begin(), v.end(),
                           [sub](const std::weak_ptr<IntraSubscription>& w) {
                             auto s = w.lock();
                             return !s || s.get() == sub;
                           }),
            v.end());
    if (v.empty()) {
      subs_.erase(it);
    }
  }

  Subscriptions Get(const std::string& topic) {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetLocked(topic);
  }

  /// Get() for a latched (transient-local) writer: also keeps `msg` in the
  /// writer's history (at most `depth` messages) for readers that join later.
  Subscriptions GetAndLatch(const std::string& topic, const void* writer,
                            const TypeSupport* type_support, size_t depth,
                            const std::shared_ptr<const void>& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& history = latched_[topic][writer];
    history.type_support = type_support;
    history.messages.push_back(msg);
    while (history.messages.size() > std::max<size_t>(depth, 1)) {
      history.messages.pop_front();
    }
    return GetLocked(topic);
  }

  void ForgetWriter(const std::string& topic, const void* writer) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = latched_.find(topic);
    if (it == latched_.end()) {
      return;
    }
    it->second.erase(writer);
    if (it->second.empty()) {
      latched_.erase(it);
    }
  }

 private:
  struct History {
    const TypeSupport* type_support = nullptr;
    std::deque<std::shared_ptr<const void>> messages;
  };

  Subscriptions GetLocked(const std::string& topic) {
    Subscriptions result;
    auto it = subs_.find(topic);
    if (it == subs_.end()) {
      return result;
    }
    result.reserve(it->second.size());
    for (auto& w : it->second) {
      if (auto s = w.lock()) {
        result.push_back(std::move(s));
      }
    }
    return result;
  }

  std::mutex mutex_;
  std::unordered_map<std::string, std::vector<std::weak_ptr<IntraSubscription>>>
      subs_;
  std::unordered_map<std::string, std::map<const void*, History>> latched_;
};

/**
 * The ROS 2 publishers that belong to Cyber writers of this process. Their
 * messages reach local readers through the intra-process path, so a reader
 * drops the copy that comes back from the middleware. Messages of any other
 * publisher of the process (a plain rclcpp node composed into the same
 * container, for example) are delivered like remote ones.
 */
class LocalPublishers {
 public:
  static LocalPublishers* Instance() {
    static LocalPublishers* instance = new LocalPublishers();
    return instance;
  }

  void Add(const rmw_gid_t& gid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    gids_.insert(Key(gid));
  }

  void Remove(const rmw_gid_t& gid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    gids_.erase(Key(gid));
  }

  bool Contains(const rmw_gid_t& gid) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return gids_.count(Key(gid)) != 0;
  }

 private:
  using GidKey = std::array<uint8_t, RMW_GID_STORAGE_SIZE>;

  static GidKey Key(const rmw_gid_t& gid) {
    GidKey key;
    std::memcpy(key.data(), gid.data, RMW_GID_STORAGE_SIZE);
    return key;
  }

  std::shared_mutex mutex_;
  std::set<GidKey> gids_;
};

bool FromLocalWriter(const rclcpp::MessageInfo& info) {
  return LocalPublishers::Instance()->Contains(
      info.get_rmw_message_info().publisher_gid);
}

// ---------------------------------------------------------------------------
// Publisher
// ---------------------------------------------------------------------------

class PublisherImpl : public PublisherEndpoint {
 public:
  PublisherImpl(const std::shared_ptr<NodeHandle>& node,
                const proto::RoleAttributes& role_attr,
                const TypeSupport* type_support)
      : node_(node),
        channel_(role_attr.channel_name()),
        topic_(ChannelToTopic(role_attr.channel_name())),
        type_support_(type_support) {
    type_name_ = role_attr.message_type().empty() ? type_support->type_name
                                                  : role_attr.message_type();
    RegisterLocalChannelType(channel_, type_name_);
    wire_ = SelectWire(channel_, type_support, type_name_);
    const auto& p = role_attr.qos_profile();
    transient_local_ =
        p.durability() == proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL;
    rclcpp::PublisherOptions options;
    options.use_intra_process_comm = rclcpp::IntraProcessSetting::Disable;
    const std::string topic = topic_;
    options.event_callbacks.incompatible_qos_callback =
        [topic](rclcpp::QOSOfferedIncompatibleQoSInfo& info) {
          AWARN << "[" << topic
                << "] a reader requested an incompatible QoS (policy id "
                << info.last_policy_kind << ")";
        };
    const rclcpp::QoS qos = MakeQos(p, WriterDepth(p));
    if (wire_.converter != nullptr) {
      ros_type_ = wire_.converter->ros_type();
      native_publisher_ = node->ros_node()->create_generic_publisher(
          topic_, ros_type_, qos, options);
      publisher_ = native_publisher_;
    } else {
      ros_type_ = kProtoMessageType;
      proto_publisher_ =
          node->ros_node()->create_publisher<ProtoMessage>(topic_, qos, options);
      publisher_ = proto_publisher_;
    }
    latch_depth_ = p.depth() > 0 ? p.depth() : 1;
    gid_ = publisher_->get_gid();
    LocalPublishers::Instance()->Add(gid_);
  }

  ~PublisherImpl() override { Shutdown(); }

  const std::string& topic() const override { return topic_; }

  bool Publish(const std::shared_ptr<const void>& msg) override {
    if (!msg || shutdown_.load()) {
      return false;
    }
    std::vector<uint8_t> bytes;
    bool serialized = false;
    auto locals =
        transient_local_
            ? IntraProcessManager::Instance()->GetAndLatch(
                  topic_, this, type_support_, latch_depth_, msg)
            : IntraProcessManager::Instance()->Get(topic_);
    size_t local_matches = 0;
    for (auto& sub : locals) {
      sub->Deliver(msg, type_support_, &bytes, &serialized);
      if (sub->HasRosType(ros_type_)) {
        ++local_matches;
      }
    }
    // An in-process reader also owns a ROS subscription (which ignores local
    // publications); it is matched with this publisher when it uses the same
    // ROS type. Only serialize when somebody else listens.
    size_t matched = publisher_->get_subscription_count();
    if (!transient_local_ && matched <= local_matches) {
      return true;
    }
    if (native_publisher_ != nullptr) {
      return PublishNative(msg, &bytes, serialized);
    }
    auto ros_msg = std::make_unique<ProtoMessage>();
    ros_msg->type = type_name_;
    if (serialized) {
      ros_msg->data = std::move(bytes);
    } else if (!type_support_->serialize(msg.get(), &ros_msg->data)) {
      AERROR << "[" << topic_ << "] failed to serialize " << type_name_;
      return false;
    }
    proto_publisher_->publish(std::move(ros_msg));
    return true;
  }

  size_t PeerCount() const override {
    return shutdown_.load() ? 0 : node_->ros_node()->count_subscribers(topic_);
  }

  void GetPeers(std::vector<proto::RoleAttributes>* peers) const override {
    if (peers == nullptr || shutdown_.load()) {
      return;
    }
    for (auto& info : node_->ros_node()->get_subscriptions_info_by_topic(topic_)) {
      peers->push_back(ToRoleAttributes(info, channel_));
    }
  }

  void Shutdown() override {
    if (shutdown_.exchange(true)) {
      return;
    }
    if (transient_local_) {
      IntraProcessManager::Instance()->ForgetWriter(topic_, this);
    }
    publisher_.reset();
    proto_publisher_.reset();
    native_publisher_.reset();
    LocalPublishers::Instance()->Remove(gid_);
  }

 private:
  /// Converts the message to its native ROS 2 type and publishes it.
  bool PublishNative(const std::shared_ptr<const void>& msg,
                     std::vector<uint8_t>* bytes, bool serialized) {
    rclcpp::SerializedMessage out;
    bool ok = false;
    if (const google::protobuf::Message* proto =
            type_support_->as_proto(msg.get())) {
      ok = wire_.converter->ToSerialized(*proto, &out);
    } else {
      // Raw writer: the payload is serialized protobuf of the channel type.
      if (!serialized && !type_support_->serialize(msg.get(), bytes)) {
        AERROR << "[" << topic_ << "] failed to serialize " << type_name_;
        return false;
      }
      auto proto_msg = NativeTypes::Instance()->NewMessage(wire_.descriptor);
      ok = proto_msg != nullptr &&
           proto_msg->ParsePartialFromArray(bytes->data(),
                                            static_cast<int>(bytes->size())) &&
           wire_.converter->ToSerialized(*proto_msg, &out);
    }
    if (!ok) {
      AERROR << "[" << topic_ << "] failed to convert " << type_name_ << " to "
             << wire_.converter->ros_type();
      return false;
    }
    native_publisher_->publish(out);
    return true;
  }

  std::shared_ptr<NodeHandle> node_;
  std::string channel_;
  std::string topic_;
  std::string type_name_;
  std::string ros_type_;
  const TypeSupport* type_support_;
  Wire wire_;
  bool transient_local_ = false;
  size_t latch_depth_ = 1;
  rmw_gid_t gid_{};
  std::atomic<bool> shutdown_{false};
  rclcpp::PublisherBase::SharedPtr publisher_;
  rclcpp::Publisher<ProtoMessage>::SharedPtr proto_publisher_;
  rclcpp::GenericPublisher::SharedPtr native_publisher_;
};

// ---------------------------------------------------------------------------
// Subscriber
// ---------------------------------------------------------------------------

class SubscriberImpl : public SubscriberEndpoint {
 public:
  SubscriberImpl(const std::shared_ptr<NodeHandle>& node,
                 const proto::RoleAttributes& role_attr,
                 const TypeSupport* type_support, uint32_t pending_queue_size,
                 Callback callback)
      : node_(node),
        channel_(role_attr.channel_name()),
        topic_(ChannelToTopic(role_attr.channel_name())),
        type_support_(type_support),
        callback_(std::move(callback)),
        qos_profile_(role_attr.qos_profile()),
        active_(std::make_shared<std::atomic<bool>>(true)) {
    RegisterLocalChannelType(channel_, type_support->type_name);
    depth_ = std::max<uint32_t>(pending_queue_size, 1);
    if (qos_profile_.durability() ==
        proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
      depth_ = std::max<size_t>(depth_, qos_profile_.depth());
    }
    auto ros_node = node->ros_node();
    group_ = ros_node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive, true);

    intra_ = std::make_shared<IntraSubscription>(
        ros_node->get_node_base_interface()->get_context(), type_support,
        depth_, callback_);
    ros_node->get_node_waitables_interface()->add_waitable(intra_, group_);
    IntraProcessManager::Instance()->Add(
        topic_, intra_,
        qos_profile_.durability() ==
            proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL);

    std::lock_guard<std::mutex> lock(*mutex_);
    if (!TrySubscribe()) {
      // A raw reader does not know the type of its channel. Wait until a
      // writer shows up in the ROS 2 graph and subscribe with its type.
      // The callback only touches `this` under the shared mutex and while
      // the reader is active, so it is safe against a concurrent Shutdown().
      auto active = active_;
      auto mutex = mutex_;
      resolve_timer_ = ros_node->create_wall_timer(
          std::chrono::milliseconds(100),
          [this, active, mutex]() {
            std::lock_guard<std::mutex> lock(*mutex);
            if (active->load() && TrySubscribe() && resolve_timer_) {
              resolve_timer_->cancel();
            }
          },
          group_);
    }
  }

  ~SubscriberImpl() override { Shutdown(); }

  const std::string& topic() const override { return topic_; }

  size_t PeerCount() const override {
    return active_->load() ? node_->ros_node()->count_publishers(topic_) : 0;
  }

  void GetPeers(std::vector<proto::RoleAttributes>* peers) const override {
    if (peers == nullptr || !active_->load()) {
      return;
    }
    for (auto& info : node_->ros_node()->get_publishers_info_by_topic(topic_)) {
      peers->push_back(ToRoleAttributes(info, channel_));
    }
  }

  void Shutdown() override {
    if (!active_->exchange(false)) {
      return;
    }
    IntraProcessManager::Instance()->Remove(topic_, intra_.get());
    intra_->Deactivate();
    try {
      node_->ros_node()->get_node_waitables_interface()->remove_waitable(
          intra_, group_);
    } catch (const std::exception& e) {
      ADEBUG << "remove_waitable: " << e.what();
    }
    std::lock_guard<std::mutex> lock(*mutex_);
    if (resolve_timer_) {
      resolve_timer_->cancel();
      resolve_timer_.reset();
    }
    subscription_.reset();
    intra_->SetRosType("");
  }

 private:
  /// Determines the wire format of the channel. Typed readers know it from
  /// their message type; raw readers take it from a writer in this process
  /// or from the ROS 2 graph. Returns false while it is still unknown.
  bool ResolveWire(Wire* wire, bool* proto_wire) {
    *proto_wire = false;
    if (!type_support_->is_raw) {
      *wire = SelectWire(channel_, type_support_, type_support_->type_name);
      *proto_wire = wire->converter == nullptr;
      return true;
    }
    // A raw reader (recorder, monitor, bridge) follows whatever the writers
    // of the channel use: first the ROS 2 graph, then the types this process
    // knows about.
    const auto topics = node_->ros_node()->get_topic_names_and_types();
    const auto it = topics.find(topic_);
    if (it != topics.end()) {
      bool has_proto_wire = false;
      for (const auto& ros_type : it->second) {
        if (ros_type == kProtoMessageType) {
          has_proto_wire = true;
          continue;
        }
        const auto* info = NativeTypes::Instance()->FindByRosType(ros_type);
        if (info != nullptr) {
          *wire = SelectWire(channel_, type_support_, info->proto_name);
          if (wire->converter != nullptr) {
            RegisterLocalChannelType(channel_, info->proto_name);
            return true;
          }
        }
      }
      if (has_proto_wire) {
        *proto_wire = true;
        return true;
      }
    }
    std::string local_type;
    if (LookupLocalChannelType(channel_, &local_type)) {
      *wire = SelectWire(channel_, type_support_, local_type);
      *proto_wire = wire->converter == nullptr;
      return true;
    }
    return false;
  }

  /// Creates the ROS 2 subscription once the wire format is known.
  bool TrySubscribe() {
    if (subscription_ != nullptr) {
      return true;
    }
    Wire wire;
    bool proto_wire = false;
    if (!ResolveWire(&wire, &proto_wire)) {
      return false;
    }
    // Publications of this process are not ignored wholesale: only those of
    // Cyber writers are dropped (see LocalPublishers), so plain ROS 2 nodes
    // in the same process can talk to Apollo components.
    rclcpp::SubscriptionOptions options;
    options.callback_group = group_;
    options.use_intra_process_comm = rclcpp::IntraProcessSetting::Disable;
    const std::string topic = topic_;
    options.event_callbacks.incompatible_qos_callback =
        [topic](rclcpp::QOSRequestedIncompatibleQoSInfo& info) {
          AWARN << "[" << topic
                << "] a writer offers an incompatible QoS (policy id "
                << info.last_policy_kind << ")";
        };
    auto active = active_;
    const TypeSupport* ts = type_support_;
    const Callback callback = callback_;
    const std::string channel = channel_;
    const rclcpp::QoS qos = MakeQos(qos_profile_, depth_);
    auto ros_node = node_->ros_node();

    if (!proto_wire) {
      const auto converter = wire.converter;
      const auto* descriptor = wire.descriptor;
      subscription_ = ros_node->create_generic_subscription(
          topic_, converter->ros_type(), qos,
          [active, ts, callback, converter, descriptor, topic](
              std::shared_ptr<const rclcpp::SerializedMessage> m,
              const rclcpp::MessageInfo& info) {
            if (!active->load() || FromLocalWriter(info)) {
              return;
            }
            std::shared_ptr<void> msg;
            if (!ts->is_raw) {
              google::protobuf::Message* proto = nullptr;
              msg = ts->create(&proto);
              if (proto == nullptr || !converter->FromSerialized(*m, proto)) {
                msg = nullptr;
              }
            } else {
              auto proto = NativeTypes::Instance()->NewMessage(descriptor);
              std::string bytes;
              if (proto != nullptr && converter->FromSerialized(*m, proto.get()) &&
                  proto->SerializePartialToString(&bytes)) {
                msg = ts->parse(reinterpret_cast<const uint8_t*>(bytes.data()),
                                bytes.size());
              }
            }
            if (!msg) {
              AWARN << "[" << topic << "] failed to convert "
                    << converter->ros_type() << " to " << ts->type_name;
              return;
            }
            callback(msg);
          },
          options);
      intra_->SetRosType(converter->ros_type());
      return true;
    }

    auto warned = std::make_shared<std::atomic<bool>>(false);
    auto type_learned = std::make_shared<std::atomic<bool>>(false);
    subscription_ = ros_node->create_subscription<ProtoMessage>(
        topic_, qos,
        [active, warned, type_learned, ts, callback, topic, channel](
            std::shared_ptr<const ProtoMessage> m,
            const rclcpp::MessageInfo& info) {
          if (!active->load() || FromLocalWriter(info)) {
            return;
          }
          // Raw readers (recorder, tools) learn the channel's protobuf type
          // from the first sample; the ROS 2 graph only knows ProtoMessage.
          if (ts->is_raw && !m->type.empty() && !type_learned->exchange(true)) {
            RegisterLocalChannelType(channel, m->type);
          }
          if (!ts->is_raw && !m->type.empty() && m->type != ts->type_name &&
              m->type != RawTypeName()) {
            if (!warned->exchange(true)) {
              AWARN << "[" << topic << "] dropping message of type " << m->type
                    << ", reader expects " << ts->type_name;
            }
            return;
          }
          auto msg = ts->parse(m->data.data(), m->data.size());
          if (!msg) {
            AWARN << "[" << topic << "] failed to parse " << ts->type_name;
            return;
          }
          callback(msg);
        },
        options);
    intra_->SetRosType(kProtoMessageType);
    return true;
  }

  std::shared_ptr<NodeHandle> node_;
  std::string channel_;
  std::string topic_;
  const TypeSupport* type_support_;
  Callback callback_;
  proto::QosProfile qos_profile_;
  size_t depth_ = 1;
  std::shared_ptr<std::atomic<bool>> active_;
  rclcpp::CallbackGroup::SharedPtr group_;
  std::shared_ptr<IntraSubscription> intra_;
  std::shared_ptr<std::mutex> mutex_ = std::make_shared<std::mutex>();
  rclcpp::TimerBase::SharedPtr resolve_timer_;
  rclcpp::SubscriptionBase::SharedPtr subscription_;
};

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

/// The native ROS 2 service type of an Apollo service, when there is one.
struct ServiceWire {
  const NativeServiceInfo* info = nullptr;
  std::shared_ptr<const MessageConverter> request;
  std::shared_ptr<const MessageConverter> response;
};

ServiceWire SelectServiceWire(const std::string& service_name,
                              const std::string& request_type,
                              const std::string& response_type) {
  ServiceWire wire;
  auto* types = NativeTypes::Instance();
  if (types->UseProtoWire(service_name)) {
    return wire;
  }
  const auto* info = types->FindService(request_type, response_type);
  if (info != nullptr &&
      MessageConverter::GetForService(*info, &wire.request, &wire.response)) {
    wire.info = info;
  }
  return wire;
}

bool ProtoFromBytes(const std::vector<uint8_t>& bytes,
                    google::protobuf::Message* out) {
  return out->ParsePartialFromArray(bytes.data(),
                                    static_cast<int>(bytes.size()));
}

bool ProtoToBytes(const google::protobuf::Message& in,
                  std::vector<uint8_t>* bytes) {
  bytes->resize(in.ByteSizeLong());
  return bytes->empty() ||
         in.SerializePartialToArray(bytes->data(),
                                    static_cast<int>(bytes->size()));
}

/**
 * A ROS 2 service of a type that is only known at run time (rclcpp in Jazzy
 * has a generic client but no generic service). Requests are deserialized by
 * the RMW layer into a ROS 2 message created through type introspection and
 * converted to protobuf for the Cyber service callback.
 */
class NativeService : public rclcpp::ServiceBase {
 public:
  NativeService(const rclcpp::Node::SharedPtr& node, const std::string& name,
                const ServiceWire& wire, ServiceEndpoint::Handler handler,
                std::shared_ptr<std::atomic<bool>> active)
      : rclcpp::ServiceBase(
            node->get_node_base_interface()->get_shared_rcl_node_handle()),
        wire_(wire),
        handler_(std::move(handler)),
        active_(std::move(active)),
        name_(name) {
    library_ = rclcpp::get_typesupport_library(wire.info->ros_type,
                                               "rosidl_typesupport_cpp");
    const rosidl_service_type_support_t* type_support =
        rclcpp::get_service_typesupport_handle(
            wire.info->ros_type, "rosidl_typesupport_cpp", *library_);
    service_handle_ = std::shared_ptr<rcl_service_t>(
        new rcl_service_t, [handle = node_handle_](rcl_service_t* service) {
          if (rcl_service_fini(service, handle.get()) != RCL_RET_OK) {
            rcl_reset_error();
          }
          delete service;
        });
    *service_handle_ = rcl_get_zero_initialized_service();
    rcl_service_options_t options = rcl_service_get_default_options();
    options.qos = rclcpp::ServicesQoS().get_rmw_qos_profile();
    if (rcl_service_init(service_handle_.get(), node_handle_.get(),
                         type_support, name.c_str(), &options) != RCL_RET_OK) {
      const std::string error = rcl_get_error_string().str;
      rcl_reset_error();
      throw std::runtime_error("rcl_service_init failed: " + error);
    }
  }

  std::shared_ptr<void> create_request() override {
    return wire_.request->CreateRos();
  }

  std::shared_ptr<rmw_request_id_t> create_request_header() override {
    return std::make_shared<rmw_request_id_t>();
  }

  void handle_request(std::shared_ptr<rmw_request_id_t> request_header,
                      std::shared_ptr<void> request) override {
    auto response = wire_.response->CreateRos();
    if (active_->load()) {
      auto* types = NativeTypes::Instance();
      auto request_proto = types->NewMessage(wire_.request->descriptor());
      auto response_proto = types->NewMessage(wire_.response->descriptor());
      std::vector<uint8_t> request_bytes;
      std::vector<uint8_t> response_bytes;
      if (request_proto != nullptr && response_proto != nullptr &&
          wire_.request->FromRos(request.get(), request_proto.get()) &&
          ProtoToBytes(*request_proto, &request_bytes) &&
          handler_(request_bytes.data(), request_bytes.size(),
                   &response_bytes) &&
          ProtoFromBytes(response_bytes, response_proto.get())) {
        wire_.response->ToRos(*response_proto, response.get());
      } else {
        AWARN << "[" << name_ << "] request could not be handled";
      }
    }
    if (rcl_send_response(service_handle_.get(), request_header.get(),
                          response.get()) != RCL_RET_OK) {
      AWARN << "[" << name_ << "] failed to send the response: "
            << rcl_get_error_string().str;
      rcl_reset_error();
    }
  }

 private:
  ServiceWire wire_;
  ServiceEndpoint::Handler handler_;
  std::shared_ptr<std::atomic<bool>> active_;
  std::string name_;
  std::shared_ptr<rcpputils::SharedLibrary> library_;
};

class ServiceImpl : public ServiceEndpoint {
 public:
  ServiceImpl(const std::shared_ptr<NodeHandle>& node,
              const std::string& service_name, const std::string& request_type,
              const std::string& response_type, Handler handler)
      : node_(node), active_(std::make_shared<std::atomic<bool>>(true)) {
    auto ros_node = node->ros_node();
    group_ = ros_node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive, true);
    auto active = active_;
    std::string name = ChannelToTopic(service_name);
    const ServiceWire wire =
        SelectServiceWire(service_name, request_type, response_type);
    if (wire.info != nullptr) {
      native_service_ = std::make_shared<NativeService>(ros_node, name, wire,
                                                        handler, active);
      ros_node->get_node_services_interface()->add_service(native_service_,
                                                           group_);
      return;
    }
    service_ = ros_node->create_service<ProtoService>(
        name,
        [active, handler, request_type, response_type, name](
            const std::shared_ptr<ProtoService::Request> request,
            std::shared_ptr<ProtoService::Response> response) {
          response->type = response_type;
          if (!active->load()) {
            return;
          }
          if (!request->type.empty() && request->type != request_type) {
            AWARN << "[" << name << "] request type " << request->type
                  << " does not match " << request_type;
          }
          if (!handler(request->data.data(), request->data.size(),
                       &response->data)) {
            response->data.clear();
          }
        },
        rclcpp::ServicesQoS(), group_);
  }

  ~ServiceImpl() override { Shutdown(); }

  void Shutdown() override {
    if (!active_->exchange(false)) {
      return;
    }
    service_.reset();
    native_service_.reset();
  }

 private:
  std::shared_ptr<NodeHandle> node_;
  std::shared_ptr<std::atomic<bool>> active_;
  rclcpp::CallbackGroup::SharedPtr group_;
  rclcpp::Service<ProtoService>::SharedPtr service_;
  std::shared_ptr<NativeService> native_service_;
};

class ClientImpl : public ClientEndpoint,
                   public std::enable_shared_from_this<ClientImpl> {
 public:
  ClientImpl(const std::shared_ptr<NodeHandle>& node,
             const std::string& service_name, const std::string& request_type,
             const std::string& response_type)
      : request_type_(request_type) {
    // Clients live on the aux node, which the background executor always
    // spins, so a blocking Call() works from any thread including callbacks
    // of a single-threaded component container.
    (void)node;
    aux_ = Runtime::Instance()->AuxNode();
    auto ros_node = aux_->ros_node();
    group_ = ros_node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive, true);
    const std::string name = ChannelToTopic(service_name);
    wire_ = SelectServiceWire(service_name, request_type, response_type);
    if (wire_.info != nullptr) {
      native_client_ = ros_node->create_generic_client(
          name, wire_.info->ros_type, rclcpp::ServicesQoS(), group_);
      client_base_ = native_client_;
    } else {
      client_ = ros_node->create_client<ProtoService>(
          name, rclcpp::ServicesQoS(), group_);
      client_base_ = client_;
    }
  }

  bool Call(std::vector<uint8_t>&& request, std::vector<uint8_t>* response,
            std::chrono::nanoseconds timeout) override {
    if (native_client_ != nullptr) {
      return CallNative(request, response, timeout);
    }
    auto req = std::make_shared<ProtoService::Request>();
    req->type = request_type_;
    req->data = std::move(request);
    auto future = client_->async_send_request(req);
    if (!WaitFor(future, timeout)) {
      client_->remove_pending_request(future);
      return false;
    }
    auto resp = future.get();
    if (!resp) {
      return false;
    }
    if (response != nullptr) {
      *response = std::move(resp->data);
    }
    return true;
  }

  void AsyncCall(std::vector<uint8_t>&& request,
                 ResponseCallback callback) override {
    if (native_client_ != nullptr) {
      // GenericClient has no callback interface: wait on the task pool.
      auto self = shared_from_this();
      auto req = std::make_shared<std::vector<uint8_t>>(std::move(request));
      TaskManager::Instance()->Enqueue([self, req, callback]() {
        std::vector<uint8_t> response;
        const bool ok =
            self->CallNative(*req, &response, std::chrono::seconds(30));
        callback(ok, response);
      });
      return;
    }
    auto req = std::make_shared<ProtoService::Request>();
    req->type = request_type_;
    req->data = std::move(request);
    client_->async_send_request(
        req, [callback](rclcpp::Client<ProtoService>::SharedFuture future) {
          auto resp = future.get();
          if (resp) {
            callback(true, resp->data);
          } else {
            callback(false, {});
          }
        });
  }

  bool ServiceIsReady() const override {
    return client_base_->service_is_ready();
  }

  bool WaitForService(std::chrono::nanoseconds timeout) override {
    if (timeout.count() < 0) {
      while (rclcpp::ok()) {
        if (client_base_->wait_for_service(std::chrono::milliseconds(200))) {
          return true;
        }
      }
      return false;
    }
    return client_base_->wait_for_service(timeout);
  }

 private:
  /// Waits in slices so a blocking call returns promptly on shutdown
  /// instead of sitting out its whole timeout.
  template <typename FutureT>
  static bool WaitFor(FutureT& future, std::chrono::nanoseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (rclcpp::ok()) {
      auto now = std::chrono::steady_clock::now();
      if (now >= deadline) {
        break;
      }
      auto slice = std::min<std::chrono::nanoseconds>(
          deadline - now, std::chrono::milliseconds(50));
      if (future.wait_for(slice) == std::future_status::ready) {
        return true;
      }
    }
    return false;
  }

  bool CallNative(const std::vector<uint8_t>& request,
                  std::vector<uint8_t>* response,
                  std::chrono::nanoseconds timeout) {
    auto* types = NativeTypes::Instance();
    auto request_proto = types->NewMessage(wire_.request->descriptor());
    if (request_proto == nullptr || !ProtoFromBytes(request, request_proto.get())) {
      return false;
    }
    auto ros_request = wire_.request->CreateRos();
    wire_.request->ToRos(*request_proto, ros_request.get());
    auto future = native_client_->async_send_request(ros_request.get());
    if (!WaitFor(future, timeout)) {
      native_client_->remove_pending_request(future.request_id);
      return false;
    }
    std::shared_ptr<void> ros_response = future.get();
    auto response_proto = types->NewMessage(wire_.response->descriptor());
    if (!ros_response || response_proto == nullptr ||
        !wire_.response->FromRos(ros_response.get(), response_proto.get())) {
      return false;
    }
    return response == nullptr || ProtoToBytes(*response_proto, response);
  }

  std::string request_type_;
  ServiceWire wire_;
  std::shared_ptr<NodeHandle> aux_;
  rclcpp::CallbackGroup::SharedPtr group_;
  rclcpp::ClientBase::SharedPtr client_base_;
  rclcpp::Client<ProtoService>::SharedPtr client_;
  rclcpp::GenericClient::SharedPtr native_client_;
};

// ---------------------------------------------------------------------------
// Timers
// ---------------------------------------------------------------------------

class TimerImpl : public TimerEndpoint {
 public:
  TimerImpl(const std::shared_ptr<NodeHandle>& node,
            std::chrono::nanoseconds period, std::function<void()> callback,
            bool oneshot) {
    node_ = node ? node : Runtime::Instance()->AuxNode();
    auto ros_node = node_->ros_node();
    group_ = ros_node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive, true);
    auto state = std::make_shared<State>();
    state->callback = std::move(callback);
    state->oneshot = oneshot;
    state_ = state;
    timer_ = ros_node->create_wall_timer(
        period,
        [state]() {
          if (!state->active.load()) {
            return;
          }
          if (state->oneshot) {
            state->active.store(false);
            if (auto t = state->timer.lock()) {
              t->cancel();
            }
          }
          state->callback();
        },
        group_);
    state->timer = timer_;
  }

  ~TimerImpl() override { Cancel(); }

  void Cancel() override {
    if (state_) {
      state_->active.store(false);
    }
    if (timer_) {
      timer_->cancel();
      timer_.reset();
    }
  }

 private:
  struct State {
    std::function<void()> callback;
    bool oneshot = false;
    std::atomic<bool> active{true};
    std::weak_ptr<rclcpp::TimerBase> timer;
  };
  std::shared_ptr<NodeHandle> node_;
  rclcpp::CallbackGroup::SharedPtr group_;
  std::shared_ptr<State> state_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------

std::shared_ptr<NodeHandle> NodeHandle::Create(const std::string& name,
                                               const std::string& name_space) {
  auto runtime = Runtime::Instance();
  if (!runtime->IsInitialized()) {
    runtime->Init();
  }
  std::string ns = name_space.empty() ? "" : ChannelToTopic(name_space);
  auto node = std::make_shared<rclcpp::Node>(ToNodeName(name), ns,
                                             runtime->DefaultNodeOptions());
  std::shared_ptr<NodeHandle> handle(new NodeHandle());
  handle->node_ = node;
  handle->name_ = name;
  handle->owned_ = true;
  runtime->AddNode(node);
  return handle;
}

std::shared_ptr<NodeHandle> NodeHandle::Wrap(
    std::shared_ptr<rclcpp::Node> node) {
  std::shared_ptr<NodeHandle> handle(new NodeHandle());
  handle->name_ = node->get_name();
  handle->node_ = std::move(node);
  handle->owned_ = false;
  return handle;
}

NodeHandle::~NodeHandle() {
  if (owned_ && node_) {
    Runtime::Instance()->RemoveNode(node_);
  }
}

std::shared_ptr<PublisherEndpoint> PublisherEndpoint::Create(
    const std::shared_ptr<NodeHandle>& node,
    const proto::RoleAttributes& role_attr, const TypeSupport* type_support) {
  try {
    return std::make_shared<PublisherImpl>(node, role_attr, type_support);
  } catch (const std::exception& e) {
    AERROR << "create writer on [" << role_attr.channel_name()
           << "] failed: " << e.what();
    return nullptr;
  }
}

std::shared_ptr<SubscriberEndpoint> SubscriberEndpoint::Create(
    const std::shared_ptr<NodeHandle>& node,
    const proto::RoleAttributes& role_attr, const TypeSupport* type_support,
    uint32_t pending_queue_size, Callback callback) {
  try {
    return std::make_shared<SubscriberImpl>(node, role_attr, type_support,
                                            pending_queue_size,
                                            std::move(callback));
  } catch (const std::exception& e) {
    AERROR << "create reader on [" << role_attr.channel_name()
           << "] failed: " << e.what();
    return nullptr;
  }
}

std::shared_ptr<ServiceEndpoint> ServiceEndpoint::Create(
    const std::shared_ptr<NodeHandle>& node, const std::string& service_name,
    const std::string& request_type, const std::string& response_type,
    Handler handler) {
  try {
    return std::make_shared<ServiceImpl>(node, service_name, request_type,
                                         response_type, std::move(handler));
  } catch (const std::exception& e) {
    AERROR << "create service [" << service_name << "] failed: " << e.what();
    return nullptr;
  }
}

std::shared_ptr<ClientEndpoint> ClientEndpoint::Create(
    const std::shared_ptr<NodeHandle>& node, const std::string& service_name,
    const std::string& request_type, const std::string& response_type) {
  try {
    return std::make_shared<ClientImpl>(node, service_name, request_type,
                                        response_type);
  } catch (const std::exception& e) {
    AERROR << "create client [" << service_name << "] failed: " << e.what();
    return nullptr;
  }
}

std::shared_ptr<TimerEndpoint> TimerEndpoint::Create(
    const std::shared_ptr<NodeHandle>& node, std::chrono::nanoseconds period,
    std::function<void()> callback, bool oneshot) {
  try {
    return std::make_shared<TimerImpl>(node, period, std::move(callback),
                                       oneshot);
  } catch (const std::exception& e) {
    AERROR << "create timer failed: " << e.what();
    return nullptr;
  }
}

namespace {
std::mutex& ChannelTypeMutex() {
  static std::mutex* m = new std::mutex();
  return *m;
}
std::unordered_map<std::string, std::string>& ChannelTypes() {
  static auto* types = new std::unordered_map<std::string, std::string>();
  return *types;
}
}  // namespace

void RegisterLocalChannelType(const std::string& channel,
                              const std::string& type_name) {
  if (type_name.empty() || type_name == RawTypeName()) {
    return;
  }
  std::lock_guard<std::mutex> lock(ChannelTypeMutex());
  ChannelTypes()[ChannelToTopic(channel)] = type_name;
}

bool LookupLocalChannelType(const std::string& channel,
                            std::string* type_name) {
  std::lock_guard<std::mutex> lock(ChannelTypeMutex());
  auto it = ChannelTypes().find(ChannelToTopic(channel));
  if (it == ChannelTypes().end()) {
    return false;
  }
  if (type_name != nullptr) {
    *type_name = it->second;
  }
  return true;
}

std::string ChannelProtoType(const std::string& channel) {
  const std::string topic = ChannelToTopic(channel);
  try {
    const auto topics =
        Runtime::Instance()->AuxNode()->ros_node()->get_topic_names_and_types();
    const auto it = topics.find(topic);
    if (it != topics.end()) {
      for (const auto& ros_type : it->second) {
        if (const auto* info = NativeTypes::Instance()->FindByRosType(ros_type)) {
          return info->proto_name;
        }
      }
    }
  } catch (const std::exception& e) {
    ADEBUG << "graph query failed: " << e.what();
  }
  std::string type;
  LookupLocalChannelType(channel, &type);
  return type;
}

bool IsApolloTopicType(const std::string& ros_type) {
  return ros_type == kProtoMessageType ||
         NativeTypes::Instance()->FindByRosType(ros_type) != nullptr;
}

bool IsApolloServiceType(const std::string& ros_type) {
  return ros_type == kProtoServiceType ||
         NativeTypes::Instance()->FindServiceByRosType(ros_type) != nullptr;
}

std::shared_ptr<NodeHandle> AuxNodeHandle() {
  return Runtime::Instance()->AuxNode();
}

size_t LocalSubscriberCount(const std::string& topic) {
  return IntraProcessManager::Instance()->Get(ChannelToTopic(topic)).size();
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
