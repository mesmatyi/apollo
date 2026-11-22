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

#include "cyber/ros2/parameters.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <utility>

#include "gflags/gflags.h"
#include "rclcpp/rclcpp.hpp"
#include "rosgraph_msgs/msg/clock.hpp"

#include "cyber/common/log.h"
#include "cyber/time/clock.h"

namespace apollo {
namespace cyber {
namespace ros2 {

const char kFlagParameterPrefix[] = "gflags.";

// Defined in rosout.cc.
void StartRosout(const std::shared_ptr<rclcpp::Node>& node);
void StopRosout();

namespace {

constexpr size_t kPrefixLength = sizeof(kFlagParameterPrefix) - 1;

/// gflags accepts every ROS 2 command line argument as "unknown but fine",
/// so a binary that parses its command line with gflags can be started with
/// --ros-args (as launch_ros does for every node).
struct AllowRosArguments {
  AllowRosArguments() {
    std::string current;
    google::GetCommandLineOption("undefok", &current);
    const std::string names =
        "ros-args,r,remap,p,param,params-file,log-level,log-config-file,"
        "log-file-name,enable-rosout-logs,disable-rosout-logs,"
        "enable-stdout-logs,disable-stdout-logs,enable-external-lib-logs,"
        "disable-external-lib-logs,e,enclave";
    google::SetCommandLineOption(
        "undefok", current.empty() ? names.c_str()
                                   : (current + "," + names).c_str());
  }
};
const AllowRosArguments allow_ros_arguments;

bool IsFlagParameter(const std::string& name) {
  return name.compare(0, kPrefixLength, kFlagParameterPrefix) == 0 &&
         name.size() > kPrefixLength;
}

std::string JoinArray(const std::vector<std::string>& items) {
  std::string out;
  for (const auto& item : items) {
    if (!out.empty()) {
      out += ",";
    }
    out += item;
  }
  return out;
}

template <typename T>
std::string JoinNumbers(const std::vector<T>& items) {
  std::vector<std::string> strings;
  strings.reserve(items.size());
  for (const auto& item : items) {
    strings.push_back(std::to_string(item));
  }
  return JoinArray(strings);
}

std::string DoubleToString(double value) {
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "%.17g", value);
  return buffer;
}

/// The command line spelling of a parameter value. Arrays become the comma
/// separated lists Apollo uses for list flags.
bool ToFlagString(const rclcpp::ParameterValue& value, std::string* out) {
  switch (value.get_type()) {
    case rclcpp::ParameterType::PARAMETER_BOOL:
      *out = value.get<bool>() ? "true" : "false";
      return true;
    case rclcpp::ParameterType::PARAMETER_INTEGER:
      *out = std::to_string(value.get<int64_t>());
      return true;
    case rclcpp::ParameterType::PARAMETER_DOUBLE:
      *out = DoubleToString(value.get<double>());
      return true;
    case rclcpp::ParameterType::PARAMETER_STRING:
      *out = value.get<std::string>();
      return true;
    case rclcpp::ParameterType::PARAMETER_STRING_ARRAY:
      *out = JoinArray(value.get<std::vector<std::string>>());
      return true;
    case rclcpp::ParameterType::PARAMETER_INTEGER_ARRAY:
      *out = JoinNumbers(value.get<std::vector<int64_t>>());
      return true;
    case rclcpp::ParameterType::PARAMETER_DOUBLE_ARRAY: {
      std::vector<std::string> strings;
      for (double d : value.get<std::vector<double>>()) {
        strings.push_back(DoubleToString(d));
      }
      *out = JoinArray(strings);
      return true;
    }
    default:
      return false;
  }
}

/// The parameter value of a flag, typed like the flag.
rclcpp::ParameterValue ToParameterValue(const google::CommandLineFlagInfo& info) {
  const std::string& v = info.current_value;
  if (info.type == "bool") {
    return rclcpp::ParameterValue(v == "true");
  }
  if (info.type == "int32" || info.type == "int64" || info.type == "uint32") {
    return rclcpp::ParameterValue(
        static_cast<int64_t>(std::strtoll(v.c_str(), nullptr, 10)));
  }
  if (info.type == "uint64") {
    const unsigned long long u = std::strtoull(v.c_str(), nullptr, 10);  // NOLINT
    const auto max =
        static_cast<unsigned long long>(std::numeric_limits<int64_t>::max());  // NOLINT
    return rclcpp::ParameterValue(static_cast<int64_t>(std::min(u, max)));
  }
  if (info.type == "double") {
    return rclcpp::ParameterValue(std::strtod(v.c_str(), nullptr));
  }
  return rclcpp::ParameterValue(v);
}

struct State {
  std::mutex mutex;
  /// gflags given as ROS 2 parameters, in the order they first appeared.
  std::vector<std::pair<std::string, std::string>> overrides;
  std::set<std::string> warned;

