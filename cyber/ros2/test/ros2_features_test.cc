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

/**
 * Apollo nodes as ordinary ROS 2 nodes: gflags as parameters, simulation
 * time from /clock, glog messages on /rosout, parameter services. Everything
 * is exercised from a plain rclcpp node, the way `ros2 param`, a simulator
 * or rqt_console would.
 */

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "rcl_interfaces/msg/log.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rosgraph_msgs/msg/clock.hpp"

#include "cyber/cyber.h"
#include "cyber/ros2/parameters.h"
#include "cyber/ros2/transport.h"
#include "cyber/time/clock.h"

DEFINE_int32(cyber_ros2_test_count, 7, "A counter of the Cyber ROS 2 tests");
DEFINE_string(cyber_ros2_test_name, "apollo", "A name of the Cyber ROS 2 tests");
DEFINE_double(cyber_ros2_test_ratio, 0.5, "A ratio of the Cyber ROS 2 tests");
DEFINE_bool(cyber_ros2_test_switch, false, "A switch of the Cyber ROS 2 tests");

namespace apollo {
namespace cyber {
namespace {

using namespace std::chrono_literals;  // NOLINT

template <typename Pred>
bool WaitFor(Pred pred, std::chrono::milliseconds timeout = 5000ms) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(10ms);
  }
  return pred();
}

/// A plain rclcpp node spun on its own thread.
class Peer {
 public:
  explicit Peer(const std::string& name)
      : node_(std::make_shared<rclcpp::Node>(name)) {
    executor_.add_node(node_);
    thread_ = std::thread([this]() {
      while (!stop_.load() && rclcpp::ok()) {
        executor_.spin_once(20ms);
      }
    });
  }
  ~Peer() {
    stop_.store(true);
    thread_.join();
    executor_.remove_node(node_);
  }
  const rclcpp::Node::SharedPtr& node() const { return node_; }

 private:
  rclcpp::Node::SharedPtr node_;
  rclcpp::executors::SingleThreadedExecutor executor_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};

/// What `ros2 param` does, against the node called `target`.
class ParameterClient {
 public:
  ParameterClient(const Peer& peer, const std::string& target)
      : client_(std::make_shared<rclcpp::AsyncParametersClient>(peer.node(),
                                                                target)) {}

  bool Ready() { return client_->wait_for_service(5s); }

  bool Get(const std::string& name, rclcpp::Parameter* out) {
    auto future = client_->get_parameters({name});
    if (future.wait_for(5s) != std::future_status::ready) {
      return false;
    }
    auto values = future.get();
    if (values.size() != 1 ||
        values[0].get_type() == rclcpp::ParameterType::PARAMETER_NOT_SET) {
      return false;
    }
    *out = values[0];
    return true;
  }

  /// Returns the result of setting one parameter ("timeout" on no answer).
  rcl_interfaces::msg::SetParametersResult Set(const rclcpp::Parameter& p) {
    rcl_interfaces::msg::SetParametersResult failed;
    failed.successful = false;
    failed.reason = "timeout";
    auto future = client_->set_parameters({p});
    if (future.wait_for(5s) != std::future_status::ready) {
      return failed;
    }
    auto results = future.get();
    return results.size() == 1 ? results[0] : failed;
  }

  std::string Describe(const std::string& name) {
    auto future = client_->describe_parameters({name});
    if (future.wait_for(5s) != std::future_status::ready) {
      return "";
    }
    auto descriptors = future.get();
    return descriptors.size() == 1 ? descriptors[0].description : "";
  }

  std::vector<std::string> List(const std::string& prefix) {
    auto future = client_->list_parameters({prefix}, 10);
    if (future.wait_for(5s) != std::future_status::ready) {
      return {};
    }
    return future.get().names;
  }

