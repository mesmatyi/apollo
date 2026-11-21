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
#include <atomic>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "apollo_msgs/msg/proto_message.hpp"
#include "apollo_msgs/srv/proto_service.hpp"
#include "rclcpp/rclcpp.hpp"

#include "cyber/common/log.h"
#include "cyber/ros2/naming.h"
#include "cyber/ros2/runtime.h"

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
  attr.set_message_type(info.topic_type());
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
};

class IntraProcessManager {
 public:
  static IntraProcessManager* Instance() {
    static IntraProcessManager* instance = new IntraProcessManager();
    return instance;
  }

  void Add(const std::string& topic,
           const std::shared_ptr<IntraSubscription>& sub) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_[topic].push_back(sub);
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

  std::vector<std::shared_ptr<IntraSubscription>> Get(
      const std::string& topic) {
    std::vector<std::shared_ptr<IntraSubscription>> result;
    std::lock_guard<std::mutex> lock(mutex_);
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

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::vector<std::weak_ptr<IntraSubscription>>>
      subs_;
};

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
    publisher_ = node->ros_node()->create_publisher<ProtoMessage>(
        topic_, MakeQos(p, WriterDepth(p)), options);
  }

  ~PublisherImpl() override { Shutdown(); }

  const std::string& topic() const override { return topic_; }

  bool Publish(const std::shared_ptr<const void>& msg) override {
    if (!msg || shutdown_.load()) {
      return false;
    }
    std::vector<uint8_t> bytes;
    bool serialized = false;
    auto locals = IntraProcessManager::Instance()->Get(topic_);
    for (auto& sub : locals) {
      sub->Deliver(msg, type_support_, &bytes, &serialized);
    }
    // Every in-process reader also owns a ROS subscription that ignores
    // local publications; only serialize when somebody else listens.
    size_t matched = publisher_->get_subscription_count();
    if (!transient_local_ && matched <= locals.size()) {
      return true;
    }
    auto ros_msg = std::make_unique<ProtoMessage>();
    ros_msg->type = type_name_;
    if (serialized) {
      ros_msg->data = std::move(bytes);
    } else if (!type_support_->serialize(msg.get(), &ros_msg->data)) {
      AERROR << "[" << topic_ << "] failed to serialize " << type_name_;
      return false;
    }
    publisher_->publish(std::move(ros_msg));
    return true;
  }

  size_t PeerCount() const override {
    return publisher_ ? node_->ros_node()->count_subscribers(topic_) : 0;
  }

  void GetPeers(std::vector<proto::RoleAttributes>* peers) const override {
    if (peers == nullptr || !publisher_) {
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
    publisher_.reset();
  }

 private:
  std::shared_ptr<NodeHandle> node_;
  std::string channel_;
  std::string topic_;
  std::string type_name_;
  const TypeSupport* type_support_;
  bool transient_local_ = false;
  std::atomic<bool> shutdown_{false};
  rclcpp::Publisher<ProtoMessage>::SharedPtr publisher_;
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
        active_(std::make_shared<std::atomic<bool>>(true)) {
    RegisterLocalChannelType(channel_, type_support->type_name);
    const auto& p = role_attr.qos_profile();
    size_t depth = std::max<uint32_t>(pending_queue_size, 1);
    if (p.durability() ==
        proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL) {
      depth = std::max<size_t>(depth, p.depth());
    }
    auto ros_node = node->ros_node();
    group_ = ros_node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive, true);

    intra_ = std::make_shared<IntraSubscription>(
        ros_node->get_node_base_interface()->get_context(), type_support,
        depth, callback);
    ros_node->get_node_waitables_interface()->add_waitable(intra_, group_);
    IntraProcessManager::Instance()->Add(topic_, intra_);

    rclcpp::SubscriptionOptions options;
    options.callback_group = group_;
    options.ignore_local_publications = true;
    options.use_intra_process_comm = rclcpp::IntraProcessSetting::Disable;
    const std::string topic = topic_;
    options.event_callbacks.incompatible_qos_callback =
        [topic](rclcpp::QOSRequestedIncompatibleQoSInfo& info) {
          AWARN << "[" << topic
                << "] a writer offers an incompatible QoS (policy id "
                << info.last_policy_kind << ")";
        };
    auto active = active_;
    auto warned = std::make_shared<std::atomic<bool>>(false);
    auto type_learned = std::make_shared<std::atomic<bool>>(false);
    const TypeSupport* ts = type_support;
    const std::string channel = channel_;
    subscription_ = ros_node->create_subscription<ProtoMessage>(
        topic_, MakeQos(p, depth),
        [active, warned, type_learned, ts, callback, topic,
         channel](std::shared_ptr<const ProtoMessage> m) {
          if (!active->load()) {
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
  }

  ~SubscriberImpl() override { Shutdown(); }

  const std::string& topic() const override { return topic_; }

  size_t PeerCount() const override {
    return subscription_ ? node_->ros_node()->count_publishers(topic_) : 0;
  }

  void GetPeers(std::vector<proto::RoleAttributes>* peers) const override {
    if (peers == nullptr || !subscription_) {
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
    subscription_.reset();
  }

 private:
  std::shared_ptr<NodeHandle> node_;
  std::string channel_;
  std::string topic_;
  const TypeSupport* type_support_;
  std::shared_ptr<std::atomic<bool>> active_;
  rclcpp::CallbackGroup::SharedPtr group_;
  std::shared_ptr<IntraSubscription> intra_;
  rclcpp::Subscription<ProtoMessage>::SharedPtr subscription_;
};

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

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
  }

 private:
  std::shared_ptr<NodeHandle> node_;
  std::shared_ptr<std::atomic<bool>> active_;
  rclcpp::CallbackGroup::SharedPtr group_;
  rclcpp::Service<ProtoService>::SharedPtr service_;
};

class ClientImpl : public ClientEndpoint {
 public:
  ClientImpl(const std::shared_ptr<NodeHandle>& node,
             const std::string& service_name, const std::string& request_type)
      : request_type_(request_type) {
    // Clients live on the aux node, which the background executor always
    // spins, so a blocking Call() works from any thread including callbacks
    // of a single-threaded component container.
    (void)node;
    aux_ = Runtime::Instance()->AuxNode();
    auto ros_node = aux_->ros_node();
    group_ = ros_node->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive, true);
    client_ = ros_node->create_client<ProtoService>(
        ChannelToTopic(service_name), rclcpp::ServicesQoS(), group_);
  }

  bool Call(std::vector<uint8_t>&& request, std::vector<uint8_t>* response,
            std::chrono::nanoseconds timeout) override {
    auto req = std::make_shared<ProtoService::Request>();
    req->type = request_type_;
    req->data = std::move(request);
    auto future = client_->async_send_request(req);
    // Wait in slices so a blocking call returns promptly on shutdown instead
    // of sitting out its whole timeout.
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool ready = false;
    while (rclcpp::ok()) {
      auto now = std::chrono::steady_clock::now();
      if (now >= deadline) {
        break;
      }
      auto slice = std::min<std::chrono::nanoseconds>(
          deadline - now, std::chrono::milliseconds(50));
      if (future.wait_for(slice) == std::future_status::ready) {
        ready = true;
        break;
      }
    }
    if (!ready) {
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

  bool ServiceIsReady() const override { return client_->service_is_ready(); }

  bool WaitForService(std::chrono::nanoseconds timeout) override {
    if (timeout.count() < 0) {
      while (rclcpp::ok()) {
        if (client_->wait_for_service(std::chrono::milliseconds(200))) {
          return true;
        }
      }
      return false;
    }
    return client_->wait_for_service(timeout);
  }

 private:
  std::string request_type_;
  std::shared_ptr<NodeHandle> aux_;
  rclcpp::CallbackGroup::SharedPtr group_;
  rclcpp::Client<ProtoService>::SharedPtr client_;
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
    const std::string& request_type) {
  try {
    return std::make_shared<ClientImpl>(node, service_name, request_type);
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

std::shared_ptr<NodeHandle> AuxNodeHandle() {
  return Runtime::Instance()->AuxNode();
}

size_t LocalSubscriberCount(const std::string& topic) {
  return IntraProcessManager::Instance()->Get(ChannelToTopic(topic)).size();
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
