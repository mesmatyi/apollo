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

#ifndef CYBER_NODE_NODE_CHANNEL_IMPL_H_
#define CYBER_NODE_NODE_CHANNEL_IMPL_H_

#include <string>

#include "cyber/proto/qos_profile.pb.h"

#include "cyber/node/reader.h"
#include "cyber/node/writer.h"

namespace apollo {
namespace cyber {

/// Configuration of a Reader created through Node::CreateReader().
struct ReaderConfig {
  ReaderConfig() {
    qos_profile.set_history(proto::QosHistoryPolicy::HISTORY_KEEP_LAST);
    qos_profile.set_depth(1);
    qos_profile.set_mps(0);
    qos_profile.set_reliability(
        proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    qos_profile.set_durability(proto::QosDurabilityPolicy::DURABILITY_VOLATILE);

    pending_queue_size = DEFAULT_PENDING_QUEUE_SIZE;
  }
  ReaderConfig(const ReaderConfig& other)
      : channel_name(other.channel_name),
        qos_profile(other.qos_profile),
        pending_queue_size(other.pending_queue_size) {}
  ReaderConfig& operator=(const ReaderConfig& other) = default;

  std::string channel_name;       ///< channel to read
  proto::QosProfile qos_profile;  ///< QoS (history depth, reliability, ...)
  /// Size of the queue of messages waiting for the callback; older messages
  /// are dropped when the callback cannot keep up (ROS 2 subscription depth).
  uint32_t pending_queue_size;
};

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_NODE_NODE_CHANNEL_IMPL_H_