  /// Nodes whose use_sim_time is followed.
  struct Watched {
    std::weak_ptr<rclcpp::Node> node;
    rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr on_set;
    rclcpp::node_interfaces::PostSetParametersCallbackHandle::SharedPtr post_set;
  };
  std::vector<Watched> watched;

  /// Serializes SyncFlagParameters(); never taken inside a parameter
  /// callback, unlike `mutex`.
  std::mutex sync_mutex;
  std::shared_ptr<rclcpp::Node> process_node;
  rclcpp::TimerBase::SharedPtr sync_timer;
  rclcpp::Subscription<rosgraph_msgs::msg::Clock>::SharedPtr clock_subscription;
  bool flags_exposed = false;
  int ticks = 0;
};

State& GetState() {
  static State* state = new State();
  return *state;
}

std::atomic<bool> g_flags_dirty{false};
std::atomic<bool> g_sim_time{false};
/// True on the thread that copies flag values into parameters, so the
/// parameter callback does not write them back.
thread_local bool t_syncing = false;

bool FlagParametersEnabled() {
  const char* env = std::getenv("APOLLO_ROS_FLAG_PARAMETERS");
  return env == nullptr || (std::string(env) != "0" &&
                            std::string(env) != "false" &&
                            std::string(env) != "off");
}

/// `ros2 param set` on the process node: validate and apply to the flags.
rcl_interfaces::msg::SetParametersResult OnSetParameters(
    const std::vector<rclcpp::Parameter>& parameters) {
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;
  if (t_syncing) {
    return result;
  }
  // (flag, previous value) of what was applied, to undo a rejected batch.
  std::vector<std::pair<std::string, std::string>> applied;
  for (const auto& parameter : parameters) {
    if (!IsFlagParameter(parameter.get_name())) {
      continue;
    }
    const std::string flag = parameter.get_name().substr(kPrefixLength);
    std::string previous;
    std::string value;
    if (!google::GetCommandLineOption(flag.c_str(), &previous)) {
      result.successful = false;
      result.reason = "no gflag named '" + flag + "' in this process";
    } else if (!ToFlagString(parameter.get_parameter_value(), &value)) {
      result.successful = false;
      result.reason = "unsupported parameter type for gflag '" + flag + "'";
    } else if (google::SetCommandLineOption(flag.c_str(), value.c_str())
                   .empty()) {
      result.successful = false;
      result.reason = "'" + value + "' is not a valid value for gflag '" +
                      flag + "'";
    } else {
      applied.emplace_back(flag, previous);
      continue;
    }
    break;
  }
  if (!result.successful) {
    for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
      google::SetCommandLineOption(it->first.c_str(), it->second.c_str());
    }
  } else if (!applied.empty()) {
    g_flags_dirty.store(true);
  }
  return result;
}

void OnClock(const rosgraph_msgs::msg::Clock& msg) {
  const uint64_t nanoseconds =
      static_cast<uint64_t>(msg.clock.sec) * 1000000000ull + msg.clock.nanosec;
  Clock::SetNow(Time(nanoseconds));
}

/// Creates or drops the /clock subscription to match g_sim_time. Caller
/// holds the state mutex.
void UpdateClockSubscription(State* state) {
  if (!state->process_node) {
    return;
  }
  if (g_sim_time.load() && !state->clock_subscription) {
    rclcpp::SubscriptionOptions options;
    options.use_intra_process_comm = rclcpp::IntraProcessSetting::Disable;
    state->clock_subscription =
        state->process_node->create_subscription<rosgraph_msgs::msg::Clock>(
            "/clock", rclcpp::ClockQoS(), &OnClock, options);
  } else if (!g_sim_time.load()) {
    state->clock_subscription.reset();
  }
}

}  // namespace

void ApplyFlagOverrides() {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  for (const auto& item : state.overrides) {
    if (google::SetCommandLineOption(item.first.c_str(), item.second.c_str())
            .empty()) {
      // The flag may belong to a library that is loaded later; it is applied
      // then. Warn once so typos do not go unnoticed.
      if (state.warned.insert(item.first).second) {
        AWARN << "parameter " << kFlagParameterPrefix << item.first << "="
              << item.second
              << " does not match a gflag of this process (yet) or is not a "
                 "valid value for it";
      }
    } else {
      state.warned.erase(item.first);
    }
  }
  g_flags_dirty.store(true);
}

void AttachNode(const std::shared_ptr<rclcpp::Node>& node) {
  if (!node) {
    return;
  }
  State& state = GetState();
  bool has_flags = false;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto& overrides =
        node->get_node_parameters_interface()->get_parameter_overrides();
    for (const auto& item : overrides) {
      if (!IsFlagParameter(item.first)) {
        continue;
      }
      std::string value;
      if (!ToFlagString(item.second, &value)) {
        AWARN << "parameter " << item.first << " has a type gflags cannot take";
        continue;
      }
      const std::string flag = item.first.substr(kPrefixLength);
      auto it = std::find_if(
          state.overrides.begin(), state.overrides.end(),
          [&flag](const std::pair<std::string, std::string>& o) {
            return o.first == flag;
          });
      if (it == state.overrides.end()) {
        state.overrides.emplace_back(flag, value);
      } else {
        it->second = value;
      }
      has_flags = true;
    }
  }
  if (has_flags) {
    ApplyFlagOverrides();
    // Show the gflags this node was given (`ros2 param list <node>`); a
    // component container does not declare parameters it does not know.
    for (const auto& item :
         node->get_node_parameters_interface()->get_parameter_overrides()) {
      if (IsFlagParameter(item.first) && !node->has_parameter(item.first)) {
        try {
          node->declare_parameter(item.first, item.second);
        } catch (const std::exception& e) {
          ADEBUG << "declare " << item.first << ": " << e.what();
        }
      }
    }
  }

