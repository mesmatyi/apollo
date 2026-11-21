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

#ifndef CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_SERVICE_MANAGER_H_
#define CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_SERVICE_MANAGER_H_

#include <string>

#include "cyber/service_discovery/specific_manager/manager.h"

namespace apollo {
namespace cyber {
namespace service_discovery {

/// Service topology (services of a native Apollo service type or of
/// apollo_msgs/srv/ProtoService), from the ROS 2 graph.
class ServiceManager : public Manager {
 public:
  ServiceManager() = default;
  virtual ~ServiceManager() = default;

  bool HasService(const std::string& service_name);
  void GetServers(RoleAttrVec* servers);
  void GetClients(const std::string& service_name, RoleAttrVec* clients);
};

}  // namespace service_discovery
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_SERVICE_MANAGER_H_
