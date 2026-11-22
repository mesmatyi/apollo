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

#include "cyber/ros2/naming.h"

#include <cctype>
#include <vector>

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

bool IsAsciiAlnum(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

bool IsAsciiDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::string ChannelToTopic(const std::string& channel) {
  if (channel.empty()) {
    return channel;
  }
  std::vector<std::string> tokens;
  std::string token;
  for (char c : channel) {
    if (c == '/') {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
      continue;
    }
    // One '_' per UTF-8 code point (skip continuation bytes), matching the
    // per-character rule of the Python implementation.
    if ((static_cast<unsigned char>(c) & 0xC0) == 0x80) {
      continue;
    }
    token.push_back((IsAsciiAlnum(c) || c == '_') ? c : '_');
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  std::string result;
  for (auto& t : tokens) {
    result.push_back('/');
    if (IsAsciiDigit(t.front())) {
      result.push_back('_');
    }
    result += t;
  }
  if (result.empty()) {
    result = "/";
  }
  // ROS 2 reserves these names for standard message types; Apollo channels
  // with the same names carry protobuf payloads and move under /apollo.
  static const char* kReserved[] = {"/tf", "/tf_static", "/clock", "/rosout",
                                    "/parameter_events"};
  for (const char* reserved : kReserved) {
    if (result == reserved) {
      return "/apollo" + result;
    }
  }
  return result;
}

std::string ToNodeName(const std::string& name) {
  if (name.empty()) {
    return "apollo_node";
  }
  std::string result;
  result.reserve(name.size() + 1);
  for (char c : name) {
    if ((static_cast<unsigned char>(c) & 0xC0) == 0x80) {
      continue;  // UTF-8 continuation byte
    }
    result.push_back((IsAsciiAlnum(c) || c == '_') ? c : '_');
  }
  if (IsAsciiDigit(result.front())) {
    result.insert(result.begin(), '_');
  }
  return result;
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
