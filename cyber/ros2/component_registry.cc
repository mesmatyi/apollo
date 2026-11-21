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

#include "cyber/ros2/component_registry.h"

#include <cxxabi.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "google/protobuf/text_format.h"

#include "class_loader/class_loader_core.hpp"
#include "class_loader/meta_object.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/node_factory.hpp"
#include "rclcpp_components/node_instance_wrapper.hpp"

#include "cyber/component/component.h"
#include "cyber/component/timer_component.h"
#include "cyber/init.h"
#include "cyber/plugin_manager/plugin_manager.h"
#include "cyber/ros2/naming.h"
#include "cyber/ros2/runtime.h"

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

constexpr char kNodeFactoryBase[] = "rclcpp_components::NodeFactory";

template <typename T>
T DeclareParam(const rclcpp::Node::SharedPtr& node, const std::string& name,
               const T& default_value) {
  if (!node->has_parameter(name)) {
    node->declare_parameter<T>(name, default_value);
  }
  T value = default_value;
  node->get_parameter(name, value);
  return value;
}

/// Builds the component config from the node parameters.
void FillCommonConfig(const rclcpp::Node::SharedPtr& node, std::string* name,
                      std::string* config_file_path,
                      std::string* flag_file_path) {
  if (name->empty()) {
    *name = node->get_name();
  }
  auto conf = DeclareParam<std::string>(node, "config_file_path", "");
  if (!conf.empty()) {
    *config_file_path = conf;
  }
  auto flag = DeclareParam<std::string>(node, "flag_file_path", "");
  if (!flag.empty()) {
    *flag_file_path = flag;
  }
}

bool BuildComponentConfig(const rclcpp::Node::SharedPtr& node,
                          ComponentConfig* config) {
  auto text = DeclareParam<std::string>(node, "component_config", "");
  if (!text.empty() &&
      !google::protobuf::TextFormat::ParseFromString(text, config)) {
    AERROR << "invalid component_config parameter: " << text;
    return false;
  }
  std::string name = config->name();
  std::string conf = config->config_file_path();
  std::string flag = config->flag_file_path();
  FillCommonConfig(node, &name, &conf, &flag);
  config->set_name(name);
  if (!conf.empty()) {
    config->set_config_file_path(conf);
  }
  if (!flag.empty()) {
    config->set_flag_file_path(flag);
  }
  auto readers =
      DeclareParam<std::vector<std::string>>(node, "readers", {});
  auto queue_sizes = DeclareParam<std::vector<int64_t>>(
      node, "reader_pending_queue_sizes", {});
  if (!readers.empty()) {
    config->clear_readers();
    for (size_t i = 0; i < readers.size(); ++i) {
      auto* reader = config->add_readers();
      reader->set_channel(readers[i]);
      if (i < queue_sizes.size() && queue_sizes[i] > 0) {
        reader->set_pending_queue_size(static_cast<uint32_t>(queue_sizes[i]));
      }
    }
  }
  return true;
}

bool BuildTimerComponentConfig(const rclcpp::Node::SharedPtr& node,
                               TimerComponentConfig* config) {
  auto text = DeclareParam<std::string>(node, "component_config", "");
  if (!text.empty() &&
      !google::protobuf::TextFormat::ParseFromString(text, config)) {
    AERROR << "invalid component_config parameter: " << text;
    return false;
  }
  std::string name = config->name();
  std::string conf = config->config_file_path();
  std::string flag = config->flag_file_path();
  FillCommonConfig(node, &name, &conf, &flag);
  config->set_name(name);
  if (!conf.empty()) {
    config->set_config_file_path(conf);
  }
  if (!flag.empty()) {
    config->set_flag_file_path(flag);
  }
  auto interval = DeclareParam<int64_t>(node, "interval", 0);
  if (interval > 0) {
    config->set_interval(static_cast<uint32_t>(interval));
  }
  return true;
}

