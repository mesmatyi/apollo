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

#ifndef CYBER_SERVICE_CLIENT_H_
#define CYBER_SERVICE_CLIENT_H_

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cyber/common/log.h"
#include "cyber/common/types.h"
#include "cyber/message/message_traits.h"
#include "cyber/ros2/transport.h"
#include "cyber/service/client_base.h"

namespace apollo {
namespace cyber {

/**
 * @class Client
 * @brief A Cyber service client, backed by a ROS 2 client of type
 * apollo_msgs/srv/ProtoService. Blocking calls are safe from any thread,
 * including reader callbacks: responses are processed by the Cyber
 * background executor.
 */
template <typename Request, typename Response>
class Client : public ClientBase {
 public:
  using SharedRequest = typename std::shared_ptr<Request>;
  using SharedResponse = typename std::shared_ptr<Response>;
  using Promise = std::promise<SharedResponse>;
  using SharedPromise = std::shared_ptr<Promise>;
  using SharedFuture = std::shared_future<SharedResponse>;
  using CallbackType = std::function<void(SharedFuture)>;

  Client(const std::string& node_name, const std::string& service_name)
      : ClientBase(service_name), node_name_(node_name) {}

  Client() = delete;

  virtual ~Client() {}

  bool Init();

  SharedResponse SendRequest(
      SharedRequest request,
      const std::chrono::seconds& timeout_s = std::chrono::seconds(5));

  SharedResponse SendRequest(
      const Request& request,
      const std::chrono::seconds& timeout_s = std::chrono::seconds(5));

  SharedFuture AsyncSendRequest(SharedRequest request);
  SharedFuture AsyncSendRequest(const Request& request);
  SharedFuture AsyncSendRequest(SharedRequest request, CallbackType&& cb);

  bool ServiceIsReady() const;

  void Destroy();

  template <typename RatioT = std::milli>
  bool WaitForService(std::chrono::duration<int64_t, RatioT> timeout =
                          std::chrono::duration<int64_t, RatioT>(-1)) {
    if (!endpoint_) {
      return false;
    }
    return endpoint_->WaitForService(
        std::chrono::duration_cast<std::chrono::nanoseconds>(timeout));
  }

  /// Selects the ROS 2 node that owns the client (set by Node).
  void SetNodeHandle(const std::shared_ptr<ros2::NodeHandle>& handle) {
    node_handle_ = handle;
  }

 private:
  bool Serialize(const Request& request, std::vector<uint8_t>* out) const {
    return ros2::GetTypeSupport<Request>()->serialize(&request, out);
  }
  SharedResponse Parse(const std::vector<uint8_t>& bytes) const {
    return std::static_pointer_cast<Response>(
        ros2::GetTypeSupport<Response>()->parse(bytes.data(), bytes.size()));
  }

  std::string node_name_;
  std::shared_ptr<ros2::NodeHandle> node_handle_;
  std::shared_ptr<ros2::ClientEndpoint> endpoint_;
};

template <typename Request, typename Response>
void Client<Request, Response>::Destroy() {
  endpoint_.reset();
}

template <typename Request, typename Response>
bool Client<Request, Response>::Init() {
  if (endpoint_) {
    return true;
  }
  endpoint_ = ros2::ClientEndpoint::Create(node_handle_, service_name_,
                                           message::MessageType<Request>(),
                                           message::MessageType<Response>());
  return endpoint_ != nullptr;
}

template <typename Request, typename Response>
typename Client<Request, Response>::SharedResponse
Client<Request, Response>::SendRequest(SharedRequest request,
                                       const std::chrono::seconds& timeout_s) {
  if (!endpoint_ || !request) {
    AERROR << "Client [" << service_name_ << "] not initialized.";
    return nullptr;
  }
  std::vector<uint8_t> bytes;
  if (!Serialize(*request, &bytes)) {
    AERROR << "Client [" << service_name_ << "] failed to serialize request.";
    return nullptr;
  }
  std::vector<uint8_t> response;
  if (!endpoint_->Call(std::move(bytes), &response, timeout_s)) {
    return nullptr;
  }
  return Parse(response);
}

template <typename Request, typename Response>
typename Client<Request, Response>::SharedResponse
Client<Request, Response>::SendRequest(const Request& request,
                                       const std::chrono::seconds& timeout_s) {
  return SendRequest(std::make_shared<Request>(request), timeout_s);
}

template <typename Request, typename Response>
typename Client<Request, Response>::SharedFuture
Client<Request, Response>::AsyncSendRequest(SharedRequest request) {
  return AsyncSendRequest(request, [](SharedFuture) {});
}

template <typename Request, typename Response>
typename Client<Request, Response>::SharedFuture
Client<Request, Response>::AsyncSendRequest(const Request& request) {
  return AsyncSendRequest(std::make_shared<Request>(request));
}

template <typename Request, typename Response>
typename Client<Request, Response>::SharedFuture
Client<Request, Response>::AsyncSendRequest(SharedRequest request,
                                            CallbackType&& cb) {
  auto promise = std::make_shared<Promise>();
  SharedFuture future(promise->get_future());
  std::vector<uint8_t> bytes;
  if (!endpoint_ || !request || !Serialize(*request, &bytes)) {
    promise->set_value(nullptr);
    cb(future);
    return future;
  }
  auto callback = std::make_shared<CallbackType>(std::move(cb));
  endpoint_->AsyncCall(
      std::move(bytes),
      [promise, future, callback](bool ok, const std::vector<uint8_t>& resp) {
        SharedResponse response = nullptr;
        if (ok) {
          response = std::static_pointer_cast<Response>(
              ros2::GetTypeSupport<Response>()->parse(resp.data(),
                                                      resp.size()));
        }
        promise->set_value(response);
        if (*callback) {
          (*callback)(future);
        }
      });
  return future;
}

template <typename Request, typename Response>
bool Client<Request, Response>::ServiceIsReady() const {
  return endpoint_ && endpoint_->ServiceIsReady();
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SERVICE_CLIENT_H_
