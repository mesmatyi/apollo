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

#ifndef CYBER_TRANSPORT_SHM_PROTOBUF_ARENA_MANAGER_H_
#define CYBER_TRANSPORT_SHM_PROTOBUF_ARENA_MANAGER_H_

#include <cstdint>

#include "cyber/common/macros.h"

namespace apollo {
namespace cyber {
namespace transport {

/**
 * @class ProtobufArenaManager
 * @brief Cyber's shared-memory protobuf arenas are not used on ROS 2:
 * in-process readers already share the published object (zero copy) and
 * cross-process traffic goes through the RMW layer. Enable() always returns
 * false so callers take their regular heap-allocation path.
 */
class ProtobufArenaManager {
 public:
  bool Enable() { return false; }
  bool EnableSegment(uint64_t channel_id) { return false; }
  template <typename T>
  bool RegisterQueue(uint64_t channel_id, uint64_t size) {
    return false;
  }
  void* GetAvailableBuffer(uint64_t channel_id) { return nullptr; }

 private:
  DECLARE_SINGLETON(ProtobufArenaManager)
};

inline ProtobufArenaManager::ProtobufArenaManager() {}

}  // namespace transport
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TRANSPORT_SHM_PROTOBUF_ARENA_MANAGER_H_