/// Keeps a component alive for the container and shuts it down on unload.
struct ComponentHolder {
  rclcpp::Node::SharedPtr node;
  std::shared_ptr<ComponentBase> component;
  ~ComponentHolder() {
    if (component) {
      component->Shutdown();
    }
  }
};

class ComponentNodeFactory : public rclcpp_components::NodeFactory {
 public:
  ComponentNodeFactory(std::string class_name, ComponentCreator creator)
      : class_name_(std::move(class_name)), creator_(std::move(creator)) {}

  rclcpp_components::NodeInstanceWrapper create_node_instance(
      const rclcpp::NodeOptions& options) override {
    // Components loaded by a container did not go through cyber::Init().
    if (!cyber::OK()) {
      cyber::Init("apollo_component_container");
    }
    // mainboard loads the installed plugin index before creating
    // components (unless --disable_plugin_autoload); do the same once per
    // container process. APOLLO_DISABLE_PLUGIN_AUTOLOAD=1 opts out.
    static std::once_flag plugins_loaded;
    std::call_once(plugins_loaded, []() {
      const char* disable = std::getenv("APOLLO_DISABLE_PLUGIN_AUTOLOAD");
      if (disable == nullptr || std::string(disable) != "1") {
        plugin_manager::PluginManager::Instance()->LoadInstalledPlugins();
      }
    });
    std::string default_name = class_name_;
    auto pos = default_name.rfind("::");
    if (pos != std::string::npos) {
      default_name = default_name.substr(pos + 2);
    }
    auto node =
        std::make_shared<rclcpp::Node>(ToNodeName(default_name), options);
    auto component = creator_();
    if (!component) {
      throw std::runtime_error("failed to construct " + class_name_);
    }
    component->SetRosNode(node);

    bool ok = false;
    if (std::dynamic_pointer_cast<TimerComponent>(component)) {
      TimerComponentConfig config;
      ok = BuildTimerComponentConfig(node, &config) &&
           component->Initialize(config);
    } else {
      ComponentConfig config;
      ok = BuildComponentConfig(node, &config) &&
           component->Initialize(config);
    }
    if (!ok) {
      component->Shutdown();
      throw std::runtime_error("Apollo component " + class_name_ +
                               " failed to initialize");
    }
    AINFO << "Loaded Apollo component " << class_name_ << " as ROS 2 node "
          << node->get_fully_qualified_name();
    auto holder = std::make_shared<ComponentHolder>();
    holder->node = node;
    holder->component = std::move(component);
    return rclcpp_components::NodeInstanceWrapper(
        std::static_pointer_cast<void>(holder),
        [](const std::shared_ptr<void>& instance) {
          return std::static_pointer_cast<ComponentHolder>(instance)
              ->node->get_node_base_interface();
        });
  }

 private:
  std::string class_name_;
  ComponentCreator creator_;
};

class ComponentMetaObject
    : public ::class_loader::impl::AbstractMetaObject<
          rclcpp_components::NodeFactory> {
 public:
  ComponentMetaObject(const std::string& registered_name,
                      std::string class_name, ComponentCreator creator)
      : ::class_loader::impl::AbstractMetaObject<rclcpp_components::NodeFactory>(
            registered_name, kNodeFactoryBase),
        class_name_(std::move(class_name)),
        creator_(std::move(creator)) {}

  rclcpp_components::NodeFactory* create() const override {
    return new ComponentNodeFactory(class_name_, creator_);
  }

 private:
  std::string class_name_;
  ComponentCreator creator_;
};

