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
 * A map tool that converts a Lanelet2 map (.osm) to an Apollo base map:
 *
 *   lanelet2_map_converter --osm_file=map.osm --output_dir=/path/to/map_dir
 *
 * writes base_map.bin and base_map.txt. The --lanelet2_* flags select the
 * projection and the conversion rules, see docs/ros2/lanelet2.md.
 * tools/ros2/lanelet2/lanelet2_to_apollo.py wraps this tool, topo_creator
 * and sim_map_generator to produce a complete map directory.
 */

#include <fstream>
#include <iostream>

#include "gflags/gflags.h"

#include "modules/common_msgs/map_msgs/map.pb.h"

#include "cyber/common/file.h"
#include "cyber/common/log.h"
#include "modules/map/hdmap/adapter/lanelet2_adapter.h"
#include "modules/map/hdmap/hdmap_impl.h"

DEFINE_string(osm_file, "", "Lanelet2 map to convert (OSM XML).");
DEFINE_string(output_dir, "", "Directory for base_map.bin and base_map.txt.");
DEFINE_string(report_file, "",
              "If set, the conversion report is written here as JSON.");
DEFINE_bool(check_map, true,
            "Load the converted map into the HD map engine before writing "
            "it, which checks what the Apollo modules require of a map.");

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_alsologtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_osm_file.empty() || FLAGS_output_dir.empty()) {
    std::cerr << "usage: " << argv[0]
              << " --osm_file=<map.osm> --output_dir=<dir> "
                 "[--report_file=<json>] [--lanelet2_...]\n";
    return 2;
  }

  apollo::hdmap::Map pb_map;
  apollo::hdmap::adapter::Lanelet2Report report;
  const bool converted = apollo::hdmap::adapter::Lanelet2Adapter::LoadData(
      FLAGS_osm_file, apollo::hdmap::adapter::Lanelet2Options::FromFlags(),
      &pb_map, &report);
  if (!FLAGS_report_file.empty()) {
    std::ofstream out(FLAGS_report_file);
    out << report.ToJson();
  }
  if (!converted) {
    std::cerr << "failed to convert " << FLAGS_osm_file << "\n"
              << report.Summary() << std::endl;
    return 1;
  }

  if (FLAGS_check_map) {
    apollo::hdmap::HDMapImpl hdmap;
    if (hdmap.LoadMapFromProto(pb_map) != 0) {
      std::cerr << "the HD map engine rejects the converted map" << std::endl;
      return 1;
    }
  }

  if (!apollo::cyber::common::EnsureDirectory(FLAGS_output_dir)) {
    std::cerr << "cannot create " << FLAGS_output_dir << std::endl;
    return 1;
  }
  const std::string txt_file = FLAGS_output_dir + "/base_map.txt";
  const std::string bin_file = FLAGS_output_dir + "/base_map.bin";
  if (!apollo::cyber::common::SetProtoToASCIIFile(pb_map, txt_file) ||
      !apollo::cyber::common::SetProtoToBinaryFile(pb_map, bin_file)) {
    std::cerr << "cannot write the base map to " << FLAGS_output_dir
              << std::endl;
    return 1;
  }
  std::cout << report.Summary() << "\nwrote " << bin_file << " and " << txt_file
            << std::endl;
  return 0;
}
