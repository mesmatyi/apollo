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

#ifndef CYBER_COMPONENT_COMPONENT_H_
#define CYBER_COMPONENT_COMPONENT_H_

#include <memory>
#include <utility>
#include <vector>

#include "cyber/base/macros.h"
#include "cyber/common/global_data.h"
#include "cyber/common/types.h"
#include "cyber/common/util.h"
#include "cyber/component/component_base.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {

using apollo::cyber::common::GlobalData;
using apollo::cyber::proto::RoleAttributes;

/**
 * @brief The Component can process up to four channels of messages. The
 * message types are specified when the component is created. The Component
 * inherits ComponentBase. Your component can inherit from Component, and
 * implement Init() & Proc(...).
 *
 * The first channel triggers Proc(); the latest message of every other
 * channel is fused in (Proc() is not called until every channel received at
 * least one message), like Cyber's AllLatest data fusion.
 *
 * @tparam M0 first message type, triggers Proc()
 * @tparam M1 second message
 * @tparam M2 third message
 * @tparam M3 fourth message
 */
template <typename M0 = NullType, typename M1 = NullType,
          typename M2 = NullType, typename M3 = NullType>
class Component : public ComponentBase {
 public:
  Component() {}
  ~Component() override {}

  bool Initialize(const ComponentConfig& config) override;
  bool Process(const std::shared_ptr<M0>& msg0, const std::shared_ptr<M1>& msg1,
               const std::shared_ptr<M2>& msg2,
               const std::shared_ptr<M3>& msg3);

 private:
  virtual bool Proc(const std::shared_ptr<M0>& msg0,
                    const std::shared_ptr<M1>& msg1,
                    const std::shared_ptr<M2>& msg2,
                    const std::shared_ptr<M3>& msg3) = 0;
};

template <>
class Component<NullType, NullType, NullType, NullType> : public ComponentBase {
 public:
  Component() {}
  ~Component() override {}
  bool Initialize(const ComponentConfig& config) override;
};

template <typename M0>
class Component<M0, NullType, NullType, NullType> : public ComponentBase {
 public:
  Component() {}
  ~Component() override {}
  bool Initialize(const ComponentConfig& config) override;
  bool Process(const std::shared_ptr<M0>& msg);

 private:
  virtual bool Proc(const std::shared_ptr<M0>& msg) = 0;
};

template <typename M0, typename M1>
class Component<M0, M1, NullType, NullType> : public ComponentBase {
 public:
  Component() {}
  ~Component() override {}
  bool Initialize(const ComponentConfig& config) override;
  bool Process(const std::shared_ptr<M0>& msg0,
               const std::shared_ptr<M1>& msg1);

 private:
  virtual bool Proc(const std::shared_ptr<M0>& msg,
                    const std::shared_ptr<M1>& msg1) = 0;
};

template <typename M0, typename M1, typename M2>
class Component<M0, M1, M2, NullType> : public ComponentBase {
 public:
  Component() {}
  ~Component() override {}
  bool Initialize(const ComponentConfig& config) override;
  bool Process(const std::shared_ptr<M0>& msg0, const std::shared_ptr<M1>& msg1,
               const std::shared_ptr<M2>& msg2);

 private:
  virtual bool Proc(const std::shared_ptr<M0>& msg,
                    const std::shared_ptr<M1>& msg1,
                    const std::shared_ptr<M2>& msg2) = 0;
};

namespace component_internal {

inline ReaderConfig MakeReaderConfig(const ComponentConfig& config, int i) {
  ReaderConfig reader_cfg;
  reader_cfg.channel_name = config.readers(i).channel();
  reader_cfg.qos_profile.CopyFrom(config.readers(i).qos_profile());
  reader_cfg.pending_queue_size = config.readers(i).pending_queue_size();
  return reader_cfg;
}

}  // namespace component_internal

inline bool Component<NullType, NullType, NullType, NullType>::Initialize(
    const ComponentConfig& config) {
  node_ = CreateComponentNode(config.name());
  LoadConfigFiles(config);
  if (!Init()) {
    AERROR << "Component Init() failed." << std::endl;
    return false;
  }
  return true;
}

