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
 * @file conversions.h
 * @brief Pure conversion functions between Apollo protobuf messages and
 * standard ROS 2 messages.
 */

#ifndef APOLLO_ROS2_BRIDGE_CONVERSIONS_H_
#define APOLLO_ROS2_BRIDGE_CONVERSIONS_H_

#include <string>
#include <vector>

#include "builtin_interfaces/msg/time.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/path.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "tf2_msgs/msg/tf_message.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

#include "modules/common_msgs/localization_msgs/imu.pb.h"
#include "modules/common_msgs/localization_msgs/localization.pb.h"
#include "modules/common_msgs/perception_msgs/perception_obstacle.pb.h"
#include "modules/common_msgs/planning_msgs/planning.pb.h"
#include "modules/common_msgs/sensor_msgs/gnss_best_pose.pb.h"
#include "modules/common_msgs/sensor_msgs/pointcloud.pb.h"
#include "modules/common_msgs/sensor_msgs/sensor_image.pb.h"
#include "modules/common_msgs/transform_msgs/transform.pb.h"

namespace apollo_ros2_bridge {

/// Frame names used by Apollo.
constexpr char kWorldFrame[] = "world";
constexpr char kLocalizationFrame[] = "localization";
constexpr char kImuFrame[] = "imu";
constexpr char kGnssFrame[] = "gnss";

/// Seconds since the epoch -> ROS time.
builtin_interfaces::msg::Time ToRosTime(double seconds);
/// ROS time -> seconds since the epoch.
double ToSeconds(const builtin_interfaces::msg::Time& stamp);

/// Picks the best timestamp of an Apollo message: header time, or the
/// measurement time when the header is empty.
double StampSeconds(const apollo::common::Header& header,
                    double measurement_time);

// ----------------------------------------------------------------- sensors

/// Fields: x, y, z (float32), intensity (float32), timestamp (float64, s).
void ToRos(const apollo::drivers::PointCloud& in,
           sensor_msgs::msg::PointCloud2* out);
/// Reads x/y/z, intensity (any numeric type) and an optional per-point time
/// field (`timestamp`, `time` or `t`; seconds, or nanoseconds for integers).
bool FromRos(const sensor_msgs::msg::PointCloud2& in,
             apollo::drivers::PointCloud* out);

void ToRos(const apollo::drivers::Image& in, sensor_msgs::msg::Image* out);
void FromRos(const sensor_msgs::msg::Image& in, apollo::drivers::Image* out);

void ToRos(const apollo::drivers::CompressedImage& in,
           sensor_msgs::msg::CompressedImage* out);
void FromRos(const sensor_msgs::msg::CompressedImage& in,
             apollo::drivers::CompressedImage* out);

void ToRos(const apollo::localization::CorrectedImu& in,
           sensor_msgs::msg::Imu* out);

void ToRos(const apollo::drivers::gnss::GnssBestPose& in,
           sensor_msgs::msg::NavSatFix* out);

// --------------------------------------------------------- localization/tf

void ToRos(const apollo::localization::LocalizationEstimate& in,
           nav_msgs::msg::Odometry* out);
/// world -> localization transform of a localization estimate.
void ToRos(const apollo::localization::LocalizationEstimate& in,
           geometry_msgs::msg::TransformStamped* out);
void ToRos(const apollo::transform::TransformStampeds& in,
           tf2_msgs::msg::TFMessage* out);

// --------------------------------------------------- planning / perception

void ToRos(const apollo::planning::ADCTrajectory& in, nav_msgs::msg::Path* out);

/// One CUBE (oriented bounding box) and one TEXT_VIEW_FACING (id/type)
/// marker per obstacle, preceded by a DELETEALL marker.
void ToRos(const apollo::perception::PerceptionObstacles& in,
           visualization_msgs::msg::MarkerArray* out);

/// Yaw (rad, around +z) -> quaternion.
geometry_msgs::msg::Quaternion YawToQuaternion(double yaw);

}  // namespace apollo_ros2_bridge

#endif  // APOLLO_ROS2_BRIDGE_CONVERSIONS_H_
