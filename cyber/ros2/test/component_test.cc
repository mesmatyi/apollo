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
#include <memory>
#include <thread>

#include "gtest/gtest.h"

#include "cyber/component/component.h"
#include "cyber/component/timer_component.h"
#include "cyber/cyber.h"
#include "cyber/ros2/test/test_msgs.pb.h"

namespace apollo {
namespace cyber {
namespace {

using apollo::cyber::ros2::test::Chatter;
using apollo::cyber::ros2::test::Other;

std::atomic<int> g_fused{0};
std::atomic<int> g_last_other{-1};
std::atomic<int> g_timer_ticks{0};

class FusionComponent : public Component<Chatter, Other> {
 public:
  bool Init() override { return true; }
  bool Proc(const std::shared_ptr<Chatter>& chatter,
            const std::shared_ptr<Other>& other) override {
    g_last_other = other->value();
    ++g_fused;
    return true;
  }
};

class TickComponent : public TimerComponent {
 public:
  bool Init() override { return true; }
  bool Proc() override {
    ++g_timer_ticks;
    return true;
  }
};

template <typename Pred>
bool WaitFor(Pred pred) {
  for (int i = 0; i < 500; ++i) {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return pred();
}

TEST(ComponentTest, FusionWaitsForAllChannels) {
  auto component = std::make_shared<FusionComponent>();
  proto::ComponentConfig config;
  config.set_name("fusion");
  config.add_readers()->set_channel("/test/fusion/chatter");
  config.add_readers()->set_channel("/test/fusion/other");
  ASSERT_TRUE(component->Initialize(config));

  auto node = CreateNode("fusion_driver");
  auto chatter_writer = node->CreateWriter<Chatter>("/test/fusion/chatter");
  auto other_writer = node->CreateWriter<Other>("/test/fusion/other");

  Chatter c;
  chatter_writer->Write(c);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_EQ(0, g_fused.load()) << "Proc must wait for every channel";

  Other o;
  o.set_value(7);
  other_writer->Write(o);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  chatter_writer->Write(c);
  ASSERT_TRUE(WaitFor([]() { return g_fused.load() >= 1; }));
  EXPECT_EQ(7, g_last_other.load());
  component->Shutdown();
}

TEST(ComponentTest, TimerComponentTicks) {
  auto component = std::make_shared<TickComponent>();
  proto::TimerComponentConfig config;
  config.set_name("ticker");
  config.set_interval(10);
  ASSERT_TRUE(component->Initialize(config));
  ASSERT_TRUE(WaitFor([]() { return g_timer_ticks.load() >= 5; }));
  EXPECT_EQ(10u, component->GetInterval());
  component->Shutdown();
  int ticks = g_timer_ticks.load();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_LE(g_timer_ticks.load(), ticks + 1);
}

}  // namespace
}  // namespace cyber
}  // namespace apollo
