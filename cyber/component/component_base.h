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

#ifndef CYBER_COMPONENT_COMPONENT_BASE_H_
#define CYBER_COMPONENT_COMPONENT_BASE_H_

#include <atomic>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

#include "gflags/gflags.h"

#include "cyber/proto/component_conf.pb.h"

#include "cyber/class_loader/class_loader.h"
#include "cyber/common/environment.h"
#include "cyber/common/file.h"
#include "cyber/node/node.h"
#include "cyber/ros2/component_registry.h"
#include "cyber/ros2/parameters.h"

namespace rclcpp {
class Node;
}  // namespace rclcpp

namespace apollo {
namespace cyber {

using apollo::cyber::proto::ComponentConfig;
using apollo::cyber::proto::TimerComponentConfig;

/**
 * @class ComponentBase
 * @brief Base of every Apollo component.
 *
 * A component runs either inside `mainboard` (dag files) or inside any ROS 2
 * component container (it is registered as an rclcpp_components node
 * factory by CYBER_REGISTER_COMPONENT).
 */
class ComponentBase : public std::enable_shared_from_this<ComponentBase> {
 public:
  template <typename M>
  using Reader = cyber::Reader<M>;

  virtual ~ComponentBase() {}

  virtual bool Initialize(const ComponentConfig& config) { return false; }
  virtual bool Initialize(const TimerComponentConfig& config) { return false; }
  virtual void Shutdown() {
    if (is_shutdown_.exchange(true)) {
      return;
    }
    Clear();
    for (auto& reader : readers_) {
      reader->Shutdown();
    }
  }

  template <typename T>
  bool GetProtoConfig(T* config) const {
    return common::GetProtoFromFile(config_file_path_, config);
  }

  /// Internal: when set before Initialize(), the component uses this ROS 2
  /// node (owned and spun by a component container) as its Cyber node.
  void SetRosNode(const std::shared_ptr<rclcpp::Node>& node) {
    ros_node_ = node;
  }

 protected:
  virtual bool Init() = 0;
  virtual void Clear() { return; }
  const std::string& ConfigFilePath() const { return config_file_path_; }

  std::shared_ptr<Node> CreateComponentNode(const std::string& name) {
    if (ros_node_) {
      return std::make_shared<Node>(ros_node_);
    }
    return std::make_shared<Node>(name);
  }

  void LoadConfigFiles(const ComponentConfig& config) {
    LoadConfigFiles(config.config_file_path(), config.flag_file_path());
  }

  void LoadConfigFiles(const TimerComponentConfig& config) {
    LoadConfigFiles(config.config_file_path(), config.flag_file_path());
  }

  void LoadConfigFiles(const std::string& config_file_path,
                       const std::string& flag_file) {
    if (!config_file_path.empty()) {
      if (!common::GetFilePathWithEnv(config_file_path, "APOLLO_CONF_PATH",
                                      &config_file_path_)) {
        AERROR << "conf file [" << config_file_path << "] not found!";
        config_file_path_ = config_file_path;
      } else {
        AINFO << "use config file: " << config_file_path_;
      }
    }
    if (!flag_file.empty()) {
      std::string flag_file_path = flag_file;
      if (!common::GetFilePathWithEnv(flag_file, "APOLLO_FLAG_PATH",
                                      &flag_file_path)) {
        AERROR << "flag file [" << flag_file << "] not found!";
      } else {
        AINFO << "use flag file: " << flag_file_path;
      }
      google::SetCommandLineOption("flagfile", flag_file_path.c_str());
      // gflags given as ROS 2 parameters (gflags.<name>) keep precedence
      // over the flag file.
      ros2::ApplyFlagOverrides();
    }
  }

  std::atomic<bool> is_shutdown_ = {false};
  std::shared_ptr<Node> node_ = nullptr;
  std::string config_file_path_ = "";
  std::vector<std::shared_ptr<ReaderBase>> readers_;
  std::shared_ptr<rclcpp::Node> ros_node_;
};

}  // namespace cyber
}  // namespace apollo

#define CYBER_ROS2_REGISTER_COMPONENT_INTERNAL(ComponentClass, UniqueID)             \
  namespace {                                                              \
  struct CyberRos2ComponentRegistrar##UniqueID {                           \
    CyberRos2ComponentRegistrar##UniqueID()                                \
        : token(::apollo::cyber::ros2::RegisterComponentFactory(           \
              typeid(ComponentClass).name(), #ComponentClass,                                  \
              []() -> std::shared_ptr<::apollo::cyber::ComponentBase> {    \
                return std::make_shared<ComponentClass>();                           \
              })) {}                                                       \
    std::shared_ptr<void> token;                                           \
  };                                                                       \
  static CyberRos2ComponentRegistrar##UniqueID                             \
      g_cyber_ros2_component_registrar_##UniqueID;                         \
  }  // namespace

#define CYBER_ROS2_REGISTER_COMPONENT_INTERNAL_1(ComponentClass, UniqueID) \
  CYBER_ROS2_REGISTER_COMPONENT_INTERNAL(ComponentClass, UniqueID)

/**
 * Registers a component with Cyber's class loader (for mainboard / dag
 * files) and with ROS 2 class_loader as an rclcpp_components node factory
 * (for component containers and launch files).
 */
#define CYBER_REGISTER_COMPONENT(ComponentClass)                             \
  CLASS_LOADER_REGISTER_CLASS(ComponentClass, apollo::cyber::ComponentBase) \
  CYBER_ROS2_REGISTER_COMPONENT_INTERNAL_1(ComponentClass, __COUNTER__)

#endif  // CYBER_COMPONENT_COMPONENT_BASE_H_
