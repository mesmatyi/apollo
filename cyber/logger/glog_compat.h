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

#ifndef CYBER_LOGGER_GLOG_COMPAT_H_
#define CYBER_LOGGER_GLOG_COMPAT_H_

#include <chrono>
#include <cstddef>
#include <ctime>

#include "glog/logging.h"

// google::base::Logger::Write() changed signature across glog releases.
// CYBER_GLOG_LOGGER_API is detected at configure time (cyber/CMakeLists.txt):
//   3: glog >= 0.7  Write(bool, const system_clock::time_point&, const char*,
//                         size_t)
//   2: glog 0.6     Write(bool, time_t, const char*, size_t)
//   1: glog <= 0.5  Write(bool, time_t, const char*, int)
#ifndef CYBER_GLOG_LOGGER_API
#define CYBER_GLOG_LOGGER_API 2
#endif

namespace apollo {
namespace cyber {
namespace logger {

#if CYBER_GLOG_LOGGER_API >= 3
using GlogTimestamp = std::chrono::system_clock::time_point;
using GlogTimestampArg = const std::chrono::system_clock::time_point&;
inline time_t GlogToTimeT(GlogTimestampArg t) {
  return std::chrono::system_clock::to_time_t(t);
}
inline bool GlogIsZeroTimestamp(GlogTimestampArg t) {
  return t.time_since_epoch().count() == 0;
}
#else
using GlogTimestamp = time_t;
using GlogTimestampArg = time_t;
inline time_t GlogToTimeT(time_t t) { return t; }
inline bool GlogIsZeroTimestamp(time_t t) { return t == 0; }
#endif

#if CYBER_GLOG_LOGGER_API >= 2
using GlogLength = size_t;
#else
using GlogLength = int;
#endif

inline google::LogSeverity ToLogSeverity(int level) {
  return static_cast<google::LogSeverity>(level);
}

}  // namespace logger
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_LOGGER_GLOG_COMPAT_H_
