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

#ifndef CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_MANAGER_H_
#define CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_MANAGER_H_

#include <functional>
#include <string>
#include <vector>

#include "cyber/proto/role_attributes.pb.h"
#include "cyber/proto/topology_change.pb.h"

#include "cyber/base/signal.h"

namespace apollo {
namespace cyber {
namespace service_discovery {

using proto::ChangeMsg;
using proto::ChangeType;
using proto::OperateType;
using proto::RoleAttributes;
using proto::RoleType;

/**
 * @class Manager
 * @brief Base of the topology managers. On ROS 2 the topology is the ROS 2
 * graph (discovered by the rmw layer); Join()/Leave() are no-ops kept for
 * source compatibility and change notifications are derived from graph
 * changes.
 */
class Manager {
 public:
  using ChangeSignal = base::Signal<const ChangeMsg&>;
  using ChangeFunc = std::function<void(const ChangeMsg&)>;
  using ChangeConnection = base::Connection<const ChangeMsg&>;
  using RoleAttrVec = std::vector<RoleAttributes>;

  Manager() = default;
  virtual ~Manager() = default;

  bool StartDiscovery(void* participant) { return true; }
  void StopDiscovery() {}
  virtual void Shutdown() { signal_.DisconnectAllSlots(); }

  bool Join(const RoleAttributes& attr, RoleType role,
            bool need_publish = true) {
    return true;
  }
  bool Leave(const RoleAttributes& attr, RoleType role) { return true; }

  ChangeConnection AddChangeListener(const ChangeFunc& func);
  void RemoveChangeListener(const ChangeConnection& conn);

  /// Internal: dispatches a graph change to listeners of this manager.
  void Notify(const ChangeMsg& msg) { signal_(msg); }

 protected:
  ChangeSignal signal_;
};

}  // namespace service_discovery
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_MANAGER_H_
