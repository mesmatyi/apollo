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
 * Native ROS 2 types: the protobuf <-> ROS 2 conversion of every generated
 * interface, and the Cyber transport talking to plain rclcpp publishers,
 * subscriptions, services and clients (which know nothing about Apollo).
 */

#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "apollo_msgs/msg/cyber_examples_proto_chatter.hpp"
#include "apollo_msgs/msg/perception_perception_obstacle.hpp"
#include "apollo_msgs/msg/planning_main_decision.hpp"
#include "apollo_msgs/msg/proto_message.hpp"
#include "apollo_msgs/srv/cyber_examples_proto_driver.hpp"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/util/field_comparator.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "rcl_interfaces/msg/log.hpp"
#include "rclcpp/rclcpp.hpp"

#include "cyber/cyber.h"
#include "cyber/examples/proto/examples.pb.h"
#include "cyber/message/raw_message.h"
#include "cyber/ros2/native_types.h"
#include "cyber/ros2/test/test_msgs.pb.h"
#include "cyber/ros2/transport.h"
#include "cyber/service_discovery/topology_manager.h"

namespace apollo {
namespace cyber {
namespace {

using apollo::cyber::examples::proto::Chatter;
using apollo::cyber::examples::proto::Driver;
using apollo::cyber::ros2::MessageConverter;
using apollo::cyber::ros2::NativeTypes;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Message;
using google::protobuf::Reflection;
using RosChatter = apollo_msgs::msg::CyberExamplesProtoChatter;
using RosDriver = apollo_msgs::srv::CyberExamplesProtoDriver;
using RosProtoMessage = apollo_msgs::msg::ProtoMessage;

constexpr char kChatterType[] = "apollo_msgs/msg/CyberExamplesProtoChatter";
constexpr char kDriverSrvType[] = "apollo_msgs/srv/CyberExamplesProtoDriver";
constexpr char kProtoMessageType[] = "apollo_msgs/msg/ProtoMessage";

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

// ---------------------------------------------------------------------------
// Conversion
// ---------------------------------------------------------------------------

/// Fills a message with random content: random presence of optional fields,
/// explicit defaults, extreme values, at most one member per oneof.
class RandomFiller {
 public:
  explicit RandomFiller(uint64_t seed) : rng_(seed) {}

  void Fill(Message* m, int depth = 0) {
    const Descriptor* d = m->GetDescriptor();
    const Reflection* r = m->GetReflection();
    --budget_;
    const bool leaf = depth >= 5 || budget_ <= 0;
    std::set<const FieldDescriptor*> skipped;
    for (int i = 0; i < d->real_oneof_decl_count(); ++i) {
      const auto* oneof = d->oneof_decl(i);
      const int keep = Int(-1, oneof->field_count() - 1);
      for (int k = 0; k < oneof->field_count(); ++k) {
        if (k != keep) {
          skipped.insert(oneof->field(k));
        }
      }
    }
    for (int i = 0; i < d->field_count(); ++i) {
      const FieldDescriptor* f = d->field(i);
      if (skipped.count(f)) {
        continue;
      }
      const bool is_message =
          f->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE;
      if (f->is_map()) {
        FillMap(m, f, leaf, depth);
        continue;
      }
      if (f->is_repeated()) {
        const int n = (leaf && is_message) ? 0 : Int(0, 3);
        for (int k = 0; k < n; ++k) {
          if (is_message) {
            Fill(r->AddMessage(m, f), depth + 1);
          } else {
            SetScalar(m, f, true);
          }
        }
        continue;
      }
      if (!f->is_required() && (Chance(0.4) || (leaf && is_message))) {
        continue;
      }
      if (is_message) {
        Fill(r->MutableMessage(m, f), depth + 1);
      } else {
        SetScalar(m, f, false);
      }
    }
  }

 private:
  int Int(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(rng_);
  }
  bool Chance(double p) {
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < p;
  }

  void FillMap(Message* m, const FieldDescriptor* f, bool leaf, int depth) {
    const Reflection* r = m->GetReflection();
    const FieldDescriptor* key = f->message_type()->map_key();
    const FieldDescriptor* value = f->message_type()->map_value();
    int n = leaf ? 0 : Int(0, 3);
    if (key->cpp_type() == FieldDescriptor::CPPTYPE_BOOL) {
      n = std::min(n, 2);
    }
    for (int k = 0; k < n; ++k) {
      Message* entry = r->AddMessage(m, f);
      const Reflection* er = entry->GetReflection();
      switch (key->cpp_type()) {   // unique keys
        case FieldDescriptor::CPPTYPE_STRING:
          er->SetString(entry, key, "key" + std::to_string(k));
          break;
        case FieldDescriptor::CPPTYPE_INT32:
          er->SetInt32(entry, key, k - 1);
          break;
        case FieldDescriptor::CPPTYPE_INT64:
          er->SetInt64(entry, key, k - 1);
          break;
        case FieldDescriptor::CPPTYPE_UINT32:
          er->SetUInt32(entry, key, static_cast<uint32_t>(k));
          break;
        case FieldDescriptor::CPPTYPE_UINT64:
          er->SetUInt64(entry, key, static_cast<uint64_t>(k));
          break;
        case FieldDescriptor::CPPTYPE_BOOL:
          er->SetBool(entry, key, k == 0);
          break;
        default:
          break;
      }
      if (value->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
        Fill(er->MutableMessage(entry, value), depth + 1);
      } else {
        SetScalar(entry, value, false);
      }
    }
  }

