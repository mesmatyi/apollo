/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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
#include "cyber/parameter/parameter_server.h"

#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "cyber/common/log.h"
#include "cyber/node/node.h"
#include "cyber/parameter/parameter_service_names.h"

namespace apollo {
namespace cyber {

namespace {

using ParamType = apollo::cyber::proto::ParamType;

/// True on the thread that copies a Cyber parameter into its ROS 2
/// parameter, so the parameter callback does not write it back.
thread_local bool t_mirroring = false;

bool ToRosValue(const proto::Param& param, rclcpp::ParameterValue* value) {
  switch (param.type()) {
    case ParamType::BOOL:
      *value = rclcpp::ParameterValue(param.bool_value());
      return true;
    case ParamType::INT:
      *value = rclcpp::ParameterValue(static_cast<int64_t>(param.int_value()));
      return true;
    case ParamType::DOUBLE:
      *value = rclcpp::ParameterValue(param.double_value());
      return true;
    case ParamType::STRING:
      *value = rclcpp::ParameterValue(param.string_value());
      return true;
    case ParamType::PROTOBUF: {
      const std::string& bytes = param.string_value();
      *value = rclcpp::ParameterValue(
          std::vector<uint8_t>(bytes.begin(), bytes.end()));
      return true;
    }
    default:
      return false;
  }
}

/// Puts the value of a ROS 2 parameter into a Cyber parameter. A protobuf
/// parameter keeps its type name and descriptor.
bool FromRosValue(const rclcpp::ParameterValue& value, proto::Param* param) {
  switch (value.get_type()) {
    case rclcpp::ParameterType::PARAMETER_BOOL:
      param->set_type(ParamType::BOOL);
      param->set_bool_value(value.get<bool>());
      return true;
    case rclcpp::ParameterType::PARAMETER_INTEGER:
      param->set_type(ParamType::INT);
      param->set_int_value(value.get<int64_t>());
      return true;
    case rclcpp::ParameterType::PARAMETER_DOUBLE:
      param->set_type(ParamType::DOUBLE);
      param->set_double_value(value.get<double>());
      return true;
    case rclcpp::ParameterType::PARAMETER_STRING:
      param->set_type(ParamType::STRING);
      param->set_string_value(value.get<std::string>());
      return true;
    case rclcpp::ParameterType::PARAMETER_BYTE_ARRAY: {
      if (param->type() != ParamType::PROTOBUF) {
        return false;
      }
      const auto& bytes = value.get<std::vector<uint8_t>>();
      param->set_string_value(std::string(bytes.begin(), bytes.end()));
      return true;
    }
    default:
      return false;
  }
}

}  // namespace

struct ParameterServer::RosBridge {
  std::shared_ptr<rclcpp::Node> node;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr on_set;
};

ParameterServer::~ParameterServer() {
  if (ros_bridge_) {
    ros_bridge_->on_set.reset();
  }
}

void ParameterServer::Store(const Param& param) {
  Param stored = param;
  bool first_time = false;
  {
    std::lock_guard<std::mutex> lock(param_map_mutex_);
    first_time = param_map_.find(param.name()) == param_map_.end();
  }
  rclcpp::ParameterValue value;
  if (ros_bridge_ && !param.name().empty() && ToRosValue(param, &value)) {
    auto& node = ros_bridge_->node;
    t_mirroring = true;
    try {
      if (!node->has_parameter(param.name())) {
        rcl_interfaces::msg::ParameterDescriptor descriptor;
        descriptor.description = "Cyber parameter (apollo::cyber::ParameterServer)";
        descriptor.dynamic_typing = true;
        node->declare_parameter(param.name(), value, descriptor);
      } else if (first_time) {
        // Given to the node at start (launch file, --ros-args -p): that
        // value takes the place of the first one the program sets.
        const auto given = node->get_parameter(param.name());
        if (given.get_parameter_value() != value &&
            FromRosValue(given.get_parameter_value(), &stored)) {
          AINFO << "parameter [" << param.name()
                << "] takes the value given to the node at start";
        } else {
          node->set_parameter(rclcpp::Parameter(param.name(), value));
        }
      } else {
        node->set_parameter(rclcpp::Parameter(param.name(), value));
      }
    } catch (const std::exception& e) {
      AWARN << "parameter [" << param.name()
            << "] is not shown as a ROS 2 parameter: " << e.what();
    }
    t_mirroring = false;
  }
  std::lock_guard<std::mutex> lock(param_map_mutex_);
  param_map_[stored.name()] = stored;
}

ParameterServer::ParameterServer(const std::shared_ptr<Node>& node)
    : node_(node) {
  auto name = node_->Name();
  if (auto ros_node = node_->RosNode()) {
    ros_bridge_ = std::make_shared<RosBridge>();
    ros_bridge_->node = ros_node;
    // `ros2 param set` on a parameter of this server changes it here too.
    ros_bridge_->on_set = ros_node->add_on_set_parameters_callback(
        [this](const std::vector<rclcpp::Parameter>& parameters) {
          rcl_interfaces::msg::SetParametersResult result;
          result.successful = true;
          if (t_mirroring) {
            return result;
          }
          std::lock_guard<std::mutex> lock(param_map_mutex_);
          for (const auto& parameter : parameters) {
            auto it = param_map_.find(parameter.get_name());
            if (it == param_map_.end()) {
              continue;
            }
            Param updated = it->second;
            if (!FromRosValue(parameter.get_parameter_value(), &updated)) {
              result.successful = false;
              result.reason = "unsupported type for Cyber parameter '" +
                              parameter.get_name() + "'";
              return result;
            }
            it->second = updated;
          }
          return result;
        });
  }
  get_parameter_service_ = node_->CreateService<ParamName, Param>(
      FixParameterServiceName(name, GET_PARAMETER_SERVICE_NAME),
      [this](const std::shared_ptr<ParamName>& request,
             std::shared_ptr<Param>& response) {
        std::lock_guard<std::mutex> lock(param_map_mutex_);
        if (param_map_.find(request->value()) != param_map_.end()) {
          response->CopyFrom(param_map_[request->value()]);
        } else {
          AINFO << "GetParameterService: [" << request->value() << "] not set";
        }
      });

  set_parameter_service_ = node_->CreateService<Param, BoolResult>(
      FixParameterServiceName(name, SET_PARAMETER_SERVICE_NAME),
      [this](const std::shared_ptr<Param>& request,
             std::shared_ptr<BoolResult>& response) {
        Store(*request);
        response->set_value(true);
      });

  list_parameters_service_ = node_->CreateService<NodeName, Params>(
      FixParameterServiceName(name, LIST_PARAMETERS_SERVICE_NAME),
      [this](const std::shared_ptr<NodeName>& request,
             std::shared_ptr<Params>& response) {
        std::lock_guard<std::mutex> lock(param_map_mutex_);
        for (auto& item : param_map_) {
          auto param = response->add_param();
          param->CopyFrom(item.second);
        }
      });
}

void ParameterServer::SetParameter(const Parameter& parameter) {
  Store(parameter.ToProtoParam());
}

bool ParameterServer::GetParameter(const std::string& parameter_name,
                                   Parameter* parameter) {
  std::lock_guard<std::mutex> lock(param_map_mutex_);
  auto ite = param_map_.find(parameter_name);
  if (ite == param_map_.end()) {
    return false;
  }
  parameter->FromProtoParam(ite->second);
  return true;
}

void ParameterServer::ListParameters(std::vector<Parameter>* parameters) {
  std::lock_guard<std::mutex> lock(param_map_mutex_);
  for (auto& item : param_map_) {
    Parameter parameter;
    parameter.FromProtoParam(item.second);
    parameters->emplace_back(parameter);
  }
}

}  // namespace cyber
}  // namespace apollo