/// Mirrors ::class_loader::impl::registerPlugin() for a non-template factory.
::class_loader::impl::UniquePtr<::class_loader::impl::AbstractMetaObjectBase>
RegisterMeta(const std::string& registered_name,
             const std::string& class_name, const ComponentCreator& creator) {
  using ::class_loader::impl::AbstractMetaObjectBase;
  ::class_loader::impl::UniquePtr<AbstractMetaObjectBase> meta(
      new ComponentMetaObject(registered_name, class_name, creator),
      [](AbstractMetaObjectBase* p) {
        ::class_loader::impl::getPluginBaseToFactoryMapMapMutex().lock();
        auto& graveyard = ::class_loader::impl::getMetaObjectGraveyard();
        for (auto it = graveyard.begin(); it != graveyard.end(); ++it) {
          if (*it == p) {
            graveyard.erase(it);
            break;
          }
        }
        auto& map_map = ::class_loader::impl::getGlobalPluginBaseToFactoryMapMap();
        bool erased = false;
        for (auto& item : map_map) {
          auto& factory_map = item.second;
          for (auto it = factory_map.begin(); it != factory_map.end(); ++it) {
            if (it->second == p) {
              factory_map.erase(it);
              erased = true;
              break;
            }
          }
          if (erased) {
            break;
          }
        }
        ::class_loader::impl::getPluginBaseToFactoryMapMapMutex().unlock();
        delete static_cast<ComponentMetaObject*>(p);
      });
  meta->addOwningClassLoader(
      ::class_loader::impl::getCurrentlyActiveClassLoader());
  meta->setAssociatedLibraryPath(
      ::class_loader::impl::getCurrentlyLoadingLibraryName());

  ::class_loader::impl::getPluginBaseToFactoryMapMapMutex().lock();
  auto& factory_map = ::class_loader::impl::getFactoryMapForBaseClass<
      rclcpp_components::NodeFactory>();
  factory_map[registered_name] = meta.get();
  ::class_loader::impl::getPluginBaseToFactoryMapMapMutex().unlock();
  return meta;
}

struct RegistrationToken {
  std::vector<
      ::class_loader::impl::UniquePtr<::class_loader::impl::AbstractMetaObjectBase>>
      metas;
};

}  // namespace

std::string Demangle(const char* mangled_name) {
  int status = 0;
  char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
  if (status != 0 || demangled == nullptr) {
    return mangled_name;
  }
  std::string result(demangled);
  std::free(demangled);
  return result;
}

std::shared_ptr<void> RegisterComponentFactory(const char* mangled_name,
                                               const char* short_name,
                                               ComponentCreator creator) {
  // Only register while a ROS 2 class_loader is opening the library (a
  // component container). Libraries opened by mainboard use Cyber's own
  // class loader registration only.
  if (::class_loader::impl::getCurrentlyActiveClassLoader() == nullptr) {
    return nullptr;
  }
  const std::string fq_name = Demangle(mangled_name);
  std::string short_str(short_name);
  short_str.erase(std::remove_if(short_str.begin(), short_str.end(),
                                 [](char c) { return c == ' '; }),
                  short_str.end());
  std::vector<std::string> names = {fq_name, short_str};
  // Templated components are spelled by Apollo tooling as
  // "<enclosing namespaces>::Name<args as written>" (e.g.
  // apollo::bridge::UDPBridgeSenderComponent<planning::ADCTrajectory>);
  // register that spelling as well.
  const std::string base = short_str.substr(0, short_str.find('<'));
  const std::string unqualified_base = base.substr(
      base.rfind("::") == std::string::npos ? 0 : base.rfind("::") + 2);
  auto pos = fq_name.find(unqualified_base + "<");
  if (pos == std::string::npos && fq_name.size() >= unqualified_base.size() &&
      fq_name.compare(fq_name.size() - unqualified_base.size(),
                      unqualified_base.size(), unqualified_base) == 0) {
    pos = fq_name.size() - unqualified_base.size();
  }
  if (pos != std::string::npos && pos > 0) {
    names.push_back(fq_name.substr(0, pos) +
                    short_str.substr(base.size() - unqualified_base.size()));
  }
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());

  auto token = std::make_shared<RegistrationToken>();
  for (const auto& name : names) {
    token->metas.push_back(RegisterMeta(name, fq_name, creator));
  }
  return token;
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