template <typename M0>
bool Component<M0, NullType, NullType, NullType>::Process(
    const std::shared_ptr<M0>& msg) {
  if (is_shutdown_.load()) {
    return true;
  }
  return Proc(msg);
}

template <typename M0>
bool Component<M0, NullType, NullType, NullType>::Initialize(
    const ComponentConfig& config) {
  node_ = CreateComponentNode(config.name());
  LoadConfigFiles(config);

  if (config.readers_size() < 1) {
    AERROR << "Invalid config file: too few readers.";
    return false;
  }

  if (!Init()) {
    AERROR << "Component Init() failed.";
    return false;
  }

  std::weak_ptr<Component<M0>> self =
      std::dynamic_pointer_cast<Component<M0>>(shared_from_this());
  auto func = [self](const std::shared_ptr<M0>& msg) {
    auto ptr = self.lock();
    if (ptr) {
      ptr->Process(msg);
    } else {
      AERROR << "Component object has been destroyed.";
    }
  };

  auto reader = node_->template CreateReader<M0>(
      component_internal::MakeReaderConfig(config, 0), func);
  if (reader == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }
  readers_.emplace_back(std::move(reader));
  return true;
}

template <typename M0, typename M1>
bool Component<M0, M1, NullType, NullType>::Process(
    const std::shared_ptr<M0>& msg0, const std::shared_ptr<M1>& msg1) {
  if (is_shutdown_.load()) {
    return true;
  }
  return Proc(msg0, msg1);
}

template <typename M0, typename M1>
bool Component<M0, M1, NullType, NullType>::Initialize(
    const ComponentConfig& config) {
  node_ = CreateComponentNode(config.name());
  LoadConfigFiles(config);

  if (config.readers_size() < 2) {
    AERROR << "Invalid config file: too few readers.";
    return false;
  }

  if (!Init()) {
    AERROR << "Component Init() failed.";
    return false;
  }

  auto reader1 = node_->template CreateReader<M1>(
      component_internal::MakeReaderConfig(config, 1));
  if (reader1 == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }

  std::weak_ptr<Component<M0, M1>> self =
      std::dynamic_pointer_cast<Component<M0, M1>>(shared_from_this());
  std::weak_ptr<Reader<M1>> weak1 = reader1;
  auto func = [self, weak1](const std::shared_ptr<M0>& msg0) {
    auto ptr = self.lock();
    auto r1 = weak1.lock();
    if (!ptr || !r1) {
      return;
    }
    auto msg1 = r1->GetLatestReceived();
    if (msg1 == nullptr) {
      return;
    }
    ptr->Process(msg0, msg1);
  };

  auto reader0 = node_->template CreateReader<M0>(
      component_internal::MakeReaderConfig(config, 0), func);
  if (reader0 == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }
  readers_.push_back(std::move(reader0));
  readers_.push_back(std::move(reader1));
  return true;
}

template <typename M0, typename M1, typename M2>
bool Component<M0, M1, M2, NullType>::Process(const std::shared_ptr<M0>& msg0,
                                              const std::shared_ptr<M1>& msg1,
                                              const std::shared_ptr<M2>& msg2) {
  if (is_shutdown_.load()) {
    return true;
  }
  return Proc(msg0, msg1, msg2);
}

