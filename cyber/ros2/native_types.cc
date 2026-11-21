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

#include "cyber/ros2/native_types.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "ament_index_cpp/get_resource.hpp"
#include "ament_index_cpp/get_resources.hpp"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/typesupport_helpers.hpp"
#include "rcpputils/shared_library.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/service_introspection.hpp"

#include "cyber/common/log.h"

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::FieldDescriptor;
using google::protobuf::Message;
using google::protobuf::Reflection;
using rosidl_typesupport_introspection_cpp::MessageMember;
using rosidl_typesupport_introspection_cpp::MessageMembers;

constexpr char kResourceType[] = "apollo_proto_types";
constexpr char kIntrospection[] = "rosidl_typesupport_introspection_cpp";
constexpr char kTypesupportCpp[] = "rosidl_typesupport_cpp";

std::string Trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) {
    return "";
  }
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

bool SameDouble(double a, double b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}

const MessageMembers* MembersOf(const rosidl_message_type_support_t* ts) {
  return ts == nullptr ? nullptr
                       : static_cast<const MessageMembers*>(ts->data);
}

/// Keeps type support libraries loaded for the life of the process.
std::vector<std::shared_ptr<rcpputils::SharedLibrary>>& LoadedLibraries() {
  static auto* libs = new std::vector<std::shared_ptr<rcpputils::SharedLibrary>>();
  return *libs;
}

std::recursive_mutex& ConverterMutex() {
  static auto* m = new std::recursive_mutex();
  return *m;
}

}  // namespace

// ---------------------------------------------------------------------------
// NativeTypes
// ---------------------------------------------------------------------------

struct NativeTypes::Impl {
  std::mutex mutex;
  bool loaded = false;
  std::unordered_map<std::string, std::unique_ptr<NativeTypeInfo>> by_proto;
  std::unordered_map<std::string, const NativeTypeInfo*> by_ros;
  std::vector<std::unique_ptr<NativeServiceInfo>> services;
  std::vector<std::string> descriptor_files;
  bool descriptors_loaded = false;
  std::unique_ptr<DescriptorPool> pool;
  std::unique_ptr<google::protobuf::DynamicMessageFactory> factory;
  std::unique_ptr<std::regex> proto_channels;

  void ParseRegistry(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
      AWARN << "cannot read " << path;
      return;
    }
    NativeTypeInfo* current = nullptr;
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::istringstream ss(line);
      std::string tag;
      ss >> tag;
      if (tag == "V") {
        int version = 0;
        ss >> version;
        if (version != 1) {
          AWARN << path << ": unsupported registry version " << version;
          return;
        }
      } else if (tag == "M") {
        auto info = std::make_unique<NativeTypeInfo>();
        ss >> info->proto_name >> info->ros_type >> info->hash;
        current = info.get();
        if (by_proto.count(info->proto_name)) {
          current = nullptr;   // first registration wins
          continue;
        }
        by_ros[info->ros_type] = info.get();
        by_proto[info->proto_name] = std::move(info);
      } else if (tag == "f" || tag == "r" || tag == "h") {
        if (current == nullptr) {
          continue;
        }
        NativeMember member;
        member.kind = tag[0];
        ss >> member.ros_name >> member.number >> member.bit;
        current->members.push_back(member);
      } else if (tag == "S") {
        auto service = std::make_unique<NativeServiceInfo>();
        ss >> service->request >> service->response >> service->ros_type;
        services.push_back(std::move(service));
      }
    }
  }

  void LoadRegistries() {
    if (loaded) {
      return;
    }
    loaded = true;
    std::map<std::string, std::string> resources;
    try {
      resources = ament_index_cpp::get_resources(kResourceType);
    } catch (const std::exception& e) {
      ADEBUG << "ament index: " << e.what();
    }
    for (const auto& item : resources) {
      std::string content;
      std::string prefix;
      if (!ament_index_cpp::get_resource(kResourceType, item.first, content,
                                         &prefix)) {
        continue;
      }
      const std::string dir = prefix + "/" + Trim(content);
      ParseRegistry(dir + "/registry.txt");
      descriptor_files.push_back(dir + "/protos.desc");
    }
  }

  void LoadDescriptors() {
    if (descriptors_loaded) {
      return;
    }
    descriptors_loaded = true;
    pool = std::make_unique<DescriptorPool>(DescriptorPool::generated_pool());
    factory =
        std::make_unique<google::protobuf::DynamicMessageFactory>(pool.get());
    factory->SetDelegateToGeneratedFactory(true);
    for (const auto& path : descriptor_files) {
      std::ifstream in(path, std::ios::binary);
      google::protobuf::FileDescriptorSet set;
      if (!in || !set.ParseFromIstream(&in)) {
        AWARN << "cannot read " << path;
        continue;
      }
      for (const auto& file : set.file()) {
        // Files compiled into the process are found through the underlay.
        if (pool->FindFileByName(file.name()) != nullptr) {
          continue;
        }
        if (pool->BuildFile(file) == nullptr) {
          AWARN << "cannot build descriptors of " << file.name();
        }
      }
    }
  }
};

