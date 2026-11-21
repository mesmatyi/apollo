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

#ifndef CYBER_NODE_READER_BASE_H_
#define CYBER_NODE_READER_BASE_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "cyber/proto/qos_profile.pb.h"
#include "cyber/proto/role_attributes.pb.h"

#include "cyber/common/global_data.h"
#include "cyber/common/macros.h"

namespace apollo {
namespace cyber {

namespace ros2 {
class NodeHandle;
}  // namespace ros2

using apollo::cyber::common::GlobalData;

/**
 * @class ReaderBase
 * @brief Base class of every Reader. Backed by a ROS 2 subscription plus a
 * zero-copy intra-process path (see cyber/ros2/transport.h).
 */
class ReaderBase {
 public:
  explicit ReaderBase(const proto::RoleAttributes& role_attr)
      : role_attr_(role_attr), init_(false) {}
  virtual ~ReaderBase() {}

  virtual bool Init() = 0;
  virtual void Shutdown() = 0;
  virtual void ClearData() = 0;
  virtual void Observe() = 0;
  virtual bool Empty() const = 0;
  virtual bool HasReceived() const = 0;
  virtual double GetDelaySec() const = 0;
  virtual uint32_t PendingQueueSize() const = 0;
  virtual bool HasWriter() { return false; }
  virtual void GetWriters(std::vector<proto::RoleAttributes>* writers) {}

  const std::string& GetChannelName() const {
    return role_attr_.channel_name();
  }
  uint64_t ChannelId() const { return role_attr_.channel_id(); }
  const proto::QosProfile& QosProfile() const {
    return role_attr_.qos_profile();
  }
  bool IsInit() const { return init_.load(); }

  /// Selects the ROS 2 node that owns the subscription. Called by Node
  /// before Init(); readers created standalone use the process aux node.
  void SetNodeHandle(const std::shared_ptr<ros2::NodeHandle>& handle) {
    node_handle_ = handle;
  }

 protected:
  proto::RoleAttributes role_attr_;
  std::atomic<bool> init_;
  std::shared_ptr<ros2::NodeHandle> node_handle_;
};

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_NODE_READER_BASE_H_