template <typename M0, typename M1, typename M2>
bool Component<M0, M1, M2, NullType>::Initialize(
    const ComponentConfig& config) {
  node_ = CreateComponentNode(config.name());
  LoadConfigFiles(config);

  if (config.readers_size() < 3) {
    AERROR << "Invalid config file: too few readers.";
    return false;
  }

  if (!Init()) {
    AERROR << "Component Init() failed.";
    return false;
  }

  auto reader1 = node_->template CreateReader<M1>(
      component_internal::MakeReaderConfig(config, 1));
  auto reader2 = node_->template CreateReader<M2>(
      component_internal::MakeReaderConfig(config, 2));
  if (reader1 == nullptr || reader2 == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }

  std::weak_ptr<Component<M0, M1, M2>> self =
      std::dynamic_pointer_cast<Component<M0, M1, M2>>(shared_from_this());
  std::weak_ptr<Reader<M1>> weak1 = reader1;
  std::weak_ptr<Reader<M2>> weak2 = reader2;
  auto func = [self, weak1, weak2](const std::shared_ptr<M0>& msg0) {
    auto ptr = self.lock();
    auto r1 = weak1.lock();
    auto r2 = weak2.lock();
    if (!ptr || !r1 || !r2) {
      return;
    }
    auto msg1 = r1->GetLatestReceived();
    auto msg2 = r2->GetLatestReceived();
    if (msg1 == nullptr || msg2 == nullptr) {
      return;
    }
    ptr->Process(msg0, msg1, msg2);
  };

  auto reader0 = node_->template CreateReader<M0>(
      component_internal::MakeReaderConfig(config, 0), func);
  if (reader0 == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }
  readers_.push_back(std::move(reader0));
  readers_.push_back(std::move(reader1));
  readers_.push_back(std::move(reader2));
  return true;
}

template <typename M0, typename M1, typename M2, typename M3>
bool Component<M0, M1, M2, M3>::Process(const std::shared_ptr<M0>& msg0,
                                        const std::shared_ptr<M1>& msg1,
                                        const std::shared_ptr<M2>& msg2,
                                        const std::shared_ptr<M3>& msg3) {
  if (is_shutdown_.load()) {
    return true;
  }
  return Proc(msg0, msg1, msg2, msg3);
}

template <typename M0, typename M1, typename M2, typename M3>
bool Component<M0, M1, M2, M3>::Initialize(const ComponentConfig& config) {
  node_ = CreateComponentNode(config.name());
  LoadConfigFiles(config);

  if (config.readers_size() < 4) {
    AERROR << "Invalid config file: too few readers.";
    return false;
  }

  if (!Init()) {
    AERROR << "Component Init() failed.";
    return false;
  }

  auto reader1 = node_->template CreateReader<M1>(
      component_internal::MakeReaderConfig(config, 1));
  auto reader2 = node_->template CreateReader<M2>(
      component_internal::MakeReaderConfig(config, 2));
  auto reader3 = node_->template CreateReader<M3>(
      component_internal::MakeReaderConfig(config, 3));
  if (reader1 == nullptr || reader2 == nullptr || reader3 == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }

  std::weak_ptr<Component<M0, M1, M2, M3>> self =
      std::dynamic_pointer_cast<Component<M0, M1, M2, M3>>(shared_from_this());
  std::weak_ptr<Reader<M1>> weak1 = reader1;
  std::weak_ptr<Reader<M2>> weak2 = reader2;
  std::weak_ptr<Reader<M3>> weak3 = reader3;
  auto func = [self, weak1, weak2, weak3](const std::shared_ptr<M0>& msg0) {
    auto ptr = self.lock();
    auto r1 = weak1.lock();
    auto r2 = weak2.lock();
    auto r3 = weak3.lock();
    if (!ptr || !r1 || !r2 || !r3) {
      return;
    }
    auto msg1 = r1->GetLatestReceived();
    auto msg2 = r2->GetLatestReceived();
    auto msg3 = r3->GetLatestReceived();
    if (msg1 == nullptr || msg2 == nullptr || msg3 == nullptr) {
      return;
    }
    ptr->Process(msg0, msg1, msg2, msg3);
  };

  auto reader0 = node_->template CreateReader<M0>(
      component_internal::MakeReaderConfig(config, 0), func);
  if (reader0 == nullptr) {
    AERROR << "Component create reader failed.";
    return false;
  }
  readers_.push_back(std::move(reader0));
  readers_.push_back(std::move(reader1));
  readers_.push_back(std::move(reader2));
  readers_.push_back(std::move(reader3));
  return true;
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_COMPONENT_COMPONENT_H_