 private:
  std::shared_ptr<rclcpp::AsyncParametersClient> client_;
};

std::string ProcessNodeName() {
  return ros2::AuxNodeHandle()->ros_node()->get_fully_qualified_name();
}

TEST(Ros2FeaturesTest, SplitRosArguments) {
  const char* argv[] = {"mainboard", "-d",  "x.dag", "--ros-args", "-r",
                        "a:=b",      "--",  "extra", "--ros-args", "-p",
                        "use_sim_time:=true"};
  std::vector<std::string> program;
  std::vector<std::string> ros;
  ros2::SplitRosArguments(11, argv, &program, &ros);
  EXPECT_EQ((std::vector<std::string>{"mainboard", "-d", "x.dag", "extra"}),
            program);
  EXPECT_EQ((std::vector<std::string>{"--ros-args", "-r", "a:=b", "--ros-args",
                                      "-p", "use_sim_time:=true"}),
            ros);
}

/// Every gflag is a parameter of the process node: list, describe, get, set.
TEST(Ros2FeaturesTest, FlagsAreParametersOfTheProcessNode) {
  Peer peer("flag_parameter_peer");
  ParameterClient client(peer, ProcessNodeName());
  ASSERT_TRUE(client.Ready());

  const auto names = client.List("gflags");
  EXPECT_GT(names.size(), 20u);
  EXPECT_NE(std::find(names.begin(), names.end(), "gflags.cyber_ros2_test_count"),
            names.end());
  EXPECT_EQ("A counter of the Cyber ROS 2 tests",
            client.Describe("gflags.cyber_ros2_test_count"));

  rclcpp::Parameter value;
  ASSERT_TRUE(client.Get("gflags.cyber_ros2_test_count", &value));
  EXPECT_EQ(7, value.as_int());
  ASSERT_TRUE(client.Get("gflags.cyber_ros2_test_name", &value));
  EXPECT_EQ("apollo", value.as_string());
  ASSERT_TRUE(client.Get("gflags.cyber_ros2_test_ratio", &value));
  EXPECT_DOUBLE_EQ(0.5, value.as_double());
  ASSERT_TRUE(client.Get("gflags.cyber_ros2_test_switch", &value));
  EXPECT_FALSE(value.as_bool());

  // ros2 param set changes the flag.
  EXPECT_TRUE(
      client.Set(rclcpp::Parameter("gflags.cyber_ros2_test_count", 42))
          .successful);
  EXPECT_EQ(42, FLAGS_cyber_ros2_test_count);
  EXPECT_TRUE(
      client.Set(rclcpp::Parameter("gflags.cyber_ros2_test_name", "ros"))
          .successful);
  EXPECT_EQ("ros", FLAGS_cyber_ros2_test_name);
  EXPECT_TRUE(
      client.Set(rclcpp::Parameter("gflags.cyber_ros2_test_ratio", 0.25))
          .successful);
  EXPECT_DOUBLE_EQ(0.25, FLAGS_cyber_ros2_test_ratio);
  EXPECT_TRUE(
      client.Set(rclcpp::Parameter("gflags.cyber_ros2_test_switch", true))
          .successful);
  EXPECT_TRUE(FLAGS_cyber_ros2_test_switch);

  // A value of the wrong type and an unknown flag are refused.
  EXPECT_FALSE(
      client.Set(rclcpp::Parameter("gflags.cyber_ros2_test_count", "many"))
          .successful);
  EXPECT_EQ(42, FLAGS_cyber_ros2_test_count);
  EXPECT_FALSE(client.Set(rclcpp::Parameter("gflags.no_such_flag", 1)).successful);

  // A flag changed by the program shows up as the parameter value.
  FLAGS_cyber_ros2_test_count = 5;
  ros2::SyncFlagParameters();
  ASSERT_TRUE(client.Get("gflags.cyber_ros2_test_count", &value));
  EXPECT_EQ(5, value.as_int());

  FLAGS_cyber_ros2_test_count = 7;
  FLAGS_cyber_ros2_test_name = "apollo";
  FLAGS_cyber_ros2_test_ratio = 0.5;
  FLAGS_cyber_ros2_test_switch = false;
  ros2::SyncFlagParameters();
}

/// gflags given to a node as parameters (launch file, --ros-args -p, YAML)
/// set the flags of the process and win over flag files.
TEST(Ros2FeaturesTest, FlagsFromNodeParameters) {
  rclcpp::NodeOptions options;
  options.parameter_overrides({{"gflags.cyber_ros2_test_name", "from_launch"},
                               {"gflags.cyber_ros2_test_count", 11},
                               {"gflags.cyber_ros2_test_switch", true}});
  // As a component container hands a node to an Apollo component.
  auto ros_node = std::make_shared<rclcpp::Node>("flag_override_node", options);
  Peer peer("flag_override_peer");
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(ros_node);
  std::atomic<bool> stop{false};
  std::thread spinner([&]() {
    while (!stop.load()) {
      executor.spin_once(20ms);
    }
  });

  auto handle = ros2::NodeHandle::Wrap(ros_node);
  EXPECT_EQ("from_launch", FLAGS_cyber_ros2_test_name);
  EXPECT_EQ(11, FLAGS_cyber_ros2_test_count);
  EXPECT_TRUE(FLAGS_cyber_ros2_test_switch);

  // A flag file read afterwards does not take the parameters' place.
  google::SetCommandLineOption("cyber_ros2_test_name", "from_flag_file");
  ros2::ApplyFlagOverrides();
  EXPECT_EQ("from_launch", FLAGS_cyber_ros2_test_name);

  // The node shows the flags it was given, and setting one changes the flag.
  ParameterClient client(peer, "/flag_override_node");
  ASSERT_TRUE(client.Ready());
  rclcpp::Parameter value;
  ASSERT_TRUE(client.Get("gflags.cyber_ros2_test_count", &value));
  EXPECT_EQ(11, value.as_int());
  EXPECT_TRUE(
      client.Set(rclcpp::Parameter("gflags.cyber_ros2_test_count", 12))
          .successful);
  EXPECT_EQ(12, FLAGS_cyber_ros2_test_count);

  stop.store(true);
  spinner.join();
  executor.remove_node(ros_node);
  FLAGS_cyber_ros2_test_count = 7;
  FLAGS_cyber_ros2_test_name = "apollo";
  FLAGS_cyber_ros2_test_switch = false;
}

/// Nodes made with the Cyber API have the standard parameter services.
TEST(Ros2FeaturesTest, CyberNodesHaveParameterServices) {
  auto node = CreateNode("ordinary_ros_node");
  ASSERT_NE(node, nullptr);
  Peer peer("ordinary_peer");
  ParameterClient client(peer, "/ordinary_ros_node");
  ASSERT_TRUE(client.Ready());
  rclcpp::Parameter value;
  ASSERT_TRUE(client.Get("use_sim_time", &value));
  EXPECT_FALSE(value.as_bool());
  const auto services = peer.node()->get_service_names_and_types();
  for (const char* name : {"list_parameters", "get_parameters",
                           "set_parameters", "describe_parameters"}) {
    EXPECT_TRUE(services.count(std::string("/ordinary_ros_node/") + name))
        << name;
  }
}

/// use_sim_time on an Apollo node makes Cyber's Clock follow /clock.
TEST(Ros2FeaturesTest, SimTimeFollowsClock) {
  auto node = CreateNode("sim_time_node");
  ASSERT_NE(node, nullptr);
  Peer peer("clock_peer");
  auto clock_publisher = peer.node()->create_publisher<rosgraph_msgs::msg::Clock>(
      "/clock", rclcpp::ClockQoS());
  ParameterClient client(peer, "/sim_time_node");
  ASSERT_TRUE(client.Ready());

  EXPECT_FALSE(ros2::SimTimeEnabled());
  EXPECT_GT(Clock::NowInSeconds(), 1.0e9);

  ASSERT_TRUE(client.Set(rclcpp::Parameter("use_sim_time", true)).successful);
  struct Restore {
    ~Restore() { ros2::SetSimTime(false); }
  } restore;
  ASSERT_TRUE(WaitFor([]() { return ros2::SimTimeEnabled(); }));
  ASSERT_TRUE(
      WaitFor([&]() { return clock_publisher->get_subscription_count() >= 1; }));

  auto publish = [&](int32_t sec, uint32_t nanosec) {
    rosgraph_msgs::msg::Clock msg;
    msg.clock.sec = sec;
    msg.clock.nanosec = nanosec;
    clock_publisher->publish(msg);
  };
  // The clock subscription is best effort: repeat until it arrives.
  EXPECT_TRUE(WaitFor([&]() {
    publish(1234, 500000000);
    return Clock::Now().ToNanosecond() == 1234500000000ull;
  }));
  EXPECT_DOUBLE_EQ(1234.5, Clock::NowInSeconds());
  EXPECT_TRUE(WaitFor([&]() {
    publish(1300, 250000000);
    return Clock::Now().ToNanosecond() == 1300250000000ull;
  }));

  ASSERT_TRUE(client.Set(rclcpp::Parameter("use_sim_time", false)).successful);
  ASSERT_TRUE(WaitFor([]() { return !ros2::SimTimeEnabled(); }));
  EXPECT_GT(Clock::NowInSeconds(), 1.0e9);
}

/// AWARN / AERROR of Apollo modules arrive on /rosout.
TEST(Ros2FeaturesTest, GlogGoesToRosout) {
  Peer peer("rosout_peer");
  std::mutex mutex;
  std::vector<rcl_interfaces::msg::Log> logs;
  auto subscription = peer.node()->create_subscription<rcl_interfaces::msg::Log>(
      "/rosout", rclcpp::RosoutQoS(),
      [&](rcl_interfaces::msg::Log::ConstSharedPtr m) {
        std::lock_guard<std::mutex> lock(mutex);
        logs.push_back(*m);
      });
  auto find = [&](const std::string& text, rcl_interfaces::msg::Log* out) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& log : logs) {
      if (log.msg.find(text) != std::string::npos) {
        if (out != nullptr) {
          *out = log;
        }
        return true;
      }
    }
    return false;
  };
  ASSERT_TRUE(WaitFor([&]() {
    return peer.node()->count_publishers("/rosout") >= 1 &&
           subscription->get_publisher_count() >= 1;
  }));

  AINFO << "rosout info marker 7c1e";
  rcl_interfaces::msg::Log warning;
  EXPECT_TRUE(WaitFor([&]() {
    AWARN << "rosout warning marker 91bd";
    return find("rosout warning marker 91bd", &warning);
  }));
  EXPECT_EQ(rcl_interfaces::msg::Log::WARN, warning.level);
  EXPECT_FALSE(warning.name.empty());
  EXPECT_NE(std::string::npos, warning.file.find("ros2_features_test.cc"));
  EXPECT_GT(warning.line, 0u);
  // The "[module]" prefix of Cyber's log lines became the logger name.
  EXPECT_NE('[', warning.msg.front());

  rcl_interfaces::msg::Log error;
  EXPECT_TRUE(WaitFor([&]() {
    AERROR << "rosout error marker 44aa";
    return find("rosout error marker 44aa", &error);
  }));
  EXPECT_EQ(rcl_interfaces::msg::Log::ERROR, error.level);
  // INFO is below the default threshold (APOLLO_ROSOUT_LEVEL).
  EXPECT_FALSE(find("rosout info marker 7c1e", nullptr));
}

}  // namespace
}  // namespace cyber
}  // namespace apollo