  // Simulation time is a property of the process: Cyber has one Clock.
  bool use_sim_time = false;
  if (node->has_parameter("use_sim_time")) {
    node->get_parameter("use_sim_time", use_sim_time);
  }
  if (use_sim_time) {
    SetSimTime(true);
  }
  // `ros2 param set <node> gflags.<name>` changes the flag, whichever node
  // the parameter was declared on.
  auto on_set = node->add_on_set_parameters_callback(&OnSetParameters);
  auto post_set = node->add_post_set_parameters_callback(
      [](const std::vector<rclcpp::Parameter>& parameters) {
        for (const auto& parameter : parameters) {
          if (parameter.get_name() == "use_sim_time" &&
              parameter.get_type() == rclcpp::ParameterType::PARAMETER_BOOL) {
            SetSimTime(parameter.as_bool());
          }
        }
      });
  std::lock_guard<std::mutex> lock(state.mutex);
  state.watched.erase(
      std::remove_if(state.watched.begin(), state.watched.end(),
                     [](const State::Watched& w) { return w.node.expired(); }),
      state.watched.end());
  state.watched.push_back({node, on_set, post_set});
}

void SyncFlagParameters() {
  State& state = GetState();
  std::lock_guard<std::mutex> sync_lock(state.sync_mutex);
  std::shared_ptr<rclcpp::Node> node;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.flags_exposed) {
      return;
    }
    node = state.process_node;
  }
  if (!node) {
    return;
  }
  std::vector<google::CommandLineFlagInfo> flags;
  google::GetAllFlags(&flags);
  t_syncing = true;
  for (const auto& flag : flags) {
    if (flag.name == "flagfile" || flag.name == "fromenv" ||
        flag.name == "tryfromenv" || flag.name == "undefok") {
      continue;   // gflags' own bookkeeping
    }
    const std::string name = kFlagParameterPrefix + flag.name;
    const rclcpp::ParameterValue value = ToParameterValue(flag);
    try {
      if (!node->has_parameter(name)) {
        rcl_interfaces::msg::ParameterDescriptor descriptor;
        descriptor.description = flag.description;
        descriptor.additional_constraints = "gflag defined in " + flag.filename;
        node->declare_parameter(name, value, descriptor, true);
      } else if (node->get_parameter(name).get_parameter_value() != value) {
        node->set_parameter(rclcpp::Parameter(name, value));
      }
    } catch (const std::exception& e) {
      ADEBUG << "parameter " << name << ": " << e.what();
    }
  }
  t_syncing = false;
}

