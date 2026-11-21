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

#ifndef CYBER_SERVICE_DISCOVERY_TOPOLOGY_MANAGER_H_
#define CYBER_SERVICE_DISCOVERY_TOPOLOGY_MANAGER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "cyber/common/macros.h"
#include "cyber/service_discovery/specific_manager/channel_manager.h"
#include "cyber/service_discovery/specific_manager/node_manager.h"
#include "cyber/service_discovery/specific_manager/service_manager.h"

namespace apollo {
namespace cyber {
namespace service_discovery {

class NodeManager;
using NodeManagerPtr = std::shared_ptr<NodeManager>;
class ChannelManager;
using ChannelManagerPtr = std::shared_ptr<ChannelManager>;
class ServiceManager;
using ServiceManagerPtr = std::shared_ptr<ServiceManager>;

/**
 * @class TopologyManager
 * @brief Elements in the Cyber topology -- Node, Channel, Service, Writer,
 * Reader, Client and Server -- read from the ROS 2 graph. A background
 * thread watches graph events and notifies change listeners with the same
 * ChangeMsg messages Cyber produced.
 */
class TopologyManager {
 public:
  using ChangeSignal = base::Signal<const ChangeMsg&>;
  using ChangeFunc = std::function<void(const ChangeMsg&)>;
  using ChangeConnection = base::Connection<const ChangeMsg&>;

  virtual ~TopologyManager();

  void Shutdown();

  ChangeConnection AddChangeListener(const ChangeFunc& func);
  void RemoveChangeListener(const ChangeConnection& conn);

  NodeManagerPtr& node_manager() { return node_manager_; }
  ChannelManagerPtr& channel_manager() { return channel_manager_; }
  ServiceManagerPtr& service_manager() { return service_manager_; }

 private:
  void EnsureWatcher();
  void Watch();

  std::atomic<bool> running_ = {false};
  std::mutex mutex_;
  std::thread watcher_;
  NodeManagerPtr node_manager_;
  ChannelManagerPtr channel_manager_;
  ServiceManagerPtr service_manager_;
  ChangeSignal change_signal_;

  friend class Manager;

  DECLARE_SINGLETON(TopologyManager)
};

}  // namespace service_discovery
}  // namespace cyber
}  // namespace apollo

#endif  //  CYBER_SERVICE_DISCOVERY_TOPOLOGY_MANAGER_H_