NativeTypes::NativeTypes() : impl_(new Impl()) {
  const char* format = std::getenv("APOLLO_ROS_WIRE_FORMAT");
  if (format != nullptr && std::string(format) == "proto") {
    enabled_ = false;
  }
  const char* channels = std::getenv("APOLLO_ROS_PROTO_CHANNELS");
  if (channels != nullptr && *channels != '\0') {
    try {
      impl_->proto_channels = std::make_unique<std::regex>(channels);
    } catch (const std::regex_error& e) {
      AERROR << "APOLLO_ROS_PROTO_CHANNELS is not a valid regex: " << e.what();
    }
  }
}

NativeTypes* NativeTypes::Instance() {
  static NativeTypes* instance = new NativeTypes();
  return instance;
}

const NativeTypeInfo* NativeTypes::FindByProtoName(
    const std::string& proto_name) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  auto it = impl_->by_proto.find(proto_name);
  return it == impl_->by_proto.end() ? nullptr : it->second.get();
}

const NativeTypeInfo* NativeTypes::FindByRosType(const std::string& ros_type) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  auto it = impl_->by_ros.find(ros_type);
  return it == impl_->by_ros.end() ? nullptr : it->second;
}

const NativeServiceInfo* NativeTypes::FindService(const std::string& request,
                                                  const std::string& response) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  for (const auto& service : impl_->services) {
    if (service->request == request && service->response == response) {
      return service.get();
    }
  }
  return nullptr;
}

const NativeServiceInfo* NativeTypes::FindServiceByRosType(
    const std::string& ros_type) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  for (const auto& service : impl_->services) {
    if (service->ros_type == ros_type) {
      return service.get();
    }
  }
  return nullptr;
}