  double RandomDouble() {
    switch (Int(0, 9)) {
      case 0:
        return 0.0;
      case 1:
        return std::numeric_limits<double>::infinity();
      case 2:
        return std::numeric_limits<double>::quiet_NaN();
      case 3:
        return -std::numeric_limits<double>::max();
      default:
        return std::uniform_real_distribution<double>(-1e6, 1e6)(rng_);
    }
  }

  template <typename T>
  T RandomInt() {
    switch (Int(0, 5)) {
      case 0:
        return 0;
      case 1:
        return std::numeric_limits<T>::max();
      case 2:
        return std::numeric_limits<T>::min();
      default:
        return static_cast<T>(std::uniform_int_distribution<int64_t>(
            std::numeric_limits<int32_t>::min(),
            std::numeric_limits<int32_t>::max())(rng_));
    }
  }

  std::string RandomString(bool bytes) {
    static const char* const kText[] = {"", "apollo", "lane_1_-1", "a b\tc",
                                        "\xe8\xb7\xaf\xe5\xbe\x84", "x"};
    if (!bytes) {
      return kText[Int(0, 5)];
    }
    std::string s(static_cast<size_t>(Int(0, 12)), '\0');
    for (auto& c : s) {
      c = static_cast<char>(Int(0, 255));
    }
    return s;
  }

