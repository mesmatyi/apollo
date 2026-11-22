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

#ifndef CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_CHANNEL_MANAGER_H_
#define CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_CHANNEL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "cyber/service_discovery/specific_manager/manager.h"

namespace apollo {
namespace cyber {
namespace service_discovery {

/**
 * @class ChannelManager
 * @brief Channel topology (writers/readers of Apollo channels), read from
 * the ROS 2 graph. Topics whose type is a native Apollo type (generated
 * from an Apollo protobuf message) or apollo_msgs/msg/ProtoMessage are
 * reported as channels.
 */
class ChannelManager : public Manager {
 public:
  ChannelManager() = default;
  virtual ~ChannelManager() = default;

  void GetChannelNames(std::vector<std::string>* channels);
  void GetProtoDesc(const std::string& channel_name, std::string* proto_desc);
  void GetMsgType(const std::string& channel_name, std::string* msg_type);

  bool HasWriter(const std::string& channel_name);
  void GetWriters(RoleAttrVec* writers);
  void GetWritersOfNode(const std::string& node_name, RoleAttrVec* writers);
  void GetWritersOfChannel(const std::string& channel_name,
                           RoleAttrVec* writers);

  bool HasReader(const std::string& channel_name);
  void GetReaders(RoleAttrVec* readers);
  void GetReadersOfNode(const std::string& node_name, RoleAttrVec* readers);
  void GetReadersOfChannel(const std::string& channel_name,
                           RoleAttrVec* readers);

  void GetUpstreamOfNode(const std::string& node_name, RoleAttrVec* upstream);
  void GetDownstreamOfNode(const std::string& node_name,
                           RoleAttrVec* downstream);

  bool IsMessageTypeMatching(const std::string& lhs, const std::string& rhs) {
    return lhs == rhs || lhs == "apollo.cyber.message.RawMessage" ||
           rhs == "apollo.cyber.message.RawMessage";
  }
};

}  // namespace service_discovery
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SERVICE_DISCOVERY_SPECIFIC_MANAGER_CHANNEL_MANAGER_H_