const Descriptor* NativeTypes::FindDescriptor(const std::string& proto_name) {
  const Descriptor* descriptor =
      DescriptorPool::generated_pool()->FindMessageTypeByName(proto_name);
  if (descriptor != nullptr) {
    return descriptor;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  impl_->LoadDescriptors();
  return impl_->pool->FindMessageTypeByName(proto_name);
}

std::unique_ptr<Message> NativeTypes::NewMessage(const Descriptor* descriptor) {
  if (descriptor == nullptr) {
    return nullptr;
  }
  if (descriptor->file()->pool() == DescriptorPool::generated_pool()) {
    const Message* prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(
            descriptor);
    if (prototype != nullptr) {
      return std::unique_ptr<Message>(prototype->New());
    }
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  impl_->LoadDescriptors();
  const Message* prototype = impl_->factory->GetPrototype(descriptor);
  return prototype == nullptr ? nullptr
                              : std::unique_ptr<Message>(prototype->New());
}

bool NativeTypes::UseProtoWire(const std::string& channel) const {
  if (!enabled_) {
    return true;
  }
  return impl_->proto_channels != nullptr &&
         std::regex_search(channel, *impl_->proto_channels);
}

size_t NativeTypes::size() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  return impl_->by_proto.size();
}

std::vector<std::string> NativeTypes::ProtoNames() {
  std::vector<std::string> names;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->LoadRegistries();
    names.reserve(impl_->by_proto.size());
    for (const auto& item : impl_->by_proto) {
      names.push_back(item.first);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::vector<NativeServiceInfo> NativeTypes::Services() {
  std::vector<NativeServiceInfo> services;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->LoadRegistries();
  for (const auto& service : impl_->services) {
    services.push_back(*service);
  }
  return services;
}

std::string StructureHash(const Descriptor* descriptor) {
  std::string canonical = descriptor->full_name();
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* f = descriptor->field(i);
    std::string type_name;
    if (f->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      type_name = f->message_type()->full_name();
    } else if (f->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      type_name = f->enum_type()->full_name();
    }
    canonical += "|" + std::to_string(f->number()) + "," +
                 std::to_string(static_cast<int>(f->type())) + "," +
                 std::to_string(static_cast<int>(f->label())) + "," + type_name;
  }
  uint64_t h = 0xcbf29ce484222325ull;   // FNV-1a
  for (unsigned char c : canonical) {
    h ^= c;
    h *= 0x100000001b3ull;
  }
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%016llx",
           static_cast<unsigned long long>(h));
  return buffer;
}

// ---------------------------------------------------------------------------
// MessageConverter
// ---------------------------------------------------------------------------

struct MessageConverter::Impl {
  enum Kind { SCALAR, STRING, BYTES, MESSAGE, RECURSIVE };

  struct Op {
    Kind kind = SCALAR;
    const FieldDescriptor* field = nullptr;
    const MessageMember* member = nullptr;
    int bit = -1;
    bool repeated = false;
    bool in_oneof = false;
    const Impl* nested = nullptr;                 // MESSAGE
    const MessageMember* wrap_type = nullptr;     // RECURSIVE (ProtoMessage)
    const MessageMember* wrap_data = nullptr;
  };

  const Descriptor* descriptor = nullptr;
  std::string ros_type;
  const MessageMembers* members = nullptr;
  const rosidl_message_type_support_t* ts_cpp = nullptr;
  std::vector<Op> ops;
  std::vector<const MessageMember*> has_words;

  static const uint8_t* At(const void* ros, const MessageMember* m) {
    return static_cast<const uint8_t*>(ros) + m->offset_;
  }
  static uint8_t* At(void* ros, const MessageMember* m) {
    return static_cast<uint8_t*>(ros) + m->offset_;
  }

  /// True when a default-initialized ROS 2 message already holds the
  /// protobuf defaults of every field, so an absent sub-message need not be
  /// written at all.
  bool defaults_match = false;

  /// A default-constructed ROS 2 message of this type.
  std::shared_ptr<void> prototype;

  /// True when the ROS 2 message carries no information: no presence bit is
  /// set, every sequence is empty and every singular field equals its
  /// protobuf default or, when `reference` (a default-constructed ROS 2
  /// message) is given, the value it has there.
  bool IsPristine(const void* ros, const void* reference = nullptr) const;

  template <typename T>
  static bool Same(const void* p, const void* reference, T proto_default) {
    const T v = *reinterpret_cast<const T*>(p);
    const T d = reference != nullptr ? *reinterpret_cast<const T*>(reference)
                                     : proto_default;
    return v == d || (v != v && d != d);   // NaN equals NaN
  }

  /// True when the member at `op` still has the value of a
  /// default-constructed ROS 2 message.
  bool Untouched(const Op& op, const void* ros) const;

  void ToRos(const Message& in, void* ros) const;
  bool FromRos(const void* ros, Message* out) const;

  template <typename ProtoT, typename RosT>
  static void RepeatedToRos(const Reflection* r, const Message& in,
                            const FieldDescriptor* f, void* p) {
    auto ref = r->GetRepeatedFieldRef<ProtoT>(in, f);
    auto* v = static_cast<std::vector<RosT>*>(p);
    v->clear();
    v->reserve(static_cast<size_t>(ref.size()));
    for (int i = 0; i < ref.size(); ++i) {
      v->push_back(static_cast<RosT>(ref.Get(i)));
    }
  }

  template <typename ProtoT, typename RosT>
  static void RepeatedFromRos(const Reflection* r, Message* out,
                              const FieldDescriptor* f, const void* p) {
    const auto* v = static_cast<const std::vector<RosT>*>(p);
    auto ref = r->GetMutableRepeatedFieldRef<ProtoT>(out, f);
    for (size_t i = 0; i < v->size(); ++i) {
      ref.Add(static_cast<ProtoT>((*v)[i]));
    }
  }

  static void WrapToRos(const Op& op, const Message& sub, void* wrapper) {
    *reinterpret_cast<std::string*>(At(wrapper, op.wrap_type)) =
        sub.GetDescriptor()->full_name();
    std::string bytes;
    sub.SerializePartialToString(&bytes);
    reinterpret_cast<std::vector<uint8_t>*>(At(wrapper, op.wrap_data))
        ->assign(bytes.begin(), bytes.end());
  }

  static const std::vector<uint8_t>& WrapData(const Op& op,
                                              const void* wrapper) {
    return *reinterpret_cast<const std::vector<uint8_t>*>(
        At(wrapper, op.wrap_data));
  }
};

void MessageConverter::Impl::ToRos(const Message& in, void* ros) const {
  const Reflection* r = in.GetReflection();
  uint64_t inline_words[4] = {0, 0, 0, 0};
  std::vector<uint64_t> heap_words;
  uint64_t* words = inline_words;
  if (has_words.size() > 4) {
    heap_words.assign(has_words.size(), 0);
    words = heap_words.data();
  }
  std::string scratch;
  for (const Op& op : ops) {
    const FieldDescriptor* f = op.field;
    void* p = At(ros, op.member);
    if (op.repeated) {
      switch (op.kind) {
        case SCALAR:
          switch (f->cpp_type()) {
            case FieldDescriptor::CPPTYPE_DOUBLE:
              RepeatedToRos<double, double>(r, in, f, p);
              break;
            case FieldDescriptor::CPPTYPE_FLOAT:
              RepeatedToRos<float, float>(r, in, f, p);
              break;
            case FieldDescriptor::CPPTYPE_INT32:
            case FieldDescriptor::CPPTYPE_ENUM:
              RepeatedToRos<int32_t, int32_t>(r, in, f, p);
              break;
            case FieldDescriptor::CPPTYPE_INT64:
              RepeatedToRos<int64_t, int64_t>(r, in, f, p);
              break;
            case FieldDescriptor::CPPTYPE_UINT32:
              RepeatedToRos<uint32_t, uint32_t>(r, in, f, p);
              break;
            case FieldDescriptor::CPPTYPE_UINT64:
              RepeatedToRos<uint64_t, uint64_t>(r, in, f, p);
              break;
            case FieldDescriptor::CPPTYPE_BOOL:
              RepeatedToRos<bool, bool>(r, in, f, p);
              break;
            default:
              break;
          }
          break;
        case STRING: {
          auto* v = static_cast<std::vector<std::string>*>(p);
          const int n = r->FieldSize(in, f);
          v->clear();
          v->reserve(static_cast<size_t>(n));
          for (int i = 0; i < n; ++i) {
            v->push_back(r->GetRepeatedStringReference(in, f, i, &scratch));
          }
          break;
        }
        case MESSAGE: {
          const int n = r->FieldSize(in, f);
          op.member->resize_function(p, static_cast<size_t>(n));
          for (int i = 0; i < n; ++i) {
            op.nested->ToRos(r->GetRepeatedMessage(in, f, i),
                             op.member->get_function(p, static_cast<size_t>(i)));
          }
          break;
        }
        case RECURSIVE: {
          const int n = r->FieldSize(in, f);
          op.member->resize_function(p, static_cast<size_t>(n));
          for (int i = 0; i < n; ++i) {
            WrapToRos(op, r->GetRepeatedMessage(in, f, i),
                      op.member->get_function(p, static_cast<size_t>(i)));
          }
          break;
        }
        case BYTES:
          break;   // repeated bytes are not generated
      }
      continue;
    }

    const bool has = r->HasField(in, f);
    if (op.bit >= 0 && has) {
      words[op.bit / 64] |= 1ull << (op.bit % 64);
    }
    switch (op.kind) {
      case SCALAR:
        switch (f->cpp_type()) {
          case FieldDescriptor::CPPTYPE_DOUBLE:
            *reinterpret_cast<double*>(p) = r->GetDouble(in, f);
            break;
          case FieldDescriptor::CPPTYPE_FLOAT:
            *reinterpret_cast<float*>(p) = r->GetFloat(in, f);
            break;
          case FieldDescriptor::CPPTYPE_INT32:
            *reinterpret_cast<int32_t*>(p) = r->GetInt32(in, f);
            break;
          case FieldDescriptor::CPPTYPE_ENUM:
            *reinterpret_cast<int32_t*>(p) = r->GetEnumValue(in, f);
            break;
          case FieldDescriptor::CPPTYPE_INT64:
            *reinterpret_cast<int64_t*>(p) = r->GetInt64(in, f);
            break;
          case FieldDescriptor::CPPTYPE_UINT32:
            *reinterpret_cast<uint32_t*>(p) = r->GetUInt32(in, f);
            break;
          case FieldDescriptor::CPPTYPE_UINT64:
            *reinterpret_cast<uint64_t*>(p) = r->GetUInt64(in, f);
            break;
          case FieldDescriptor::CPPTYPE_BOOL:
            *reinterpret_cast<bool*>(p) = r->GetBool(in, f);
            break;
          default:
            break;
        }
        break;
      case STRING:
        *reinterpret_cast<std::string*>(p) =
            r->GetStringReference(in, f, &scratch);
        break;
      case BYTES: {
        const std::string& bytes = r->GetStringReference(in, f, &scratch);
        reinterpret_cast<std::vector<uint8_t>*>(p)->assign(bytes.begin(),
                                                           bytes.end());
        break;
      }
      case MESSAGE:
        // An absent sub-message reads as its protobuf defaults, which is
        // what a default-initialized ROS 2 message normally holds already.
        if (has || !op.nested->defaults_match) {
          op.nested->ToRos(r->GetMessage(in, f), p);
        }
        break;
      case RECURSIVE:
        if (has) {
          WrapToRos(op, r->GetMessage(in, f), p);
        }
        break;
    }
  }
  for (size_t w = 0; w < has_words.size(); ++w) {
    if (has_words[w] != nullptr) {
      *reinterpret_cast<uint64_t*>(At(ros, has_words[w])) = words[w];
    }
  }
}

bool MessageConverter::Impl::FromRos(const void* ros, Message* out) const {
  const Reflection* r = out->GetReflection();
  bool ok = true;
  auto bit_set = [&](int bit) -> bool {
    if (bit < 0) {
      return false;
    }
    const size_t w = static_cast<size_t>(bit / 64);
    if (w >= has_words.size() || has_words[w] == nullptr) {
      return false;
    }
    return (((*reinterpret_cast<const uint64_t*>(At(ros, has_words[w]))) >>
             (bit % 64)) & 1ull) != 0;
  };
  for (const Op& op : ops) {
    const FieldDescriptor* f = op.field;
    const void* p = At(ros, op.member);
    if (op.repeated) {
      switch (op.kind) {
        case SCALAR:
          switch (f->cpp_type()) {
            case FieldDescriptor::CPPTYPE_DOUBLE:
              RepeatedFromRos<double, double>(r, out, f, p);
              break;
            case FieldDescriptor::CPPTYPE_FLOAT:
              RepeatedFromRos<float, float>(r, out, f, p);
              break;
            case FieldDescriptor::CPPTYPE_INT32:
              RepeatedFromRos<int32_t, int32_t>(r, out, f, p);
              break;
            case FieldDescriptor::CPPTYPE_ENUM: {
              const auto* v = static_cast<const std::vector<int32_t>*>(p);
              for (int32_t x : *v) {
                r->AddEnumValue(out, f, x);
              }
              break;
            }
            case FieldDescriptor::CPPTYPE_INT64:
              RepeatedFromRos<int64_t, int64_t>(r, out, f, p);
              break;
            case FieldDescriptor::CPPTYPE_UINT32:
              RepeatedFromRos<uint32_t, uint32_t>(r, out, f, p);
              break;
            case FieldDescriptor::CPPTYPE_UINT64:
              RepeatedFromRos<uint64_t, uint64_t>(r, out, f, p);
              break;
            case FieldDescriptor::CPPTYPE_BOOL:
              RepeatedFromRos<bool, bool>(r, out, f, p);
              break;
            default:
              break;
          }
          break;
        case STRING: {
          const auto* v = static_cast<const std::vector<std::string>*>(p);
          for (const auto& s : *v) {
            r->AddString(out, f, s);
          }
          break;
        }
        case MESSAGE: {
          const size_t n = op.member->size_function(p);
          for (size_t i = 0; i < n; ++i) {
            ok = op.nested->FromRos(op.member->get_const_function(p, i),
                                    r->AddMessage(out, f)) && ok;
          }
          break;
        }
        case RECURSIVE: {
          const size_t n = op.member->size_function(p);
          for (size_t i = 0; i < n; ++i) {
            const auto& data =
                WrapData(op, op.member->get_const_function(p, i));
            ok = r->AddMessage(out, f)->ParsePartialFromArray(
                     data.data(), static_cast<int>(data.size())) && ok;
          }
          break;
        }
        case BYTES:
          break;
      }
      continue;
    }

    // A singular field is present when its bit is set, when presence is not
    // tracked (required fields), or when it differs from the protobuf
    // default. A oneof member must also differ from what a newly constructed
    // ROS 2 message holds: the two differ where a default cannot be written
    // in a .msg file (NaN), and members the publisher never touched must not
    // select the oneof.
    const bool has = op.bit < 0 || bit_set(op.bit);
    if (!has && op.in_oneof && Untouched(op, ros)) {
      continue;
    }
    switch (op.kind) {
      case SCALAR:
        switch (f->cpp_type()) {
          case FieldDescriptor::CPPTYPE_DOUBLE: {
            const double v = *reinterpret_cast<const double*>(p);
            if (has || !SameDouble(v, f->default_value_double())) {
              r->SetDouble(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_FLOAT: {
            const float v = *reinterpret_cast<const float*>(p);
            if (has || !SameDouble(v, f->default_value_float())) {
              r->SetFloat(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_INT32: {
            const int32_t v = *reinterpret_cast<const int32_t*>(p);
            if (has || v != f->default_value_int32()) {
              r->SetInt32(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_ENUM: {
            const int32_t v = *reinterpret_cast<const int32_t*>(p);
            if (has || v != f->default_value_enum()->number()) {
              r->SetEnumValue(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_INT64: {
            const int64_t v = *reinterpret_cast<const int64_t*>(p);
            if (has || v != f->default_value_int64()) {
              r->SetInt64(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_UINT32: {
            const uint32_t v = *reinterpret_cast<const uint32_t*>(p);
            if (has || v != f->default_value_uint32()) {
              r->SetUInt32(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_UINT64: {
            const uint64_t v = *reinterpret_cast<const uint64_t*>(p);
            if (has || v != f->default_value_uint64()) {
              r->SetUInt64(out, f, v);
            }
            break;
          }
          case FieldDescriptor::CPPTYPE_BOOL: {
            const bool v = *reinterpret_cast<const bool*>(p);
            if (has || v != f->default_value_bool()) {
              r->SetBool(out, f, v);
            }
            break;
          }
          default:
            break;
        }
        break;
      case STRING: {
        const std::string& s = *reinterpret_cast<const std::string*>(p);
        if (has || s != f->default_value_string()) {
          r->SetString(out, f, s);
        }
        break;
      }
      case BYTES: {
        const auto& v = *reinterpret_cast<const std::vector<uint8_t>*>(p);
        std::string s(v.begin(), v.end());
        if (has || s != f->default_value_string()) {
          r->SetString(out, f, std::move(s));
        }
        break;
      }
      case MESSAGE:
        // Without a presence bit the sub-message is present when it carries
        // anything (a plain ROS 2 publisher filled it in).
        if (has || !op.nested->IsPristine(p)) {
          ok = op.nested->FromRos(p, r->MutableMessage(out, f)) && ok;
        }
        break;
      case RECURSIVE: {
        const auto& data = WrapData(op, p);
        const bool present = op.bit >= 0 ? (has || !data.empty())
                                         : (f->is_required() || !data.empty());
        if (present) {
          ok = r->MutableMessage(out, f)->ParsePartialFromArray(
                   data.data(), static_cast<int>(data.size())) && ok;
        }
        break;
      }
    }
  }
  return ok;
}

bool MessageConverter::Impl::IsPristine(const void* ros,
                                        const void* reference) const {
  for (const MessageMember* m : has_words) {
    if (m != nullptr && *reinterpret_cast<const uint64_t*>(At(ros, m)) != 0) {
      return false;
    }
  }
  for (const Op& op : ops) {
    const FieldDescriptor* f = op.field;
    const void* p = At(ros, op.member);
    const void* ref = reference == nullptr ? nullptr : At(reference, op.member);
    if (op.repeated) {
      if (op.member->size_function(p) != 0) {
        return false;
      }
      continue;
    }
    bool same = true;
    switch (op.kind) {
      case SCALAR:
        switch (f->cpp_type()) {
          case FieldDescriptor::CPPTYPE_DOUBLE:
            same = Same<double>(p, ref, f->default_value_double());
            break;
          case FieldDescriptor::CPPTYPE_FLOAT:
            same = Same<float>(p, ref, f->default_value_float());
            break;
          case FieldDescriptor::CPPTYPE_INT32:
            same = Same<int32_t>(p, ref, f->default_value_int32());
            break;
          case FieldDescriptor::CPPTYPE_ENUM:
            same = Same<int32_t>(p, ref, f->default_value_enum()->number());
            break;
          case FieldDescriptor::CPPTYPE_INT64:
            same = Same<int64_t>(p, ref, f->default_value_int64());
            break;
          case FieldDescriptor::CPPTYPE_UINT32:
            same = Same<uint32_t>(p, ref, f->default_value_uint32());
            break;
          case FieldDescriptor::CPPTYPE_UINT64:
            same = Same<uint64_t>(p, ref, f->default_value_uint64());
            break;
          case FieldDescriptor::CPPTYPE_BOOL:
            same = Same<bool>(p, ref, f->default_value_bool());
            break;
          default:
            break;
        }
        break;
      case STRING:
        same = *reinterpret_cast<const std::string*>(p) ==
               (ref != nullptr ? *reinterpret_cast<const std::string*>(ref)
                               : f->default_value_string());
        break;
      case BYTES: {
        const auto& v = *reinterpret_cast<const std::vector<uint8_t>*>(p);
        if (ref != nullptr) {
          same = v == *reinterpret_cast<const std::vector<uint8_t>*>(ref);
        } else {
          const std::string& d = f->default_value_string();
          same = v.size() == d.size() &&
                 (v.empty() || std::memcmp(v.data(), d.data(), v.size()) == 0);
        }
        break;
      }
      case MESSAGE:
        same = op.nested->IsPristine(p, ref);
        break;
      case RECURSIVE:
        same = WrapData(op, p).empty();
        break;
    }
    if (!same) {
      return false;
    }
  }
  return true;
}

bool MessageConverter::Impl::Untouched(const Op& op, const void* ros) const {
  const void* p = At(ros, op.member);
  const void* ref = At(prototype.get(), op.member);
  switch (op.kind) {
    case SCALAR:
      switch (op.field->cpp_type()) {
        case FieldDescriptor::CPPTYPE_DOUBLE:
          return Same<double>(p, ref, 0);
        case FieldDescriptor::CPPTYPE_FLOAT:
          return Same<float>(p, ref, 0);
        case FieldDescriptor::CPPTYPE_INT32:
        case FieldDescriptor::CPPTYPE_ENUM:
          return Same<int32_t>(p, ref, 0);
        case FieldDescriptor::CPPTYPE_INT64:
          return Same<int64_t>(p, ref, 0);
        case FieldDescriptor::CPPTYPE_UINT32:
          return Same<uint32_t>(p, ref, 0);
        case FieldDescriptor::CPPTYPE_UINT64:
          return Same<uint64_t>(p, ref, 0);
        case FieldDescriptor::CPPTYPE_BOOL:
          return Same<bool>(p, ref, false);
        default:
          return true;
      }
    case STRING:
      return *reinterpret_cast<const std::string*>(p) ==
             *reinterpret_cast<const std::string*>(ref);
    case BYTES:
      return *reinterpret_cast<const std::vector<uint8_t>*>(p) ==
             *reinterpret_cast<const std::vector<uint8_t>*>(ref);
    case MESSAGE:
      return op.nested->IsPristine(p, ref);
    case RECURSIVE:
      return WrapData(op, p).empty();
  }
  return true;
}

struct ConverterBuilder {
  using Key = std::pair<const Descriptor*, const MessageMembers*>;

  static std::map<Key, std::shared_ptr<MessageConverter>>& Cache() {
    static auto* cache = new std::map<Key, std::shared_ptr<MessageConverter>>();
    return *cache;
  }

  /// Builds the converter of `d` for the ROS 2 struct described by
  /// `members`. The caller holds ConverterMutex().
  static std::shared_ptr<MessageConverter> Build(const Descriptor* d,
                                                 const MessageMembers* members,
                                                 std::string* error) {
    const Key key(d, members);
    auto it = Cache().find(key);
    if (it != Cache().end()) {
      if (it->second == nullptr) {
        *error = d->full_name() + " has no usable native type";
      }
      return it->second;
    }
    auto converter = BuildUncached(d, members, error);
    Cache()[key] = converter;
    return converter;
  }

  static bool ScalarMatches(const FieldDescriptor* f, uint8_t type_id) {
    using namespace rosidl_typesupport_introspection_cpp;  // NOLINT
    switch (f->cpp_type()) {
      case FieldDescriptor::CPPTYPE_DOUBLE:
        return type_id == ROS_TYPE_DOUBLE;
      case FieldDescriptor::CPPTYPE_FLOAT:
        return type_id == ROS_TYPE_FLOAT;
      case FieldDescriptor::CPPTYPE_INT32:
      case FieldDescriptor::CPPTYPE_ENUM:
        return type_id == ROS_TYPE_INT32;
      case FieldDescriptor::CPPTYPE_INT64:
        return type_id == ROS_TYPE_INT64;
      case FieldDescriptor::CPPTYPE_UINT32:
        return type_id == ROS_TYPE_UINT32;
      case FieldDescriptor::CPPTYPE_UINT64:
        return type_id == ROS_TYPE_UINT64;
      case FieldDescriptor::CPPTYPE_BOOL:
        return type_id == ROS_TYPE_BOOLEAN;
      default:
        return false;
    }
  }

  static std::shared_ptr<MessageConverter> BuildUncached(
      const Descriptor* d, const MessageMembers* members, std::string* error) {
    using namespace rosidl_typesupport_introspection_cpp;  // NOLINT
    const NativeTypeInfo* info =
        NativeTypes::Instance()->FindByProtoName(d->full_name());
    if (info == nullptr) {
      *error = d->full_name() + " is not in the native type registry";
      return nullptr;
    }
    if (StructureHash(d) != info->hash) {
      *error = "the definition of " + d->full_name() +
               " in this process differs from the one " + info->ros_type +
               " was generated from";
      return nullptr;
    }
    if (members == nullptr) {
      *error = "no introspection data for " + info->ros_type;
      return nullptr;
    }
    const size_t expected = info->members.size();
    if (!(members->member_count_ == expected ||
          (expected == 0 && members->member_count_ == 1))) {
      *error = info->ros_type + " has " +
               std::to_string(members->member_count_) + " members, registry " +
               std::to_string(expected);
      return nullptr;
    }
    std::shared_ptr<MessageConverter> converter(new MessageConverter());
    auto* impl = converter->impl_.get();
    impl->descriptor = d;
    impl->ros_type = info->ros_type;
    impl->members = members;
    for (size_t i = 0; i < expected; ++i) {
      const NativeMember& nm = info->members[i];
      const MessageMember& mm = members->members_[i];
      if (nm.ros_name != mm.name_) {
        *error = info->ros_type + ": member " + mm.name_ + " != " + nm.ros_name;
        return nullptr;
      }
      if (nm.kind == 'h') {
        if (mm.type_id_ != ROS_TYPE_UINT64 || mm.is_array_ || nm.number < 0) {
          *error = info->ros_type + ": bad presence member " + nm.ros_name;
          return nullptr;
        }
        if (impl->has_words.size() <= static_cast<size_t>(nm.number)) {
          impl->has_words.resize(static_cast<size_t>(nm.number) + 1, nullptr);
        }
        impl->has_words[static_cast<size_t>(nm.number)] = &mm;
        continue;
      }
      const FieldDescriptor* f = d->FindFieldByNumber(nm.number);
      if (f == nullptr) {
        *error = d->full_name() + " has no field " + std::to_string(nm.number);
        return nullptr;
      }
      MessageConverter::Impl::Op op;
      op.field = f;
      op.member = &mm;
      op.bit = nm.bit;
      op.repeated = f->is_repeated();
      op.in_oneof = f->real_containing_oneof() != nullptr;
      bool type_ok = false;
      if (nm.kind == 'r') {
        op.kind = MessageConverter::Impl::RECURSIVE;
        const MessageMembers* wrapper =
            mm.type_id_ == ROS_TYPE_MESSAGE ? MembersOf(mm.members_) : nullptr;
        if (wrapper != nullptr) {
          for (uint32_t k = 0; k < wrapper->member_count_; ++k) {
            const MessageMember& wm = wrapper->members_[k];
            if (std::string(wm.name_) == "type" &&
                wm.type_id_ == ROS_TYPE_STRING) {
              op.wrap_type = &wm;
            } else if (std::string(wm.name_) == "data" && wm.is_array_ &&
                       (wm.type_id_ == ROS_TYPE_UINT8 ||
                        wm.type_id_ == ROS_TYPE_OCTET)) {
              op.wrap_data = &wm;
            }
          }
        }
        type_ok = f->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
                  op.wrap_type != nullptr && op.wrap_data != nullptr &&
                  mm.is_array_ == op.repeated;
      } else if (f->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
        op.kind = MessageConverter::Impl::MESSAGE;
        if (mm.type_id_ == ROS_TYPE_MESSAGE && mm.is_array_ == op.repeated) {
          auto nested = Build(f->message_type(), MembersOf(mm.members_), error);
          if (nested == nullptr) {
            return nullptr;
          }
          op.nested = nested->impl_.get();
          type_ok = true;
        }
      } else if (f->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
        if (f->type() == FieldDescriptor::TYPE_BYTES) {
          op.kind = MessageConverter::Impl::BYTES;
          type_ok = !op.repeated && mm.is_array_ &&
                    (mm.type_id_ == ROS_TYPE_UINT8 ||
                     mm.type_id_ == ROS_TYPE_OCTET);
        } else {
          op.kind = MessageConverter::Impl::STRING;
          type_ok = mm.type_id_ == ROS_TYPE_STRING &&
                    mm.is_array_ == op.repeated;
        }
      } else {
        op.kind = MessageConverter::Impl::SCALAR;
        type_ok = ScalarMatches(f, mm.type_id_) && mm.is_array_ == op.repeated;
      }
      if (!type_ok || (mm.is_array_ && mm.array_size_ != 0 &&
                       !mm.is_upper_bound_)) {
        *error = info->ros_type + ": member " + nm.ros_name +
                 " does not match field " + f->full_name();
        return nullptr;
      }
      impl->ops.push_back(op);
    }
    // Does a default-initialized ROS 2 message hold the protobuf defaults?
    // (Not when a default cannot be written in a .msg file, e.g. inf.)
    void* memory = ::operator new(members->size_of_);
    members->init_function(memory,
                           rosidl_runtime_cpp::MessageInitialization::ALL);
    impl->prototype = std::shared_ptr<void>(memory, [members](void* p) {
      members->fini_function(p);
      ::operator delete(p);
    });
    impl->defaults_match = impl->IsPristine(memory);
    return converter;
  }
};

MessageConverter::MessageConverter() : impl_(new Impl()) {}

MessageConverter::~MessageConverter() = default;

std::shared_ptr<const MessageConverter> MessageConverter::Get(
    const Descriptor* descriptor) {
  if (descriptor == nullptr) {
    return nullptr;
  }
  static auto* top =
      new std::unordered_map<const Descriptor*,
                             std::shared_ptr<MessageConverter>>();
  std::lock_guard<std::recursive_mutex> lock(ConverterMutex());
  auto it = top->find(descriptor);
  if (it != top->end()) {
    return it->second;
  }
  std::shared_ptr<MessageConverter> converter;
  const NativeTypeInfo* info =
      NativeTypes::Instance()->FindByProtoName(descriptor->full_name());
  if (info != nullptr && NativeTypes::Instance()->enabled()) {
    try {
      auto introspection_lib =
          rclcpp::get_typesupport_library(info->ros_type, kIntrospection);
      auto cpp_lib =
          rclcpp::get_typesupport_library(info->ros_type, kTypesupportCpp);
      const auto* introspection = rclcpp::get_message_typesupport_handle(
          info->ros_type, kIntrospection, *introspection_lib);
      const auto* ts_cpp = rclcpp::get_message_typesupport_handle(
          info->ros_type, kTypesupportCpp, *cpp_lib);
      LoadedLibraries().push_back(introspection_lib);
      LoadedLibraries().push_back(cpp_lib);
      std::string error;
      converter =
          ConverterBuilder::Build(descriptor, MembersOf(introspection), &error);
      if (converter == nullptr) {
        AWARN << "no native ROS 2 type for " << descriptor->full_name() << ": "
              << error << "; using apollo_msgs/msg/ProtoMessage";
      } else {
        converter->impl_->ts_cpp = ts_cpp;
      }
    } catch (const std::exception& e) {
      AWARN << "cannot load " << info->ros_type << " (" << e.what()
            << "); using apollo_msgs/msg/ProtoMessage for "
            << descriptor->full_name();
      converter = nullptr;
    }
  }
  (*top)[descriptor] = converter;
  return converter;
}

bool MessageConverter::GetForService(
    const NativeServiceInfo& service,
    std::shared_ptr<const MessageConverter>* request,
    std::shared_ptr<const MessageConverter>* response) {
  auto* types = NativeTypes::Instance();
  if (!types->enabled()) {
    return false;
  }
  const Descriptor* request_descriptor = types->FindDescriptor(service.request);
  const Descriptor* response_descriptor =
      types->FindDescriptor(service.response);
  if (request_descriptor == nullptr || response_descriptor == nullptr) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(ConverterMutex());
  try {
    auto lib = rclcpp::get_typesupport_library(service.ros_type, kIntrospection);
    const auto* ts = rclcpp::get_service_typesupport_handle(
        service.ros_type, kIntrospection, *lib);
    LoadedLibraries().push_back(lib);
    const auto* members =
        static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers*>(
            ts->data);
    std::string error;
    auto req = ConverterBuilder::Build(request_descriptor,
                                       members->request_members_, &error);
    auto resp = req == nullptr
                    ? nullptr
                    : ConverterBuilder::Build(response_descriptor,
                                              members->response_members_,
                                              &error);
    if (req == nullptr || resp == nullptr) {
      AWARN << "no native ROS 2 service type for " << service.request << ": "
            << error << "; using apollo_msgs/srv/ProtoService";
      return false;
    }
    *request = req;
    *response = resp;
    return true;
  } catch (const std::exception& e) {
    AWARN << "cannot load " << service.ros_type << " (" << e.what()
          << "); using apollo_msgs/srv/ProtoService";
    return false;
  }
}

const std::string& MessageConverter::ros_type() const {
  return impl_->ros_type;
}

const Descriptor* MessageConverter::descriptor() const {
  return impl_->descriptor;
}

bool MessageConverter::DefaultsMatch() const { return impl_->defaults_match; }

std::shared_ptr<void> MessageConverter::CreateRos() const {
  const MessageMembers* m = impl_->members;
  void* memory = ::operator new(m->size_of_);
  m->init_function(memory, rosidl_runtime_cpp::MessageInitialization::ALL);
  return std::shared_ptr<void>(memory, [m](void* p) {
    m->fini_function(p);
    ::operator delete(p);
  });
}

void MessageConverter::ToRos(const Message& in, void* ros) const {
  impl_->ToRos(in, ros);
}

bool MessageConverter::FromRos(const void* ros, Message* out) const {
  return impl_->FromRos(ros, out);
}

bool MessageConverter::ToSerialized(const Message& in,
                                    rclcpp::SerializedMessage* out) const {
  if (impl_->ts_cpp == nullptr) {
    return false;
  }
  try {
    auto ros = CreateRos();
    impl_->ToRos(in, ros.get());
    rclcpp::SerializationBase serialization(impl_->ts_cpp);
    serialization.serialize_message(ros.get(), out);
    return true;
  } catch (const std::exception& e) {
    AERROR << "serializing " << impl_->ros_type << " failed: " << e.what();
    return false;
  }
}

bool MessageConverter::FromSerialized(const rclcpp::SerializedMessage& in,
                                      Message* out) const {
  if (impl_->ts_cpp == nullptr) {
    return false;
  }
  try {
    auto ros = CreateRos();
    rclcpp::SerializationBase serialization(impl_->ts_cpp);
    serialization.deserialize_message(&in, ros.get());
    out->Clear();
    return impl_->FromRos(ros.get(), out);
  } catch (const std::exception& e) {
    AERROR << "deserializing " << impl_->ros_type << " failed: " << e.what();
    return false;
  }
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
