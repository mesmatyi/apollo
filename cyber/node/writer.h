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

#ifndef CYBER_NODE_WRITER_H_
#define CYBER_NODE_WRITER_H_

#include <memory>
#include <string>
#include <vector>

#include "cyber/common/log.h"
#include "cyber/node/writer_base.h"
#include "cyber/ros2/transport.h"

namespace apollo {
namespace cyber {

/**
 * @class Writer
 * @brief Publishes messages on a channel (a ROS 2 topic of type
 * apollo_msgs/msg/ProtoMessage).
 *
 * Readers in the same process receive the written shared_ptr itself (no
 * copy, no serialization), exactly like Cyber's intra-process transport, so
 * a message must not be modified after it was written.
 */
template <typename MessageT>
class Writer : public WriterBase {
 public:
  explicit Writer(const proto::RoleAttributes& role_attr);
  virtual ~Writer();

  bool Init() override;
  void Shutdown() override;

  virtual bool Write(const MessageT& msg);
  virtual bool Write(const std::shared_ptr<MessageT>& msg_ptr);

  bool HasReader() override;
  void GetReaders(std::vector<proto::RoleAttributes>* readers) override;

  /// Returns a fresh message to fill and write.
  std::shared_ptr<MessageT> AcquireMessage();

 private:
  std::shared_ptr<ros2::PublisherEndpoint> endpoint_;
};

template <typename MessageT>
Writer<MessageT>::Writer(const proto::RoleAttributes& role_attr)
    : WriterBase(role_attr) {}

template <typename MessageT>
Writer<MessageT>::~Writer() {
  Shutdown();
}

template <typename MessageT>
bool Writer<MessageT>::Init() {
  {
    std::lock_guard<std::mutex> g(lock_);
    if (init_) {
      return true;
    }
    if (!node_handle_) {
      node_handle_ = ros2::AuxNodeHandle();
    }
    endpoint_ = ros2::PublisherEndpoint::Create(
        node_handle_, role_attr_, ros2::GetTypeSupport<MessageT>());
    if (endpoint_ == nullptr) {
      AERROR << "Create writer on channel [" << role_attr_.channel_name()
             << "] failed.";
      return false;
    }
    init_ = true;
  }
  return true;
}

template <typename MessageT>
void Writer<MessageT>::Shutdown() {
  std::shared_ptr<ros2::PublisherEndpoint> endpoint;
  {
    std::lock_guard<std::mutex> g(lock_);
    if (!init_) {
      return;
    }
    init_ = false;
    endpoint = std::move(endpoint_);
  }
  if (endpoint) {
    endpoint->Shutdown();
  }
}

template <typename MessageT>
bool Writer<MessageT>::Write(const MessageT& msg) {
  RETURN_VAL_IF(!WriterBase::IsInit(), false);
  auto msg_ptr = std::make_shared<MessageT>(msg);
  return Write(msg_ptr);
}

template <typename MessageT>
bool Writer<MessageT>::Write(const std::shared_ptr<MessageT>& msg_ptr) {
  std::shared_ptr<ros2::PublisherEndpoint> endpoint;
  {
    std::lock_guard<std::mutex> g(lock_);
    if (!init_) {
      return false;
    }
    endpoint = endpoint_;
  }
  return endpoint->Publish(std::static_pointer_cast<const void>(
      std::const_pointer_cast<const MessageT>(msg_ptr)));
}

template <typename MessageT>
bool Writer<MessageT>::HasReader() {
  std::lock_guard<std::mutex> g(lock_);
  return init_ && endpoint_ && endpoint_->PeerCount() > 0;
}

template <typename MessageT>
void Writer<MessageT>::GetReaders(std::vector<proto::RoleAttributes>* readers) {
  if (readers == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> g(lock_);
  if (init_ && endpoint_) {
    endpoint_->GetPeers(readers);
  }
}

template <typename MessageT>
std::shared_ptr<MessageT> Writer<MessageT>::AcquireMessage() {
  return std::make_shared<MessageT>();
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_NODE_WRITER_H_
