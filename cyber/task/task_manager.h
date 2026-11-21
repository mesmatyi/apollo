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

#ifndef CYBER_TASK_TASK_MANAGER_H_
#define CYBER_TASK_TASK_MANAGER_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "cyber/common/macros.h"

namespace apollo {
namespace cyber {

/**
 * @class TaskManager
 * @brief Thread pool backing cyber::Async().
 */
class TaskManager {
 public:
  virtual ~TaskManager();

  void Shutdown();

  template <typename F, typename... Args>
  auto Enqueue(F&& func, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stop_) {
        // Run inline once shut down so callers never hang.
        (*task)();
        return res;
      }
      tasks_.emplace_back([task]() { (*task)(); });
    }
    cv_.notify_one();
    return res;
  }

 private:
  void Worker();

  uint32_t num_threads_ = 0;
  std::vector<std::thread> workers_;
  std::deque<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;

  DECLARE_SINGLETON(TaskManager);
};

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TASK_TASK_MANAGER_H_
