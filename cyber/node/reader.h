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

#ifndef CYBER_NODE_READER_H_
#define CYBER_NODE_READER_H_

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cyber/proto/topology_change.pb.h"

#include "cyber/common/log.h"
#include "cyber/node/reader_base.h"
#include "cyber/ros2/transport.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {

template <typename M0>
using CallbackFunc = std::function<void(const std::shared_ptr<M0>&)>;

using proto::RoleType;

const uint32_t DEFAULT_PENDING_QUEUE_SIZE = 1;

/**
 * @class Reader
 * @brief Subscribes to a channel (a ROS 2 topic of type
 * apollo_msgs/msg/ProtoMessage).
 *
 * Every received message is stored in a history (capacity = qos depth) that
 * Observe()/GetLatestObserved() expose, and is passed to the optional
 * callback. Callbacks of one Reader never run concurrently.
 */
template <typename MessageT>
class Reader : public ReaderBase {
 public:
  using Iterator =
      typename std::list<std::shared_ptr<MessageT>>::const_iterator;

  explicit Reader(const proto::RoleAttributes& role_attr,
                  const CallbackFunc<MessageT>& reader_func = nullptr,
                  uint32_t pending_queue_size = DEFAULT_PENDING_QUEUE_SIZE);
  virtual ~Reader();

  bool Init() override;
  void Shutdown() override;
  void Observe() override;
  void ClearData() override;
  bool HasReceived() const override;
  bool Empty() const override;
  double GetDelaySec() const override;
  uint32_t PendingQueueSize() const override;

  virtual void Enqueue(const std::shared_ptr<MessageT>& msg);
  virtual void SetHistoryDepth(const uint32_t& depth);
  virtual uint32_t GetHistoryDepth() const;
  virtual std::shared_ptr<MessageT> GetLatestObserved() const;
  virtual std::shared_ptr<MessageT> GetOldestObserved() const;
  virtual Iterator Begin() const { return observed_queue_.begin(); }
  virtual Iterator End() const { return observed_queue_.end(); }

  bool HasWriter() override;
  void GetWriters(std::vector<proto::RoleAttributes>* writers) override;

  /// Latest message received (independent of Observe()); used by
  /// multi-channel Components to fuse channels.
  std::shared_ptr<MessageT> GetLatestReceived() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return published_queue_.empty() ? nullptr : published_queue_.front();
  }

 protected:
  double latest_recv_time_sec_ = -1.0;
  double second_to_lastest_recv_time_sec_ = -1.0;
  uint32_t pending_queue_size_;

 private:
  void OnMessage(const std::shared_ptr<void>& msg);

  CallbackFunc<MessageT> reader_func_;
  std::shared_ptr<ros2::SubscriberEndpoint> endpoint_;
  mutable std::mutex mutex_;
  uint32_t history_depth_ = 1;
  // Newest message first, like Cyber's Blocker.
  std::list<std::shared_ptr<MessageT>> published_queue_;
  std::list<std::shared_ptr<MessageT>> observed_queue_;
};

template <typename MessageT>
Reader<MessageT>::Reader(const proto::RoleAttributes& role_attr,
                         const CallbackFunc<MessageT>& reader_func,
                         uint32_t pending_queue_size)
    : ReaderBase(role_attr),
      pending_queue_size_(pending_queue_size),
      reader_func_(reader_func) {
  history_depth_ = std::max<uint32_t>(role_attr_.qos_profile().depth(), 1);
}

template <typename MessageT>
Reader<MessageT>::~Reader() {
  Shutdown();
}

template <typename MessageT>
void Reader<MessageT>::Enqueue(const std::shared_ptr<MessageT>& msg) {
  std::lock_guard<std::mutex> lock(mutex_);
  second_to_lastest_recv_time_sec_ = latest_recv_time_sec_;
  latest_recv_time_sec_ = Time::Now().ToSecond();
  published_queue_.push_front(msg);
  while (published_queue_.size() > history_depth_) {
    published_queue_.pop_back();
  }
}

template <typename MessageT>
void Reader<MessageT>::OnMessage(const std::shared_ptr<void>& msg) {
  auto typed = std::static_pointer_cast<MessageT>(msg);
  Enqueue(typed);
  if (reader_func_) {
    reader_func_(typed);
  }
}

template <typename MessageT>
void Reader<MessageT>::Observe() {
  std::lock_guard<std::mutex> lock(mutex_);
  observed_queue_ = published_queue_;
}

template <typename MessageT>
bool Reader<MessageT>::Init() {
  if (init_.exchange(true)) {
    return true;
  }
  if (!node_handle_) {
    node_handle_ = ros2::AuxNodeHandle();
  }
  endpoint_ = ros2::SubscriberEndpoint::Create(
      node_handle_, role_attr_, ros2::GetTypeSupport<MessageT>(),
      pending_queue_size_,
      [this](const std::shared_ptr<void>& msg) { this->OnMessage(msg); });
  if (endpoint_ == nullptr) {
    init_.store(false);
    return false;
  }
  return true;
}

template <typename MessageT>
void Reader<MessageT>::Shutdown() {
  if (!init_.exchange(false)) {
    return;
  }
  if (endpoint_) {
    endpoint_->Shutdown();
  }
  endpoint_.reset();
}

template <typename MessageT>
bool Reader<MessageT>::HasReceived() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !published_queue_.empty();
}

template <typename MessageT>
bool Reader<MessageT>::Empty() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return observed_queue_.empty();
}

template <typename MessageT>
double Reader<MessageT>::GetDelaySec() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (latest_recv_time_sec_ < 0) {
    return -1.0;
  }
  if (second_to_lastest_recv_time_sec_ < 0) {
    return Time::Now().ToSecond() - latest_recv_time_sec_;
  }
  return std::max((Time::Now().ToSecond() - latest_recv_time_sec_),
                  (latest_recv_time_sec_ - second_to_lastest_recv_time_sec_));
}

template <typename MessageT>
uint32_t Reader<MessageT>::PendingQueueSize() const {
  return pending_queue_size_;
}

template <typename MessageT>
std::shared_ptr<MessageT> Reader<MessageT>::GetLatestObserved() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (observed_queue_.empty()) {
    return nullptr;
  }
  return observed_queue_.front();
}

template <typename MessageT>
std::shared_ptr<MessageT> Reader<MessageT>::GetOldestObserved() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (observed_queue_.empty()) {
    return nullptr;
  }
  return observed_queue_.back();
}

template <typename MessageT>
void Reader<MessageT>::ClearData() {
  std::lock_guard<std::mutex> lock(mutex_);
  published_queue_.clear();
  observed_queue_.clear();
}

template <typename MessageT>
void Reader<MessageT>::SetHistoryDepth(const uint32_t& depth) {
  std::lock_guard<std::mutex> lock(mutex_);
  history_depth_ = std::max<uint32_t>(depth, 1);
  while (published_queue_.size() > history_depth_) {
    published_queue_.pop_back();
  }
  while (observed_queue_.size() > history_depth_) {
    observed_queue_.pop_back();
  }
}

template <typename MessageT>
uint32_t Reader<MessageT>::GetHistoryDepth() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return history_depth_;
}

template <typename MessageT>
bool Reader<MessageT>::HasWriter() {
  if (!init_.load() || !endpoint_) {
    return false;
  }
  return endpoint_->PeerCount() > 0;
}

template <typename MessageT>
void Reader<MessageT>::GetWriters(std::vector<proto::RoleAttributes>* writers) {
  if (writers == nullptr || !init_.load() || !endpoint_) {
    return;
  }
  endpoint_->GetPeers(writers);
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_NODE_READER_H_