void NotifyFlagsChanged() { g_flags_dirty.store(true); }

void AttachProcessNode(const std::shared_ptr<rclcpp::Node>& node) {
  if (!node) {
    return;
  }
  State& state = GetState();
  const bool expose = FlagParametersEnabled();
  rclcpp::TimerBase::SharedPtr timer;
  if (expose) {
    // Flags appear when libraries are loaded and change when flag files are
    // read or code assigns them; catch up shortly after a known change and
    // every ten seconds otherwise.
    timer = node->create_wall_timer(std::chrono::seconds(1), []() {
      State& s = GetState();
      bool due = false;
      {
        std::lock_guard<std::mutex> lock(s.mutex);
        due = ++s.ticks % 10 == 0;
      }
      if (g_flags_dirty.exchange(false) || due) {
        SyncFlagParameters();
      }
    });
  }
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.process_node = node;
    state.flags_exposed = expose;
    state.sync_timer = timer;
    UpdateClockSubscription(&state);
  }
  SyncFlagParameters();
  StartRosout(node);
}

void DetachProcessNode() {
  StopRosout();
  State& state = GetState();
  std::lock_guard<std::mutex> sync_lock(state.sync_mutex);
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.sync_timer) {
    state.sync_timer->cancel();
  }
  state.sync_timer.reset();
  state.clock_subscription.reset();
  state.process_node.reset();
  state.watched.clear();
}

void SetSimTime(bool enabled) {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (g_sim_time.exchange(enabled) != enabled) {
    Clock::SetMode(enabled ? proto::MODE_MOCK : proto::MODE_CYBER);
    AINFO << "Cyber clock follows " << (enabled ? "/clock" : "the system clock");
  }
  UpdateClockSubscription(&state);
}

bool SimTimeEnabled() { return g_sim_time.load(); }

void SplitRosArguments(int argc, const char* const* argv,
                       std::vector<std::string>* program_args,
                       std::vector<std::string>* ros_args) {
  bool in_ros_args = false;
  for (int i = 0; i < argc && argv != nullptr; ++i) {
    const std::string arg = argv[i] == nullptr ? "" : argv[i];
    if (!in_ros_args && arg == "--ros-args") {
      in_ros_args = true;
      if (ros_args != nullptr) {
        ros_args->push_back(arg);
      }
      continue;
    }
    if (in_ros_args) {
      if (arg == "--") {
        in_ros_args = false;
        continue;
      }
      if (ros_args != nullptr) {
        ros_args->push_back(arg);
      }
      continue;
    }
    if (program_args != nullptr) {
      program_args->push_back(arg);
    }
  }
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
