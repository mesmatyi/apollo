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

#ifndef CYBER_TIMER_TIMER_H_
#define CYBER_TIMER_TIMER_H_

#include <atomic>
#include <functional>
#include <memory>

#include "cyber/ros2/transport.h"

namespace apollo {
namespace cyber {

/**
 * @brief The options of timer
 */
struct TimerOption {
  /**
   * @brief Construct a new Timer Option object
   *
   * @param period The period of the timer, unit is ms
   * @param callback The task that the timer needs to perform
   * @param oneshot Oneshot or period
   */
  TimerOption(uint32_t period, std::function<void()> callback, bool oneshot)
      : period(period), callback(callback), oneshot(oneshot) {}

  TimerOption() : period(), callback(), oneshot() {}

  uint32_t period = 0;             ///< The period of the timer, unit is ms
  std::function<void()> callback;  ///< The task that the timer performs
  bool oneshot;  ///< True: perform the callback only after the first timing
                 ///< cycle; False: perform the callback every timed period
};

/**
 * @class Timer
 * @brief Used to perform oneshot or periodic timing tasks. Backed by an
 * rclcpp wall timer spun by the Cyber background executor.
 */
class Timer {
 public:
  Timer();
  explicit Timer(TimerOption opt);
  Timer(uint32_t period, std::function<void()> callback, bool oneshot);
  ~Timer();

  void SetTimerOption(TimerOption opt);
  void Start();
  void Stop();

 private:
  TimerOption timer_opt_;
  std::shared_ptr<ros2::TimerEndpoint> endpoint_;
  std::atomic<bool> started_ = {false};
};

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TIMER_TIMER_H_
