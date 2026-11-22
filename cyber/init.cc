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

#include "cyber/init.h"

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>

#include "gflags/gflags.h"
#include "rclcpp/rclcpp.hpp"

#include "cyber/proto/clock.pb.h"

#include "cyber/binary.h"
#include "cyber/common/file.h"
#include "cyber/common/global_data.h"
#include "cyber/logger/async_logger.h"
#include "cyber/node/node.h"
#include "cyber/ros2/parameters.h"
#include "cyber/ros2/runtime.h"
#include "cyber/ros2/paths.h"
#include "cyber/task/task_manager.h"
#include "cyber/time/clock.h"

namespace apollo {
namespace cyber {

namespace {

const std::string& kClockChannel = "/clock";
const std::string& kClockNode = "clock";

bool g_atexit_registered = false;
std::mutex g_mutex;
std::unique_ptr<Node> clock_node;
logger::AsyncLogger* async_logger = nullptr;
google::base::Logger* original_logger = nullptr;
google::LogSeverity logger_severity = google::INFO;

void InitLogger(const char* binary_name) {
  const char* slash = strrchr(binary_name, '/');
  if (slash) {
    ::apollo::cyber::binary::SetName(slash + 1);
  } else {
    ::apollo::cyber::binary::SetName(binary_name);
  }

  // Init glog (only once per process; a component container may host many
  // Apollo components).
  static bool glog_initialized = false;
  if (!glog_initialized) {
    if (!google::IsGoogleLoggingInitialized()) {
      google::InitGoogleLogging(binary_name);
    }
    glog_initialized = true;
  }
  google::SetLogDestination(google::ERROR, "");
  google::SetLogDestination(google::WARNING, "");
  google::SetLogDestination(google::FATAL, "");

  // Init async logger
  logger_severity = logger::ToLogSeverity(FLAGS_minloglevel);
  original_logger = google::base::GetLogger(logger_severity);
  async_logger = new ::apollo::cyber::logger::AsyncLogger(original_logger);
  // glog takes ownership of loggers passed to SetLogger().
  google::base::SetLogger(logger_severity, async_logger);
  async_logger->Start();
}

void StopLogger() {
  if (async_logger == nullptr) {
    return;
  }
  // Hand glog its own logger back; glog then deletes (and thereby stops and
  // flushes) the AsyncLogger it owns. Deleting it here as well would free it
  // twice when glog's static destructors run at exit.
  google::base::SetLogger(logger_severity, original_logger);
  async_logger = nullptr;
}

}  // namespace

void OnShutdown(int sig) {
  (void)sig;
  if (GetState() != STATE_SHUTDOWN) {
    SetState(STATE_SHUTTING_DOWN);
  }
}

void ExitHandle() { Clear(); }

bool Init(const char* binary_name, const std::string& dag_info) {
  (void)dag_info;
  std::lock_guard<std::mutex> lg(g_mutex);
  if (GetState() != STATE_UNINITIALIZED) {
    return false;
  }

  ros2::SetDefaultSearchPaths();
  InitLogger(binary_name);

  // ROS 2 owns signal handling: SIGINT/SIGTERM shut the rclcpp context down,
  // which moves Cyber into STATE_SHUTTING_DOWN so `while (!IsShutdown())`
  // loops and WaitForShutdown() return.
  const char* argv[] = {binary_name};
  if (!ros2::Runtime::Instance()->Init(1, argv)) {
    AERROR << "failed to initialize the ROS 2 runtime";
    return false;
  }
  rclcpp::contexts::get_global_default_context()->add_pre_shutdown_callback(
      []() { OnShutdown(SIGINT); });
  // Create the process helper node now: the RMW layer (Fast DDS) creates its
  // participant -- and the static factory owning it -- with the first node.
  // The exit handler registered below then runs before that factory's static
  // destructor, so Clear() releases our nodes while DDS is still alive.
  ros2::Runtime::Instance()->AuxNode();

  // Register exit handlers
  if (!g_atexit_registered) {
    if (std::atexit(ExitHandle) != 0) {
      AERROR << "Register exit handle failed";
      return false;
    }
    AINFO << "Register exit handle succ.";
    g_atexit_registered = true;
  }
  SetState(STATE_INITIALIZED);

  auto global_data = GlobalData::Instance();
  if (global_data->IsMockTimeMode()) {
    auto node_name = kClockNode + std::to_string(getpid());
    clock_node = std::unique_ptr<Node>(new Node(node_name));
    auto cb =
        [](const std::shared_ptr<const apollo::cyber::proto::Clock>& msg) {
          if (msg->has_clock()) {
            Clock::Instance()->SetNow(Time(msg->clock()));
          }
        };
    clock_node->CreateReader<apollo::cyber::proto::Clock>(kClockChannel, cb);
    // The same mode with ROS 2 means: follow /clock (rosgraph_msgs/Clock),
    // as a node with use_sim_time does.
    ros2::SetSimTime(true);
  }
  return true;
}

void Clear() {
  std::lock_guard<std::mutex> lg(g_mutex);
  if (GetState() == STATE_SHUTDOWN || GetState() == STATE_UNINITIALIZED) {
    return;
  }
  SetState(STATE_SHUTTING_DOWN);
  clock_node.reset();
  TaskManager::CleanUp();
  ros2::Runtime::Instance()->Shutdown();
  StopLogger();
  SetState(STATE_SHUTDOWN);
}

}  // namespace cyber
}  // namespace apollo
