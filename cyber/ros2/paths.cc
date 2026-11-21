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

#include "cyber/ros2/paths.h"

#include <sys/stat.h>

#include <cstdlib>

namespace apollo {
namespace cyber {
namespace ros2 {

namespace {

bool Exists(const std::string& path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0;
}

std::vector<std::string> Split(const std::string& value) {
  std::vector<std::string> parts;
  size_t begin = 0;
  while (begin <= value.size()) {
    size_t end = value.find(':', begin);
    if (end == std::string::npos) {
      end = value.size();
    }
    if (end > begin) {
      parts.push_back(value.substr(begin, end - begin));
    }
    begin = end + 1;
  }
  return parts;
}

std::string Join(const std::vector<std::string>& parts) {
  std::string result;
  for (const auto& p : parts) {
    if (!result.empty()) {
      result += ':';
    }
    result += p;
  }
  return result;
}

void SetIfUnset(const char* name, const std::vector<std::string>& values) {
  if (std::getenv(name) != nullptr || values.empty()) {
    return;
  }
  setenv(name, Join(values).c_str(), 0);
}

std::vector<std::string> Collect(const std::vector<std::string>& prefixes,
                                 const std::string& suffix) {
  std::vector<std::string> result;
  for (const auto& prefix : prefixes) {
    std::string dir = prefix + suffix;
    if (Exists(dir)) {
      result.push_back(dir);
    }
  }
  return result;
}

const bool g_paths_initialized = (SetDefaultSearchPaths(), true);

}  // namespace

std::vector<std::string> AmentPrefixes() {
  const char* env = std::getenv("AMENT_PREFIX_PATH");
  return env == nullptr ? std::vector<std::string>() : Split(env);
}

void SetDefaultSearchPaths() {
  (void)g_paths_initialized;
  const auto prefixes = AmentPrefixes();

  for (const auto& prefix : prefixes) {
    if (std::getenv("CYBER_PATH") != nullptr) {
      break;
    }
    std::string dir = prefix + "/share/cyber";
    if (Exists(dir + "/conf/cyber.pb.conf")) {
      setenv("CYBER_PATH", dir.c_str(), 0);
    }
  }

  SetIfUnset("APOLLO_PLUGIN_INDEX_PATH",
             Collect(prefixes, "/share/cyber_plugin_index"));
  SetIfUnset("APOLLO_PLUGIN_DESCRIPTION_PATH",
             Collect(prefixes, "/share/apollo"));
  SetIfUnset("APOLLO_PLUGIN_LIB_PATH", Collect(prefixes, "/lib/apollo"));

  auto libs = Collect(prefixes, "/lib/apollo");
  auto plain_libs = Collect(prefixes, "/lib");
  libs.insert(libs.end(), plain_libs.begin(), plain_libs.end());
  SetIfUnset("APOLLO_LIB_PATH", libs);

  std::vector<std::string> conf;
  const char* root = std::getenv("APOLLO_ROOT");
  if (root != nullptr && Exists(root)) {
    conf.push_back(root);
  }
  auto shares = Collect(prefixes, "/share/apollo");
  conf.insert(conf.end(), shares.begin(), shares.end());
  SetIfUnset("APOLLO_CONF_PATH", conf);
  SetIfUnset("APOLLO_FLAG_PATH", conf);
  SetIfUnset("APOLLO_DAG_PATH", conf);
}

std::string ResolveApolloPath(const std::string& path) {
  static const std::string kApolloPrefix = "/apollo/";
  if (path.compare(0, kApolloPrefix.size(), kApolloPrefix) != 0 ||
      Exists(path)) {
    return path;
  }
  const std::string rest = path.substr(kApolloPrefix.size());
  const char* root = std::getenv("APOLLO_ROOT");
  if (root != nullptr && *root != '\0') {
    std::string candidate = std::string(root) + "/" + rest;
    if (Exists(candidate)) {
      return candidate;
    }
  }
  for (const auto& prefix : AmentPrefixes()) {
    std::string candidate = prefix + "/share/apollo/" + rest;
    if (Exists(candidate)) {
      return candidate;
    }
  }
  return path;
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
