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
 * @file native_types.h
 * @brief Native ROS 2 interfaces for Apollo protobuf messages.
 *
 * tools/ros2/proto2ros.py generates one ROS 2 message type per Apollo
 * protobuf message (package apollo_msgs) together with a registry that
 * describes the mapping. This file is the run-time side: it reads the
 * registries found through the ament index and converts between protobuf
 * messages (reflection) and ROS 2 messages (rosidl introspection), so the
 * Cyber transport can put ordinary, typed ROS 2 messages on the wire without
 * compile-time knowledge of the generated types.
 *
 * Field presence: a ROS 2 message carries `proto_has_bits`; a field is
 * present when its bit is set or its value differs from the protobuf
 * default. Apollo -> ROS 2 -> Apollo therefore round-trips exactly, and
 * messages from plain ROS 2 publishers (which leave the bits at zero) carry
 * every field they changed. The value a ROS 2 node sees is always the value
 * Apollo reads: an absent field shows its protobuf default.
 *
 * A few Apollo fields default to NaN, which a .msg file cannot express, so
 * a newly constructed ROS 2 message holds 0 there. Such a zero is a value
 * like any other (a velocity of 0 must not turn into NaN), which means an
 * untouched sub-message of such a type arrives as "present, all zero". Set
 * the fields to NaN to say "absent". Oneof members are stricter: a member
 * only selects the oneof when its bit is set or the publisher changed it.
 */

#ifndef CYBER_ROS2_NATIVE_TYPES_H_
#define CYBER_ROS2_NATIVE_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace rclcpp {
class SerializedMessage;
}  // namespace rclcpp

namespace apollo {
namespace cyber {
namespace ros2 {

/// One member of a generated ROS 2 message.
struct NativeMember {
  char kind = 'f';        ///< 'f' field, 'r' recursive field, 'h' presence word
  std::string ros_name;   ///< member name in the ROS 2 message
  int number = 0;         ///< protobuf field number ('f', 'r') or word ('h')
  int bit = -1;           ///< presence bit, -1 when presence is not tracked
};

/// Registry entry of a protobuf message type.
struct NativeTypeInfo {
  std::string proto_name;   ///< e.g. apollo.planning.ADCTrajectory
  std::string ros_type;     ///< e.g. apollo_msgs/msg/PlanningADCTrajectory
  std::string hash;         ///< structure hash of the protobuf definition
  std::vector<NativeMember> members;
};

/// Registry entry of an Apollo service.
struct NativeServiceInfo {
  std::string request;      ///< protobuf request type
  std::string response;     ///< protobuf response type
  std::string ros_type;     ///< e.g. apollo_msgs/srv/ExternalCommandLaneFollowCommand
};

/**
 * @class NativeTypes
 * @brief The registries of all installed interface packages (resource type
 * `apollo_proto_types` in the ament index) plus their protobuf descriptors.
 */
class NativeTypes {
 public:
  static NativeTypes* Instance();

  const NativeTypeInfo* FindByProtoName(const std::string& proto_name);
  const NativeTypeInfo* FindByRosType(const std::string& ros_type);
  const NativeServiceInfo* FindService(const std::string& request,
                                       const std::string& response);
  const NativeServiceInfo* FindServiceByRosType(const std::string& ros_type);

  /// Descriptor of a protobuf type: the types compiled into the process
  /// first, then the descriptor sets shipped with the interface packages (so
  /// tools work with types they were not built against).
  const google::protobuf::Descriptor* FindDescriptor(
      const std::string& proto_name);

  /// New, empty message of the given type.
  std::unique_ptr<google::protobuf::Message> NewMessage(
      const google::protobuf::Descriptor* descriptor);

  /// False when native types are switched off (APOLLO_ROS_WIRE_FORMAT=proto).
  bool enabled() const { return enabled_; }

  /// True when `channel` must keep the protobuf wire format: native types
  /// are disabled, or the channel matches APOLLO_ROS_PROTO_CHANNELS (regex).
  bool UseProtoWire(const std::string& channel) const;

  /// Number of registered message types (diagnostics).
  size_t size();
  /// All registered protobuf type names, sorted.
  std::vector<std::string> ProtoNames();
  /// All registered services.
  std::vector<NativeServiceInfo> Services();

 private:
  NativeTypes();
  void Load();

  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool enabled_ = true;
};

/**
 * @class MessageConverter
 * @brief Converts between one protobuf type and its native ROS 2 type.
 */
class MessageConverter {
 public:
  /// Converter for `descriptor`, or null when the type has no native ROS 2
  /// interface that matches this definition (the caller then uses
  /// apollo_msgs/msg/ProtoMessage).
  static std::shared_ptr<const MessageConverter> Get(
      const google::protobuf::Descriptor* descriptor);

  /// Converters for the request and response of a native service type.
  static bool GetForService(
      const NativeServiceInfo& service,
      std::shared_ptr<const MessageConverter>* request,
      std::shared_ptr<const MessageConverter>* response);

  ~MessageConverter();

  const std::string& ros_type() const;
  const google::protobuf::Descriptor* descriptor() const;

  /// True when a default-constructed ROS 2 message of this type holds the
  /// protobuf default of every field, i.e. converts to an empty protobuf
  /// message. False for types with defaults a .msg file cannot express (the
  /// NaN defaults of Apollo's geometry types): there the ROS 2 default is 0.
  bool DefaultsMatch() const;

  /// protobuf -> CDR-serialized ROS 2 message.
  bool ToSerialized(const google::protobuf::Message& in,
                    rclcpp::SerializedMessage* out) const;
  /// CDR-serialized ROS 2 message -> protobuf (`out` is cleared first).
  bool FromSerialized(const rclcpp::SerializedMessage& in,
                      google::protobuf::Message* out) const;

  /// A default-initialized ROS 2 message of this type.
  std::shared_ptr<void> CreateRos() const;
  /// protobuf -> ROS 2 message (`ros` freshly created by CreateRos()).
  void ToRos(const google::protobuf::Message& in, void* ros) const;
  /// ROS 2 message -> protobuf (`out` must be empty).
  bool FromRos(const void* ros, google::protobuf::Message* out) const;

  struct Impl;

 private:
  MessageConverter();
  std::unique_ptr<Impl> impl_;
  friend struct ConverterBuilder;
};

/// Structure hash of a protobuf message definition, as written to the
/// registry by tools/ros2/proto2ros.py.
std::string StructureHash(const google::protobuf::Descriptor* descriptor);

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_ROS2_NATIVE_TYPES_H_
