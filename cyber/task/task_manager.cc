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

#include "cyber/task/task_manager.h"

#include <cstdlib>

namespace apollo {
namespace cyber {

TaskManager::TaskManager() {
  const char* env = std::getenv("APOLLO_TASK_THREADS");
  int n = env != nullptr ? std::atoi(env) : 0;
  if (n <= 0) {
    n = static_cast<int>(std::max(2u, std::thread::hardware_concurrency()));
  }
  num_threads_ = static_cast<uint32_t>(n);
  for (uint32_t i = 0; i < num_threads_; ++i) {
    workers_.emplace_back([this]() { Worker(); });
  }
}

TaskManager::~TaskManager() { Shutdown(); }

void TaskManager::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) {
      return;
    }
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void TaskManager::Worker() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    task();
  }
}

}  // namespace cyber
}  // namespace apollo