  /// Sets (or, for a repeated field, appends) a random scalar; one time in
  /// five a singular field is set to its default value explicitly.
  void SetScalar(Message* m, const FieldDescriptor* f, bool repeated) {
    const Reflection* r = m->GetReflection();
    const bool use_default = !repeated && Chance(0.2);
    switch (f->cpp_type()) {
      case FieldDescriptor::CPPTYPE_DOUBLE: {
        const double v = use_default ? f->default_value_double() : RandomDouble();
        repeated ? r->AddDouble(m, f, v) : r->SetDouble(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_FLOAT: {
        const float v = use_default ? f->default_value_float()
                                    : static_cast<float>(RandomDouble());
        repeated ? r->AddFloat(m, f, v) : r->SetFloat(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_INT32: {
        const int32_t v =
            use_default ? f->default_value_int32() : RandomInt<int32_t>();
        repeated ? r->AddInt32(m, f, v) : r->SetInt32(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_INT64: {
        const int64_t v =
            use_default ? f->default_value_int64() : RandomInt<int64_t>();
        repeated ? r->AddInt64(m, f, v) : r->SetInt64(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_UINT32: {
        const uint32_t v =
            use_default ? f->default_value_uint32() : RandomInt<uint32_t>();
        repeated ? r->AddUInt32(m, f, v) : r->SetUInt32(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_UINT64: {
        const uint64_t v =
            use_default ? f->default_value_uint64() : RandomInt<uint64_t>();
        repeated ? r->AddUInt64(m, f, v) : r->SetUInt64(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_BOOL: {
        const bool v = use_default ? f->default_value_bool() : Chance(0.5);
        repeated ? r->AddBool(m, f, v) : r->SetBool(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        const auto* e = f->enum_type();
        const int v = use_default
                          ? f->default_value_enum()->number()
                          : e->value(Int(0, e->value_count() - 1))->number();
        repeated ? r->AddEnumValue(m, f, v) : r->SetEnumValue(m, f, v);
        break;
      }
      case FieldDescriptor::CPPTYPE_STRING: {
        const std::string v =
            use_default ? f->default_value_string()
                        : RandomString(f->type() == FieldDescriptor::TYPE_BYTES);
        repeated ? r->AddString(m, f, v) : r->SetString(m, f, v);
        break;
      }
      default:
        break;
    }
  }

  std::mt19937_64 rng_;
  int budget_ = 400;   // messages per top-level message
};

/// True when the two messages are equal including field presence (NaN equals
/// NaN). Fills `diff` otherwise.
bool SameMessage(const Message& a, const Message& b, std::string* diff) {
  google::protobuf::util::MessageDifferencer differencer;
  google::protobuf::util::DefaultFieldComparator comparator;
  comparator.set_treat_nan_as_equal(true);
  differencer.set_field_comparator(&comparator);
  differencer.ReportDifferencesToString(diff);
  return differencer.Compare(a, b);
}

bool HasRequiredField(const Descriptor* d) {
  for (int i = 0; i < d->field_count(); ++i) {
    if (d->field(i)->is_required()) {
      return true;
    }
  }
  return false;
}

TEST(NativeTypesTest, RegistryIsInstalled) {
  auto* types = NativeTypes::Instance();
  EXPECT_TRUE(types->enabled());
  EXPECT_GT(types->size(), 700u);
  const auto* chatter =
      types->FindByProtoName("apollo.cyber.examples.proto.Chatter");
  ASSERT_NE(chatter, nullptr);
  EXPECT_EQ(kChatterType, chatter->ros_type);
  EXPECT_EQ(chatter, types->FindByRosType(kChatterType));
  EXPECT_EQ(nullptr, types->FindByProtoName("apollo.cyber.ros2.test.Chatter"));
  const auto* driver =
      types->FindService("apollo.cyber.examples.proto.Driver",
                         "apollo.cyber.examples.proto.Driver");
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(kDriverSrvType, driver->ros_type);
  // Types that are not compiled into this process come from the installed
  // descriptor set.
  const Descriptor* trajectory =
      types->FindDescriptor("apollo.planning.ADCTrajectory");
  ASSERT_NE(trajectory, nullptr);
  EXPECT_NE(nullptr, types->NewMessage(trajectory));
  EXPECT_EQ(Chatter::descriptor(),
            types->FindDescriptor("apollo.cyber.examples.proto.Chatter"));
}

/// Every registered type: protobuf -> ROS 2 message -> CDR -> ROS 2 message
/// -> protobuf gives back the same message, including field presence.
TEST(NativeTypesTest, EveryTypeRoundTrips) {
  auto* types = NativeTypes::Instance();
  const auto names = types->ProtoNames();
  ASSERT_GT(names.size(), 700u);
  size_t messages = 0;
  std::vector<std::string> failures;
  for (size_t index = 0; index < names.size(); ++index) {
    const std::string& name = names[index];
    const Descriptor* descriptor = types->FindDescriptor(name);
    if (descriptor == nullptr) {
      failures.push_back(name + ": no descriptor");
      continue;
    }
    auto converter = MessageConverter::Get(descriptor);
    if (converter == nullptr) {
      failures.push_back(name + ": no converter");
      continue;
    }
    EXPECT_EQ(types->FindByProtoName(name)->ros_type, converter->ros_type());
    // Seed 0 is the empty message (unless fields are required).
    for (uint64_t seed = HasRequiredField(descriptor) ? 1 : 0; seed < 6;
         ++seed) {
      auto original = types->NewMessage(descriptor);
      ASSERT_NE(original, nullptr) << name;
      if (seed != 0) {
        RandomFiller(index * 1000 + seed).Fill(original.get());
      }
      rclcpp::SerializedMessage serialized;
      if (!converter->ToSerialized(*original, &serialized)) {
        failures.push_back(name + ": ToSerialized failed");
        break;
      }
      auto copy = types->NewMessage(descriptor);
      if (!converter->FromSerialized(serialized, copy.get())) {
        failures.push_back(name + ": FromSerialized failed");
        break;
      }
      std::string diff;
      if (!SameMessage(*original, *copy, &diff)) {
        failures.push_back(name + " (seed " + std::to_string(seed) + "):\n" +
                           diff.substr(0, 600));
        break;
      }
      ++messages;
    }
  }
  std::string report;
  for (size_t i = 0; i < failures.size() && i < 25; ++i) {
    report += failures[i] + "\n";
  }
  EXPECT_TRUE(failures.empty())
      << failures.size() << " of " << names.size() << " types failed:\n"
      << report;
  std::cout << "[          ] " << names.size() << " types, " << messages
            << " messages round-tripped" << std::endl;
}

/// Every registered service has converters for its request and response.
TEST(NativeTypesTest, EveryServiceHasConverters) {
  const auto services = NativeTypes::Instance()->Services();
  ASSERT_GE(services.size(), 14u);
  for (const auto& service : services) {
    std::shared_ptr<const MessageConverter> request;
    std::shared_ptr<const MessageConverter> response;
    ASSERT_TRUE(MessageConverter::GetForService(service, &request, &response))
        << service.ros_type;
    auto* types = NativeTypes::Instance();
    auto original = types->NewMessage(request->descriptor());
    RandomFiller(7).Fill(original.get());
    auto ros = request->CreateRos();
    request->ToRos(*original, ros.get());
    auto copy = types->NewMessage(request->descriptor());
    ASSERT_TRUE(request->FromRos(ros.get(), copy.get())) << service.ros_type;
    std::string diff;
    EXPECT_TRUE(SameMessage(*original, *copy, &diff))
        << service.ros_type << ":\n" << diff;
  }
}

/// What a plain ROS 2 node sees and what it can send.
TEST(NativeTypesTest, PresenceSemantics) {
  auto converter = MessageConverter::Get(Chatter::descriptor());
  ASSERT_NE(converter, nullptr);

  // Apollo -> ROS 2: values plus one presence bit per optional field.
  Chatter chatter;
  chatter.set_timestamp(0);   // explicitly set to the default
  chatter.set_seq(7);
  chatter.set_content(std::string("\x00\x01\xff", 3));
  auto ros = converter->CreateRos();
  converter->ToRos(chatter, ros.get());
  const auto& out = *static_cast<const RosChatter*>(ros.get());
  EXPECT_EQ(0u, out.timestamp);
  EXPECT_EQ(7u, out.seq);
  EXPECT_EQ((std::vector<uint8_t>{0x00, 0x01, 0xff}), out.content);
  EXPECT_EQ(RosChatter::HAS_TIMESTAMP | RosChatter::HAS_SEQ |
                RosChatter::HAS_CONTENT,
            out.proto_has_bits);

  // ROS 2 -> Apollo from a publisher that never heard of presence bits: a
  // field is present when it differs from its default.
  RosChatter plain;
  plain.seq = 9;
  plain.content = {1, 2, 3};
  Chatter from_plain;
  ASSERT_TRUE(converter->FromRos(&plain, &from_plain));
  EXPECT_TRUE(from_plain.has_seq());
  EXPECT_EQ(9u, from_plain.seq());
  EXPECT_EQ(std::string("\x01\x02\x03"), from_plain.content());
  EXPECT_FALSE(from_plain.has_timestamp());
  EXPECT_FALSE(from_plain.has_lidar_timestamp());

  // ... and a publisher that sets a bit can send an explicit default.
  plain.proto_has_bits = RosChatter::HAS_LIDAR_TIMESTAMP;
  Chatter with_bit;
  ASSERT_TRUE(converter->FromRos(&plain, &with_bit));
  EXPECT_TRUE(with_bit.has_lidar_timestamp());
  EXPECT_EQ(0u, with_bit.lidar_timestamp());
}

/// A nested message from a plain ROS 2 publisher is present exactly when
/// something in it was filled in.
TEST(NativeTypesTest, NestedPresenceFromPlainRos) {
  auto* types = NativeTypes::Instance();
  const Descriptor* descriptor =
      types->FindDescriptor("apollo.localization.LocalizationEstimate");
  ASSERT_NE(descriptor, nullptr);
  auto converter = MessageConverter::Get(descriptor);
  ASSERT_NE(converter, nullptr);

  // Only pose.position.x is set: no other sub-message may appear on the way
  // through ROS 2.
  auto original = types->NewMessage(descriptor);
  const Reflection* r = original->GetReflection();
  Message* pose =
      r->MutableMessage(original.get(), descriptor->FindFieldByName("pose"));
  Message* position = pose->GetReflection()->MutableMessage(
      pose, pose->GetDescriptor()->FindFieldByName("position"));
  position->GetReflection()->SetDouble(
      position, position->GetDescriptor()->FindFieldByName("x"), 12.5);
  rclcpp::SerializedMessage serialized;
  ASSERT_TRUE(converter->ToSerialized(*original, &serialized));
  auto copy = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromSerialized(serialized, copy.get()));
  std::string diff;
  EXPECT_TRUE(SameMessage(*original, *copy, &diff)) << diff;
  // Only pose.position.x is present: no other sub-message appeared.
  EXPECT_EQ(original->ByteSizeLong(), copy->ByteSizeLong());
}

/// A newly constructed ROS 2 message converts to the empty protobuf message
/// for every type whose defaults a .msg file can express.
TEST(NativeTypesTest, UntouchedRosMessageIsEmpty) {
  auto* types = NativeTypes::Instance();
  size_t matching = 0;
  size_t total = 0;
  for (const auto& name : types->ProtoNames()) {
    const Descriptor* descriptor = types->FindDescriptor(name);
    ASSERT_NE(descriptor, nullptr) << name;
    auto converter = MessageConverter::Get(descriptor);
    ASSERT_NE(converter, nullptr) << name;
    ++total;
    if (!converter->DefaultsMatch()) {
      continue;
    }
    ++matching;
    // Required fields and the key/value of a map entry are always set.
    if (HasRequiredField(descriptor) || descriptor->options().map_entry()) {
      continue;
    }
    auto ros = converter->CreateRos();
    auto message = types->NewMessage(descriptor);
    ASSERT_TRUE(converter->FromRos(ros.get(), message.get())) << name;
    EXPECT_EQ(0u, message->ByteSizeLong()) << name << ":\n"
                                           << message->DebugString();
  }
  EXPECT_GT(matching, total / 2);
  std::cout << "[          ] " << matching << " of " << total
            << " types have ROS 2 defaults equal to their protobuf defaults"
            << std::endl;
}

/// Apollo's geometry types default to NaN, which a .msg file cannot say: a
/// new ROS 2 message holds 0 there. A zero is a value (a standing obstacle
/// has velocity 0, not NaN); NaN says "absent".
TEST(NativeTypesTest, NanDefaults) {
  using RosObstacle = apollo_msgs::msg::PerceptionPerceptionObstacle;
  auto* types = NativeTypes::Instance();
  const Descriptor* descriptor =
      types->FindDescriptor("apollo.perception.PerceptionObstacle");
  ASSERT_NE(descriptor, nullptr);
  auto converter = MessageConverter::Get(descriptor);
  ASSERT_NE(converter, nullptr);
  EXPECT_FALSE(converter->DefaultsMatch());
  const FieldDescriptor* velocity = descriptor->FindFieldByName("velocity");
  ASSERT_NE(velocity, nullptr);

  // Apollo -> ROS 2: an absent velocity reads NaN, as it does in Apollo.
  auto empty = types->NewMessage(descriptor);
  auto ros = converter->CreateRos();
  converter->ToRos(*empty, ros.get());
  const auto& shown = *static_cast<const RosObstacle*>(ros.get());
  EXPECT_TRUE(std::isnan(shown.velocity.x));
  EXPECT_EQ(0u, shown.proto_has_bits & RosObstacle::HAS_VELOCITY);
  auto back = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromRos(ros.get(), back.get()));
  EXPECT_EQ(0u, back->ByteSizeLong());

  // ROS 2 -> Apollo: a plain publisher's zero velocity is a zero velocity.
  RosObstacle plain;
  plain.id = 7;
  EXPECT_EQ(0.0, plain.velocity.x);
  auto from_plain = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromRos(&plain, from_plain.get()));
  const Reflection* r = from_plain->GetReflection();
  ASSERT_TRUE(r->HasField(*from_plain, velocity));
  const Message& v = r->GetMessage(*from_plain, velocity);
  EXPECT_EQ(0.0, v.GetReflection()->GetDouble(
                     v, v.GetDescriptor()->FindFieldByName("x")));

  // ... and NaN says "no velocity".
  plain.velocity.x = plain.velocity.y = plain.velocity.z =
      std::numeric_limits<double>::quiet_NaN();
  auto without = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromRos(&plain, without.get()));
  EXPECT_FALSE(without->GetReflection()->HasField(*without, velocity));
}

/// A oneof is only selected by a member the publisher set a bit for or
/// changed, never by a member that merely holds zeros where protobuf
/// defaults to NaN.
TEST(NativeTypesTest, OneofFromPlainRos) {
  using RosMainDecision = apollo_msgs::msg::PlanningMainDecision;
  auto* types = NativeTypes::Instance();
  const Descriptor* descriptor =
      types->FindDescriptor("apollo.planning.MainDecision");
  ASSERT_NE(descriptor, nullptr);
  auto converter = MessageConverter::Get(descriptor);
  ASSERT_NE(converter, nullptr);
  const auto* task = descriptor->FindOneofByName("task");
  ASSERT_NE(task, nullptr);

  RosMainDecision untouched;
  auto none = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromRos(&untouched, none.get()));
  EXPECT_FALSE(none->GetReflection()->HasOneof(*none, task));
  EXPECT_EQ(0u, none->ByteSizeLong()) << none->DebugString();

  RosMainDecision stop;
  stop.stop.reason = "pedestrian";
  auto stopped = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromRos(&stop, stopped.get()));
  const Reflection* r = stopped->GetReflection();
  ASSERT_TRUE(r->HasOneof(*stopped, task));
  EXPECT_EQ("stop", r->GetOneofFieldDescriptor(*stopped, task)->name());

  // An empty member can be selected with its presence bit.
  RosMainDecision cruise;
  cruise.proto_has_bits = RosMainDecision::HAS_CRUISE;
  auto cruising = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromRos(&cruise, cruising.get()));
  ASSERT_TRUE(cruising->GetReflection()->HasOneof(*cruising, task));
  EXPECT_EQ("cruise", cruising->GetReflection()
                          ->GetOneofFieldDescriptor(*cruising, task)
                          ->name());
}

/// A process built against a different definition of a message must not use
/// the native type generated from another definition.
TEST(NativeTypesTest, DifferentDefinitionFallsBack) {
  google::protobuf::FileDescriptorProto file;
  file.set_name("other/examples.proto");
  file.set_package("apollo.cyber.examples.proto");
  auto* message = file.add_message_type();
  message->set_name("Chatter");
  auto* field = message->add_field();
  field->set_name("timestamp");
  field->set_number(1);
  field->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
  field->set_type(google::protobuf::FieldDescriptorProto::TYPE_UINT64);
  google::protobuf::DescriptorPool pool;
  const auto* built = pool.BuildFile(file);
  ASSERT_NE(built, nullptr);
  const Descriptor* other = built->message_type(0);
  EXPECT_EQ("apollo.cyber.examples.proto.Chatter", other->full_name());
  EXPECT_NE(ros2::StructureHash(Chatter::descriptor()),
            ros2::StructureHash(other));
  EXPECT_EQ(nullptr, MessageConverter::Get(other));
  EXPECT_NE(nullptr, MessageConverter::Get(Chatter::descriptor()));
}

/// Cost of the conversion for a large message (information only).
TEST(NativeTypesTest, LargeMessageConversionCost) {
  auto* types = NativeTypes::Instance();
  const Descriptor* descriptor = types->FindDescriptor("apollo.drivers.PointCloud");
  ASSERT_NE(descriptor, nullptr);
  auto converter = MessageConverter::Get(descriptor);
  ASSERT_NE(converter, nullptr);
  auto cloud = types->NewMessage(descriptor);
  const Reflection* r = cloud->GetReflection();
  const FieldDescriptor* points = descriptor->FindFieldByName("point");
  ASSERT_NE(points, nullptr);
  const int kPoints = 100000;
  for (int i = 0; i < kPoints; ++i) {
    Message* p = r->AddMessage(cloud.get(), points);
    const Reflection* pr = p->GetReflection();
    const Descriptor* pd = p->GetDescriptor();
    pr->SetFloat(p, pd->FindFieldByName("x"), 0.01f * i);
    pr->SetFloat(p, pd->FindFieldByName("y"), -0.02f * i);
    pr->SetFloat(p, pd->FindFieldByName("z"), 1.5f);
    pr->SetUInt32(p, pd->FindFieldByName("intensity"), i % 255);
    pr->SetUInt64(p, pd->FindFieldByName("timestamp"), 1700000000000000000ull + i);
  }
  const auto t0 = std::chrono::steady_clock::now();
  rclcpp::SerializedMessage serialized;
  ASSERT_TRUE(converter->ToSerialized(*cloud, &serialized));
  const auto t1 = std::chrono::steady_clock::now();
  auto copy = types->NewMessage(descriptor);
  ASSERT_TRUE(converter->FromSerialized(serialized, copy.get()));
  const auto t2 = std::chrono::steady_clock::now();
  std::string diff;
  EXPECT_TRUE(SameMessage(*cloud, *copy, &diff)) << diff.substr(0, 400);
  const auto ms = [](std::chrono::steady_clock::duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
  };
  std::cout << "[          ] PointCloud with " << kPoints << " points: protobuf "
            << cloud->ByteSizeLong() << " bytes, CDR " << serialized.size()
            << " bytes, to ROS 2 " << ms(t1 - t0) << " ms, from ROS 2 "
            << ms(t2 - t1) << " ms" << std::endl;
}

// ---------------------------------------------------------------------------
// Transport: Cyber <-> plain rclcpp in one process, over the middleware
// ---------------------------------------------------------------------------

/// A plain rclcpp node spun on its own thread. It knows nothing about Apollo
/// except the generated message types.
class PlainRosPeer {
 public:
  explicit PlainRosPeer(const std::string& name)
      : node_(std::make_shared<rclcpp::Node>(name)) {
    executor_.add_node(node_);
    thread_ = std::thread([this]() {
      while (!stop_.load() && rclcpp::ok()) {
        executor_.spin_once(std::chrono::milliseconds(20));
      }
    });
  }

  ~PlainRosPeer() {
    stop_.store(true);
    thread_.join();
    executor_.remove_node(node_);
  }

  const rclcpp::Node::SharedPtr& node() const { return node_; }

  std::vector<std::string> TopicTypes(const std::string& topic) const {
    auto topics = node_->get_topic_names_and_types();
    auto it = topics.find(topic);
    return it == topics.end() ? std::vector<std::string>() : it->second;
  }

  std::vector<std::string> ServiceTypes(const std::string& service) const {
    auto services = node_->get_service_names_and_types();
    auto it = services.find(service);
    return it == services.end() ? std::vector<std::string>() : it->second;
  }

 private:
  rclcpp::Node::SharedPtr node_;
  rclcpp::executors::SingleThreadedExecutor executor_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};

template <typename T>
class Collector {
 public:
  void Add(const T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_.push_back(value);
  }
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.size();
  }
  std::vector<T> values() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<T> values_;
};

TEST(NativeTransportTest, WriterPublishesNativeType) {
  const std::string topic = "/test/native/chatter_out";
  PlainRosPeer peer("plain_listener");
  Collector<RosChatter> received;
  auto subscription = peer.node()->create_subscription<RosChatter>(
      topic, 10, [&](RosChatter::ConstSharedPtr m) { received.Add(*m); });

  auto node = CreateNode("native_talker");
  auto writer = node->CreateWriter<Chatter>(topic);
  ASSERT_NE(writer, nullptr);
  ASSERT_TRUE(WaitFor([&]() { return writer->HasReader(); }));
  EXPECT_EQ((std::vector<std::string>{kChatterType}), peer.TopicTypes(topic));

  Chatter chatter;
  chatter.set_timestamp(123456789);
  chatter.set_seq(1);
  chatter.set_content("payload");
  ASSERT_TRUE(writer->Write(chatter));
  ASSERT_TRUE(WaitFor([&]() { return received.size() == 1; }));
  const RosChatter got = received.values()[0];
  EXPECT_EQ(123456789u, got.timestamp);
  EXPECT_EQ(1u, got.seq);
  EXPECT_EQ(0u, got.lidar_timestamp);
  EXPECT_EQ("payload", std::string(got.content.begin(), got.content.end()));
  EXPECT_EQ(RosChatter::HAS_TIMESTAMP | RosChatter::HAS_SEQ |
                RosChatter::HAS_CONTENT,
            got.proto_has_bits);
}

TEST(NativeTransportTest, ReaderReceivesFromPlainPublisher) {
  const std::string topic = "/test/native/chatter_in";
  PlainRosPeer peer("plain_talker");
  auto publisher = peer.node()->create_publisher<RosChatter>(topic, 10);

  auto node = CreateNode("native_listener");
  Collector<Chatter> received;
  auto reader = node->CreateReader<Chatter>(
      topic, [&](const std::shared_ptr<Chatter>& m) { received.Add(*m); });
  ASSERT_NE(reader, nullptr);
  ASSERT_TRUE(
      WaitFor([&]() { return publisher->get_subscription_count() == 1; }));

  RosChatter message;
  message.seq = 5;
  message.content = {'r', 'o', 's'};
  publisher->publish(message);
  ASSERT_TRUE(WaitFor([&]() { return received.size() == 1; }));
  const Chatter got = received.values()[0];
  EXPECT_EQ(5u, got.seq());
  EXPECT_EQ("ros", got.content());
  EXPECT_FALSE(got.has_timestamp());
}

/// The same process may hold Cyber readers, Cyber writers and plain ROS 2
/// subscriptions on one topic: readers get each message once (through the
/// zero-copy path), the plain subscription gets it from the middleware.
TEST(NativeTransportTest, NoDuplicatesWithLocalAndPlainReaders) {
  const std::string topic = "/test/native/mixed";
  PlainRosPeer peer("plain_mixed");
  Collector<uint64_t> plain_received;
  auto subscription = peer.node()->create_subscription<RosChatter>(
      topic, 50, [&](RosChatter::ConstSharedPtr m) { plain_received.Add(m->seq); });

  auto node = CreateNode("native_mixed");
  Collector<uint64_t> cyber_received;
  ReaderConfig config;
  config.channel_name = topic;
  config.pending_queue_size = 50;
  auto reader = node->CreateReader<Chatter>(
      config,
      [&](const std::shared_ptr<Chatter>& m) { cyber_received.Add(m->seq()); });
  auto writer = node->CreateWriter<Chatter>(topic);
  ASSERT_TRUE(WaitFor([&]() {
    return node->GetReader<Chatter>(topic) != nullptr &&
           peer.node()->count_publishers(topic) == 1 &&
           peer.node()->count_subscribers(topic) == 2;
  }));
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  const size_t kCount = 20;
  for (size_t i = 1; i <= kCount; ++i) {
    Chatter chatter;
    chatter.set_seq(i);
    ASSERT_TRUE(writer->Write(chatter));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_TRUE(WaitFor([&]() { return plain_received.size() == kCount; }));
  ASSERT_TRUE(WaitFor([&]() { return cyber_received.size() >= kCount; }));
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(kCount, cyber_received.size());
  EXPECT_EQ(kCount, plain_received.size());
}

/// A raw reader (recorder, monitor) takes the type of a channel from the
/// ROS 2 graph, also when the writer appears after the reader.
TEST(NativeTransportTest, RawReaderFollowsTheGraph) {
  const std::string topic = "/test/native/raw_in";
  auto node = CreateNode("native_raw_listener");
  Collector<std::string> received;
  auto reader = node->CreateReader<message::RawMessage>(
      topic, [&](const std::shared_ptr<message::RawMessage>& m) {
        received.Add(m->message);
      });
  ASSERT_NE(reader, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  PlainRosPeer peer("plain_raw_talker");
  auto publisher = peer.node()->create_publisher<RosChatter>(topic, 10);
  ASSERT_TRUE(
      WaitFor([&]() { return publisher->get_subscription_count() == 1; }));
  RosChatter message;
  message.seq = 77;
  publisher->publish(message);
  ASSERT_TRUE(WaitFor([&]() { return received.size() == 1; }));
  Chatter got;
  ASSERT_TRUE(got.ParseFromString(received.values()[0]));
  EXPECT_EQ(77u, got.seq());

  std::string type;
  service_discovery::TopologyManager::Instance()->channel_manager()->GetMsgType(
      topic, &type);
  EXPECT_EQ("apollo.cyber.examples.proto.Chatter", type);
}

/// A raw writer that names its message type (player, bridge) publishes the
/// native type.
TEST(NativeTransportTest, RawWriterPublishesNativeType) {
  const std::string topic = "/test/native/raw_out";
  PlainRosPeer peer("plain_raw_listener");
  Collector<RosChatter> received;
  auto subscription = peer.node()->create_subscription<RosChatter>(
      topic, 10, [&](RosChatter::ConstSharedPtr m) { received.Add(*m); });

  auto node = CreateNode("native_raw_talker");
  proto::RoleAttributes attr;
  attr.set_channel_name(topic);
  attr.set_message_type("apollo.cyber.examples.proto.Chatter");
  auto writer = node->CreateWriter<message::RawMessage>(attr);
  ASSERT_NE(writer, nullptr);
  ASSERT_TRUE(WaitFor([&]() { return writer->HasReader(); }));
  EXPECT_EQ((std::vector<std::string>{kChatterType}), peer.TopicTypes(topic));

  Chatter chatter;
  chatter.set_seq(31);
  auto raw = std::make_shared<message::RawMessage>();
  ASSERT_TRUE(chatter.SerializeToString(&raw->message));
  ASSERT_TRUE(writer->Write(raw));
  ASSERT_TRUE(WaitFor([&]() { return received.size() == 1; }));
  EXPECT_EQ(31u, received.values()[0].seq);
}

/// Latched (transient local) channels: readers that join late get the last
/// message, both in the writer's process and through the middleware.
TEST(NativeTransportTest, LatchedChannelReachesLateJoiners) {
  const std::string topic = "/test/native/latched";
  auto node = CreateNode("native_latched");
  proto::RoleAttributes attr;
  attr.set_channel_name(topic);
  auto* qos = attr.mutable_qos_profile();
  qos->set_history(proto::QosHistoryPolicy::HISTORY_KEEP_LAST);
  qos->set_depth(1);
  qos->set_reliability(proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
  qos->set_durability(proto::QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL);
  auto writer = node->CreateWriter<Chatter>(attr);
  ASSERT_NE(writer, nullptr);
  for (uint64_t seq = 98; seq <= 99; ++seq) {
    Chatter chatter;
    chatter.set_seq(seq);
    ASSERT_TRUE(writer->Write(chatter));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // In the writer's process.
  Collector<uint64_t> late;
  auto late_node = CreateNode("native_latched_late");
  auto late_reader = late_node->CreateReader<Chatter>(
      attr, [&](const std::shared_ptr<Chatter>& m) { late.Add(m->seq()); });
  ASSERT_TRUE(WaitFor([&]() { return late.size() >= 1; }));
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ((std::vector<uint64_t>{99}), late.values());

  // A volatile reader does not ask for history.
  Collector<uint64_t> volatile_late;
  auto volatile_reader = late_node->CreateReader<Chatter>(
      topic + std::string(),
      [&](const std::shared_ptr<Chatter>& m) { volatile_late.Add(m->seq()); });
  (void)volatile_reader;

  // Through the middleware.
  PlainRosPeer peer("plain_latched");
  Collector<uint64_t> plain;
  auto subscription = peer.node()->create_subscription<RosChatter>(
      topic, rclcpp::QoS(1).transient_local(),
      [&](RosChatter::ConstSharedPtr m) { plain.Add(m->seq); });
  ASSERT_TRUE(WaitFor([&]() { return plain.size() >= 1; }));
  EXPECT_EQ((std::vector<uint64_t>{99}), plain.values());
}

TEST(NativeTransportTest, ServiceAnswersPlainClient) {
  const std::string name = "/test/native/driver";
  auto node = CreateNode("native_server");
  auto service = node->CreateService<Driver, Driver>(
      name, [](const std::shared_ptr<Driver>& request,
               std::shared_ptr<Driver>& response) {
        response->set_msg_id(request->msg_id() + 1);
        response->set_content("re: " + request->content());
      });
  ASSERT_NE(service, nullptr);

  PlainRosPeer peer("plain_client");
  auto client = peer.node()->create_client<RosDriver>(name);
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(5)));
  EXPECT_EQ((std::vector<std::string>{kDriverSrvType}), peer.ServiceTypes(name));

  auto request = std::make_shared<RosDriver::Request>();
  request->msg_id = 41;
  request->content = "hello";
  auto future = client->async_send_request(request);
  ASSERT_EQ(std::future_status::ready, future.wait_for(std::chrono::seconds(5)));
  auto response = future.get();
  EXPECT_EQ(42u, response->msg_id);
  EXPECT_EQ("re: hello", response->content);
  EXPECT_EQ(RosDriver::Response::HAS_MSG_ID | RosDriver::Response::HAS_CONTENT,
            response->proto_has_bits);

  // The Cyber client of the same process uses the same native service.
  auto cyber_client = node->CreateClient<Driver, Driver>(name);
  ASSERT_NE(cyber_client, nullptr);
  ASSERT_TRUE(cyber_client->WaitForService(std::chrono::seconds(5)));
  Driver cyber_request;
  cyber_request.set_msg_id(1);
  cyber_request.set_content("cyber");
  auto cyber_response =
      cyber_client->SendRequest(cyber_request, std::chrono::seconds(5));
  ASSERT_NE(cyber_response, nullptr);
  EXPECT_EQ(2u, cyber_response->msg_id());
  EXPECT_EQ("re: cyber", cyber_response->content());
  EXPECT_FALSE(cyber_response->has_timestamp());
}

TEST(NativeTransportTest, ClientCallsPlainService) {
  const std::string name = "/test/native/ros_driver";
  PlainRosPeer peer("plain_server");
  auto service = peer.node()->create_service<RosDriver>(
      name, [](const std::shared_ptr<RosDriver::Request> request,
               std::shared_ptr<RosDriver::Response> response) {
        response->msg_id = request->msg_id * 2;
        response->content = request->content + "!";
      });

  auto node = CreateNode("native_client");
  auto client = node->CreateClient<Driver, Driver>(name);
  ASSERT_NE(client, nullptr);
  ASSERT_TRUE(client->WaitForService(std::chrono::seconds(5)));
  Driver request;
  request.set_msg_id(21);
  request.set_content("ping");
  auto response = client->SendRequest(request, std::chrono::seconds(5));
  ASSERT_NE(response, nullptr);
  EXPECT_EQ(42u, response->msg_id());
  EXPECT_EQ("ping!", response->content());

  auto future = client->AsyncSendRequest(request);
  ASSERT_EQ(std::future_status::ready, future.wait_for(std::chrono::seconds(5)));
  ASSERT_NE(future.get(), nullptr);
  EXPECT_EQ(42u, future.get()->msg_id());
}

/// Types without a generated interface, and channels listed in
/// APOLLO_ROS_PROTO_CHANNELS, keep the serialized-protobuf wire format.
TEST(NativeTransportTest, ProtobufWireFallback) {
  PlainRosPeer peer("plain_fallback");
  auto node = CreateNode("native_fallback");

  // No generated interface for apollo.cyber.ros2.test.Chatter.
  const std::string unregistered = "/test/native/unregistered";
  auto unregistered_writer =
      node->CreateWriter<ros2::test::Chatter>(unregistered);
  ASSERT_NE(unregistered_writer, nullptr);
  ASSERT_TRUE(
      WaitFor([&]() { return !peer.TopicTypes(unregistered).empty(); }));
  EXPECT_EQ((std::vector<std::string>{kProtoMessageType}),
            peer.TopicTypes(unregistered));

  // Opted out by name (the test sets APOLLO_ROS_PROTO_CHANNELS).
  const std::string opted_out = "/test/protowire/chatter";
  Collector<RosProtoMessage> received;
  auto subscription = peer.node()->create_subscription<RosProtoMessage>(
      opted_out, 10,
      [&](RosProtoMessage::ConstSharedPtr m) { received.Add(*m); });
  auto writer = node->CreateWriter<Chatter>(opted_out);
  ASSERT_NE(writer, nullptr);
  ASSERT_TRUE(WaitFor([&]() { return writer->HasReader(); }));
  EXPECT_EQ((std::vector<std::string>{kProtoMessageType}),
            peer.TopicTypes(opted_out));
  Chatter chatter;
  chatter.set_seq(3);
  ASSERT_TRUE(writer->Write(chatter));
  ASSERT_TRUE(WaitFor([&]() { return received.size() == 1; }));
  const RosProtoMessage got = received.values()[0];
  EXPECT_EQ("apollo.cyber.examples.proto.Chatter", got.type);
  Chatter parsed;
  ASSERT_TRUE(parsed.ParseFromArray(got.data.data(),
                                    static_cast<int>(got.data.size())));
  EXPECT_EQ(3u, parsed.seq());
}

/// Channel topology: native Apollo types are channels, other ROS 2 topics
/// are not.
TEST(NativeTransportTest, TopologyKnowsNativeChannels) {
  PlainRosPeer peer("plain_topology");
  auto apollo_publisher =
      peer.node()->create_publisher<RosChatter>("/test/native/topology", 10);
  auto other_publisher = peer.node()->create_publisher<
      rcl_interfaces::msg::Log>("/test/native/not_apollo", 10);
  auto* channel_manager =
      service_discovery::TopologyManager::Instance()->channel_manager().get();
  ASSERT_TRUE(WaitFor(
      [&]() { return channel_manager->HasWriter("/test/native/topology"); }));
  std::vector<std::string> channels;
  channel_manager->GetChannelNames(&channels);
  EXPECT_NE(std::find(channels.begin(), channels.end(), "/test/native/topology"),
            channels.end());
  EXPECT_EQ(std::find(channels.begin(), channels.end(),
                      "/test/native/not_apollo"),
            channels.end());
  std::string type;
  channel_manager->GetMsgType("/test/native/topology", &type);
  EXPECT_EQ("apollo.cyber.examples.proto.Chatter", type);
  std::string proto_desc;
  channel_manager->GetProtoDesc("/test/native/topology", &proto_desc);
  EXPECT_FALSE(proto_desc.empty());
  std::vector<proto::RoleAttributes> writers;
  channel_manager->GetWritersOfChannel("/test/native/topology", &writers);
  ASSERT_EQ(1u, writers.size());
  EXPECT_EQ("apollo.cyber.examples.proto.Chatter", writers[0].message_type());
  EXPECT_EQ("plain_topology", writers[0].node_name());
}

}  // namespace
}  // namespace cyber
}  // namespace apollo
