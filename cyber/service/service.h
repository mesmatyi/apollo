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

#ifndef CYBER_SERVICE_SERVICE_H_
#define CYBER_SERVICE_SERVICE_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cyber/common/log.h"
#include "cyber/common/types.h"
#include "cyber/message/message_traits.h"
#include "cyber/ros2/transport.h"
#include "cyber/service/service_base.h"

namespace apollo {
namespace cyber {

/**
 * @class Service
 * @brief A Cyber service, served as a ROS 2 service of type
 * apollo_msgs/srv/ProtoService. Requests are handled one at a time.
 */
template <typename Request, typename Response>
class Service : public ServiceBase {
 public:
  using ServiceCallback = std::function<void(const std::shared_ptr<Request>&,
                                             std::shared_ptr<Response>&)>;

  Service(const std::string& node_name, const std::string& service_name,
          const ServiceCallback& service_callback)
      : ServiceBase(service_name),
        node_name_(node_name),
        service_callback_(service_callback) {}

  Service(const std::string& node_name, const std::string& service_name,
          ServiceCallback&& service_callback)
      : ServiceBase(service_name),
        node_name_(node_name),
        service_callback_(std::move(service_callback)) {}

  Service() = delete;

  ~Service() { destroy(); }

  bool Init();
  void destroy();

  /// Selects the ROS 2 node that owns the service (set by Node).
  void SetNodeHandle(const std::shared_ptr<ros2::NodeHandle>& handle) {
    node_handle_ = handle;
  }

 private:
  bool IsInit() const { return endpoint_ != nullptr; }

  std::string node_name_;
  ServiceCallback service_callback_;
  std::shared_ptr<ros2::NodeHandle> node_handle_;
  std::shared_ptr<ros2::ServiceEndpoint> endpoint_;
};

template <typename Request, typename Response>
bool Service<Request, Response>::Init() {
  if (IsInit()) {
    return true;
  }
  if (!node_handle_) {
    node_handle_ = ros2::AuxNodeHandle();
  }
  auto callback = service_callback_;
  const auto* response_ts = ros2::GetTypeSupport<Response>();
  const auto* request_ts = ros2::GetTypeSupport<Request>();
  const std::string name = service_name_;
  endpoint_ = ros2::ServiceEndpoint::Create(
      node_handle_, service_name_, message::MessageType<Request>(),
      message::MessageType<Response>(),
      [callback, request_ts, response_ts, name](
          const uint8_t* data, size_t size,
          std::vector<uint8_t>* response_bytes) -> bool {
        auto request =
            std::static_pointer_cast<Request>(request_ts->parse(data, size));
        if (!request) {
          AERROR << "Service [" << name << "] failed to parse request.";
          return false;
        }
        auto response = std::make_shared<Response>();
        callback(request, response);
        if (!response) {
          return false;
        }
        return response_ts->serialize(response.get(), response_bytes);
      });
  return endpoint_ != nullptr;
}

template <typename Request, typename Response>
void Service<Request, Response>::destroy() {
  if (endpoint_) {
    endpoint_->Shutdown();
    endpoint_.reset();
  }
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SERVICE_SERVICE_H_
