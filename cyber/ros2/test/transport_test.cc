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

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "cyber/cyber.h"
#include "cyber/message/raw_message.h"
#include "cyber/ros2/test/test_msgs.pb.h"
#include "cyber/ros2/transport.h"
#include "cyber/service_discovery/topology_manager.h"

namespace apollo {
namespace cyber {
namespace {

using apollo::cyber::ros2::test::AddRequest;
using apollo::cyber::ros2::test::AddResponse;
using apollo::cyber::ros2::test::Chatter;

template <typename Pred>
bool WaitFor(Pred pred, std::chrono::milliseconds timeout =
                            std::chrono::milliseconds(5000)) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return pred();
}

class CyberEnv : public ::testing::Environment {
 public:
  void SetUp() override { ASSERT_TRUE(apollo::cyber::Init("cyber_ros2_test")); }
};

TEST(TransportTest, IntraProcessIsZeroCopyAndDeliveredOnce) {
  auto writer_node = CreateNode("zero_copy_writer");
  auto reader_node = CreateNode("zero_copy_reader");
  ASSERT_NE(writer_node, nullptr);
  ASSERT_NE(reader_node, nullptr);

  std::mutex mutex;
  std::vector<std::shared_ptr<Chatter>> received;
  auto reader = reader_node->CreateReader<Chatter>(
      "/test/zero_copy", [&](const std::shared_ptr<Chatter>& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(msg);
      });
  ASSERT_NE(reader, nullptr);
  auto writer = writer_node->CreateWriter<Chatter>("/test/zero_copy");
  ASSERT_NE(writer, nullptr);
  // Let ROS 2 discovery match the (ignored) local ROS subscription.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  auto msg = std::make_shared<Chatter>();
  msg->set_seq(1);
  msg->set_content("hello");
  ASSERT_TRUE(writer->Write(msg));

  ASSERT_TRUE(WaitFor([&]() {
    std::lock_guard<std::mutex> lock(mutex);
    return !received.empty();
  }));
  // Give a potential duplicate (via DDS) time to show up.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::lock_guard<std::mutex> lock(mutex);
  ASSERT_EQ(1u, received.size());
  EXPECT_EQ(msg.get(), received[0].get()) << "in-process delivery must share "
                                             "the published object";
  EXPECT_EQ("hello", received[0]->content());
}

TEST(TransportTest, HistoryObserveAndRawReader) {
  auto node = CreateNode("history_node");
  ReaderConfig config;
  config.channel_name = "/test/history";
  config.qos_profile.set_depth(3);
  config.pending_queue_size = 10;  // keep the whole burst
  auto reader = node->CreateReader<Chatter>(config);
  ASSERT_NE(reader, nullptr);

  std::atomic<int> raw_count{0};
  std::string raw_bytes;
  std::mutex raw_mutex;
  auto raw_reader = node->CreateReader<message::RawMessage>(
      "/test/history_raw",
      [&](const std::shared_ptr<message::RawMessage>& msg) {
        std::lock_guard<std::mutex> lock(raw_mutex);
        raw_bytes = msg->message;
        ++raw_count;
      });
  ASSERT_NE(raw_reader, nullptr);

  auto writer = node->CreateWriter<Chatter>("/test/history");
  auto raw_writer = node->CreateWriter<Chatter>("/test/history_raw");
  for (int i = 0; i < 5; ++i) {
    Chatter c;
    c.set_seq(i);
    writer->Write(c);
    raw_writer->Write(c);
  }
  // The raw reader keeps Cyber's default pending queue of 1: a burst may be
  // coalesced, but the newest message must arrive.
  ASSERT_TRUE(WaitFor([&]() {
    std::lock_guard<std::mutex> lock(raw_mutex);
    Chatter last;
    return reader->HasReceived() && raw_count >= 1 &&
           last.ParseFromString(raw_bytes) && last.seq() == 4;
  }));
  ASSERT_TRUE(WaitFor([&]() {
    node->Observe();
    auto latest = reader->GetLatestObserved();
    return latest && latest->seq() == 4;
  }));
  EXPECT_EQ(2u, reader->GetOldestObserved()->seq());
  int count = 0;
  for (auto it = reader->Begin(); it != reader->End(); ++it) {
    ++count;
  }
  EXPECT_EQ(3, count);

  Chatter parsed;
  {
    std::lock_guard<std::mutex> lock(raw_mutex);
    ASSERT_TRUE(parsed.ParseFromString(raw_bytes));
  }
  EXPECT_EQ(4u, parsed.seq());

  node->ClearData();
  node->Observe();
  EXPECT_TRUE(reader->Empty());
}

TEST(TransportTest, ServiceRoundTrip) {
  auto server_node = CreateNode("add_server");
  auto client_node = CreateNode("add_client");
  auto service = server_node->CreateService<AddRequest, AddResponse>(
      "/test/add", [](const std::shared_ptr<AddRequest>& req,
                      std::shared_ptr<AddResponse>& resp) {
        resp->set_sum(req->a() + req->b());
      });
  ASSERT_NE(service, nullptr);
  auto client = client_node->CreateClient<AddRequest, AddResponse>("/test/add");
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(client->WaitForService(std::chrono::seconds(5)));

  AddRequest req;
  req.set_a(40);
  req.set_b(2);
  auto resp = client->SendRequest(req, std::chrono::seconds(5));
  ASSERT_NE(resp, nullptr);
  EXPECT_EQ(42, resp->sum());

  auto future = client->AsyncSendRequest(req);
  ASSERT_EQ(std::future_status::ready,
            future.wait_for(std::chrono::seconds(5)));
  ASSERT_NE(future.get(), nullptr);
  EXPECT_EQ(42, future.get()->sum());
}

TEST(TransportTest, TimerAndAsync) {
  std::atomic<int> ticks{0};
  Timer timer(20, [&ticks]() { ++ticks; }, false);
  timer.Start();
  EXPECT_TRUE(WaitFor([&]() { return ticks >= 3; }));
  timer.Stop();

  std::atomic<int> oneshot{0};
  Timer once(10, [&oneshot]() { ++oneshot; }, true);
  once.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_EQ(1, oneshot.load());

  auto future = Async([](int a, int b) { return a * b; }, 6, 7);
  EXPECT_EQ(42, future.get());
}

TEST(TransportTest, TopologySeesWriters) {
  auto node = CreateNode("topology_node");
  auto writer = node->CreateWriter<Chatter>("/test/topology");
  ASSERT_NE(writer, nullptr);
  auto channel_manager =
      service_discovery::TopologyManager::Instance()->channel_manager();
  EXPECT_TRUE(WaitFor([&]() { return channel_manager->HasWriter("/test/topology"); }));
  std::vector<std::string> channels;
  channel_manager->GetChannelNames(&channels);
  // The graph lists the channel under its ROS 2 name: the same name unless
  // the process was started with a remapping rule for it.
  EXPECT_NE(std::find(channels.begin(), channels.end(),
                      ros2::ResolveChannel("/test/topology")),
            channels.end());
  std::string type;
  channel_manager->GetMsgType("/test/topology", &type);
  EXPECT_EQ("apollo.cyber.ros2.test.Chatter", type);
  EXPECT_TRUE(service_discovery::TopologyManager::Instance()
                  ->node_manager()
                  ->HasNode("topology_node"));
}

}  // namespace
}  // namespace cyber
}  // namespace apollo

int main(int argc, char** argv) {
  // Channels that keep the protobuf wire format (see native_types_test.cc).
  setenv("APOLLO_ROS_PROTO_CHANNELS", "^/test/protowire/", 1);
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new apollo::cyber::CyberEnv());
  int ret = RUN_ALL_TESTS();
  apollo::cyber::Clear();
  return ret;
}
