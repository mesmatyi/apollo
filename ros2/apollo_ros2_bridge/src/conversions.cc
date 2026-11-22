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

#include "apollo_ros2_bridge/conversions.h"

#include <cmath>
#include <cstring>
#include <string>

#include "sensor_msgs/point_cloud2_iterator.hpp"

namespace apollo_ros2_bridge {

namespace {

double OrZero(double v) { return std::isfinite(v) ? v : 0.0; }

geometry_msgs::msg::Quaternion ToRos(const apollo::common::Quaternion& q) {
  geometry_msgs::msg::Quaternion out;
  if (!q.has_qw() || !std::isfinite(q.qw()) || !std::isfinite(q.qx()) ||
      !std::isfinite(q.qy()) || !std::isfinite(q.qz())) {
    out.w = 1.0;
    return out;
  }
  out.x = q.qx();
  out.y = q.qy();
  out.z = q.qz();
  out.w = q.qw();
  return out;
}

const sensor_msgs::msg::PointField* FindField(
    const sensor_msgs::msg::PointCloud2& cloud, const std::string& name) {
  for (const auto& f : cloud.fields) {
    if (f.name == name) {
      return &f;
    }
  }
  return nullptr;
}

/// Reads a numeric field of any PointField datatype as double.
double ReadField(const uint8_t* point, const sensor_msgs::msg::PointField& f) {
  const uint8_t* p = point + f.offset;
  switch (f.datatype) {
    case sensor_msgs::msg::PointField::INT8: {
      int8_t v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    case sensor_msgs::msg::PointField::UINT8:
      return *p;
    case sensor_msgs::msg::PointField::INT16: {
      int16_t v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    case sensor_msgs::msg::PointField::UINT16: {
      uint16_t v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    case sensor_msgs::msg::PointField::INT32: {
      int32_t v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    case sensor_msgs::msg::PointField::UINT32: {
      uint32_t v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    case sensor_msgs::msg::PointField::FLOAT32: {
      float v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    case sensor_msgs::msg::PointField::FLOAT64: {
      double v;
      std::memcpy(&v, p, sizeof(v));
      return v;
    }
    default:
      return 0.0;
  }
}

bool IsIntegerField(const sensor_msgs::msg::PointField& f) {
  return f.datatype != sensor_msgs::msg::PointField::FLOAT32 &&
         f.datatype != sensor_msgs::msg::PointField::FLOAT64;
}

}  // namespace

builtin_interfaces::msg::Time ToRosTime(double seconds) {
  builtin_interfaces::msg::Time t;
  if (!std::isfinite(seconds) || seconds < 0) {
    return t;
  }
  double sec = std::floor(seconds);
  t.sec = static_cast<int32_t>(sec);
  t.nanosec = static_cast<uint32_t>(std::round((seconds - sec) * 1e9));
  if (t.nanosec >= 1000000000u) {
    t.sec += 1;
    t.nanosec -= 1000000000u;
  }
  return t;
}

double ToSeconds(const builtin_interfaces::msg::Time& stamp) {
  return static_cast<double>(stamp.sec) + stamp.nanosec * 1e-9;
}

double StampSeconds(const apollo::common::Header& header,
                    double measurement_time) {
  if (header.has_timestamp_sec() && header.timestamp_sec() > 0) {
    return header.timestamp_sec();
  }
  return measurement_time;
}

geometry_msgs::msg::Quaternion YawToQuaternion(double yaw) {
  geometry_msgs::msg::Quaternion q;
  q.z = std::sin(yaw * 0.5);
  q.w = std::cos(yaw * 0.5);
  return q;
}

// ----------------------------------------------------------------- sensors

void ToRos(const apollo::drivers::PointCloud& in,
           sensor_msgs::msg::PointCloud2* out) {
  out->header.stamp = ToRosTime(StampSeconds(in.header(), in.measurement_time()));
  out->header.frame_id = in.frame_id();
  const size_t n = static_cast<size_t>(in.point_size());
  if (in.height() > 1 && static_cast<size_t>(in.width()) * in.height() == n) {
    out->height = in.height();
    out->width = in.width();
  } else {
    out->height = 1;
    out->width = static_cast<uint32_t>(n);
  }
  out->is_dense = in.is_dense();
  out->is_bigendian = false;
  sensor_msgs::PointCloud2Modifier modifier(*out);
  modifier.setPointCloud2Fields(
      5, "x", 1, sensor_msgs::msg::PointField::FLOAT32, "y", 1,
      sensor_msgs::msg::PointField::FLOAT32, "z", 1,
      sensor_msgs::msg::PointField::FLOAT32, "intensity", 1,
      sensor_msgs::msg::PointField::FLOAT32, "timestamp", 1,
      sensor_msgs::msg::PointField::FLOAT64);
  modifier.resize(n);
  // resize() may reset the layout for unorganized clouds; restore it.
  if (in.height() > 1 && static_cast<size_t>(in.width()) * in.height() == n) {
    out->height = in.height();
    out->width = in.width();
    out->row_step = out->point_step * out->width;
  }
  sensor_msgs::PointCloud2Iterator<float> x(*out, "x");
  sensor_msgs::PointCloud2Iterator<float> y(*out, "y");
  sensor_msgs::PointCloud2Iterator<float> z(*out, "z");
  sensor_msgs::PointCloud2Iterator<float> intensity(*out, "intensity");
  sensor_msgs::PointCloud2Iterator<double> stamp(*out, "timestamp");
  for (const auto& p : in.point()) {
    *x = p.x();
    *y = p.y();
    *z = p.z();
    *intensity = static_cast<float>(p.intensity());
    *stamp = static_cast<double>(p.timestamp()) * 1e-9;
    ++x;
    ++y;
    ++z;
    ++intensity;
    ++stamp;
  }
}

bool FromRos(const sensor_msgs::msg::PointCloud2& in,
             apollo::drivers::PointCloud* out) {
  const auto* fx = FindField(in, "x");
  const auto* fy = FindField(in, "y");
  const auto* fz = FindField(in, "z");
  if (fx == nullptr || fy == nullptr || fz == nullptr ||
      in.point_step == 0) {
    return false;
  }
  const auto* fi = FindField(in, "intensity");
  const sensor_msgs::msg::PointField* ft = FindField(in, "timestamp");
  if (ft == nullptr) {
    ft = FindField(in, "time");
  }
  if (ft == nullptr) {
    ft = FindField(in, "t");
  }
  const double stamp = ToSeconds(in.header.stamp);
  out->Clear();
  out->mutable_header()->set_timestamp_sec(stamp);
  out->mutable_header()->set_module_name("apollo_ros2_bridge");
  out->set_frame_id(in.header.frame_id);
  out->set_measurement_time(stamp);
  out->set_is_dense(in.is_dense);
  out->set_width(in.width);
  out->set_height(in.height);
  const size_t n = static_cast<size_t>(in.width) * in.height;
  out->mutable_point()->Reserve(static_cast<int>(n));
  for (size_t row = 0; row < in.height; ++row) {
    const uint8_t* row_ptr = in.data.data() + row * in.row_step;
    for (size_t col = 0; col < in.width; ++col) {
      const uint8_t* p = row_ptr + col * in.point_step;
      if (p + in.point_step > in.data.data() + in.data.size()) {
        return false;
      }
      auto* point = out->add_point();
      point->set_x(static_cast<float>(ReadField(p, *fx)));
      point->set_y(static_cast<float>(ReadField(p, *fy)));
      point->set_z(static_cast<float>(ReadField(p, *fz)));
      if (fi != nullptr) {
        point->set_intensity(
            static_cast<uint32_t>(std::max(0.0, ReadField(p, *fi))));
      }
      if (ft != nullptr) {
        double t = ReadField(p, *ft);
        // Integer time fields are nanoseconds; float fields are seconds
        // (absolute, or relative to the cloud stamp when small).
        uint64_t ns;
        if (IsIntegerField(*ft)) {
          ns = static_cast<uint64_t>(t);
        } else {
          if (t < 1e6) {
            t += stamp;
          }
          ns = static_cast<uint64_t>(t * 1e9);
        }
        point->set_timestamp(ns);
      } else {
        point->set_timestamp(static_cast<uint64_t>(stamp * 1e9));
      }
    }
  }
  return true;
}

void ToRos(const apollo::drivers::Image& in, sensor_msgs::msg::Image* out) {
  out->header.stamp = ToRosTime(StampSeconds(in.header(), in.measurement_time()));
  out->header.frame_id = in.frame_id();
  out->height = in.height();
  out->width = in.width();
  out->encoding = in.encoding();
  out->is_bigendian = false;
  out->step = in.step();
  out->data.assign(in.data().begin(), in.data().end());
}

void FromRos(const sensor_msgs::msg::Image& in, apollo::drivers::Image* out) {
  const double stamp = ToSeconds(in.header.stamp);
  out->Clear();
  out->mutable_header()->set_timestamp_sec(stamp);
  out->mutable_header()->set_module_name("apollo_ros2_bridge");
  out->set_frame_id(in.header.frame_id);
  out->set_measurement_time(stamp);
  out->set_height(in.height);
  out->set_width(in.width);
  out->set_encoding(in.encoding);
  out->set_step(in.step);
  out->set_data(in.data.data(), in.data.size());
}

void ToRos(const apollo::drivers::CompressedImage& in,
           sensor_msgs::msg::CompressedImage* out) {
  out->header.stamp = ToRosTime(StampSeconds(in.header(), in.measurement_time()));
  out->header.frame_id = in.frame_id();
  out->format = in.format();
  out->data.assign(in.data().begin(), in.data().end());
}

void FromRos(const sensor_msgs::msg::CompressedImage& in,
             apollo::drivers::CompressedImage* out) {
  const double stamp = ToSeconds(in.header.stamp);
  out->Clear();
  out->mutable_header()->set_timestamp_sec(stamp);
  out->mutable_header()->set_module_name("apollo_ros2_bridge");
  out->set_frame_id(in.header.frame_id);
  out->set_measurement_time(stamp);
  out->set_format(in.format);
  out->set_data(in.data.data(), in.data.size());
}

void ToRos(const apollo::localization::CorrectedImu& in,
           sensor_msgs::msg::Imu* out) {
  out->header.stamp = ToRosTime(in.header().timestamp_sec());
  out->header.frame_id = kImuFrame;
  const auto& imu = in.imu();
  if (imu.has_orientation()) {
    out->orientation = ToRos(imu.orientation());
  } else {
    out->orientation_covariance[0] = -1.0;  // orientation unknown
  }
  out->angular_velocity.x = OrZero(imu.angular_velocity().x());
  out->angular_velocity.y = OrZero(imu.angular_velocity().y());
  out->angular_velocity.z = OrZero(imu.angular_velocity().z());
  out->linear_acceleration.x = OrZero(imu.linear_acceleration().x());
  out->linear_acceleration.y = OrZero(imu.linear_acceleration().y());
  out->linear_acceleration.z = OrZero(imu.linear_acceleration().z());
}

void ToRos(const apollo::drivers::gnss::GnssBestPose& in,
           sensor_msgs::msg::NavSatFix* out) {
  out->header.stamp = ToRosTime(StampSeconds(in.header(), in.measurement_time()));
  out->header.frame_id = kGnssFrame;
  out->latitude = in.latitude();
  out->longitude = in.longitude();
  // NavSatFix altitude is above the WGS-84 ellipsoid.
  out->altitude = in.height_msl() + in.undulation();
  out->status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS;
  out->status.status = in.sol_status() == 0
                           ? sensor_msgs::msg::NavSatStatus::STATUS_FIX
                           : sensor_msgs::msg::NavSatStatus::STATUS_NO_FIX;
  if (in.has_latitude_std_dev() || in.longitude_std_dev() ||
      in.height_std_dev()) {
    out->position_covariance[0] =
        static_cast<double>(in.longitude_std_dev()) * in.longitude_std_dev();
    out->position_covariance[4] =
        static_cast<double>(in.latitude_std_dev()) * in.latitude_std_dev();
    out->position_covariance[8] =
        static_cast<double>(in.height_std_dev()) * in.height_std_dev();
    out->position_covariance_type =
        sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;
  } else {
    out->position_covariance_type =
        sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_UNKNOWN;
  }
}

// --------------------------------------------------------- localization/tf

void ToRos(const apollo::localization::LocalizationEstimate& in,
           nav_msgs::msg::Odometry* out) {
  out->header.stamp = ToRosTime(StampSeconds(in.header(), in.measurement_time()));
  out->header.frame_id = kWorldFrame;
  out->child_frame_id = kLocalizationFrame;
  const auto& pose = in.pose();
  out->pose.pose.position.x = OrZero(pose.position().x());
  out->pose.pose.position.y = OrZero(pose.position().y());
  out->pose.pose.position.z = OrZero(pose.position().z());
  out->pose.pose.orientation = ToRos(pose.orientation());
  // Apollo velocities are expressed in the world (ENU) frame; nav_msgs
  // expects the twist in child_frame_id, so rotate it into the body frame.
  const auto& q = out->pose.pose.orientation;
  const double vx = OrZero(pose.linear_velocity().x());
  const double vy = OrZero(pose.linear_velocity().y());
  const double vz = OrZero(pose.linear_velocity().z());
  // v_body = R^T * v_world
  const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
  const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
  const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  const double r00 = 1 - 2 * (yy + zz), r01 = 2 * (xy - wz), r02 = 2 * (xz + wy);
  const double r10 = 2 * (xy + wz), r11 = 1 - 2 * (xx + zz), r12 = 2 * (yz - wx);
  const double r20 = 2 * (xz - wy), r21 = 2 * (yz + wx), r22 = 1 - 2 * (xx + yy);
  out->twist.twist.linear.x = r00 * vx + r10 * vy + r20 * vz;
  out->twist.twist.linear.y = r01 * vx + r11 * vy + r21 * vz;
  out->twist.twist.linear.z = r02 * vx + r12 * vy + r22 * vz;
  out->twist.twist.angular.x = OrZero(pose.angular_velocity().x());
  out->twist.twist.angular.y = OrZero(pose.angular_velocity().y());
  out->twist.twist.angular.z = OrZero(pose.angular_velocity().z());
}

void ToRos(const apollo::localization::LocalizationEstimate& in,
           geometry_msgs::msg::TransformStamped* out) {
  out->header.stamp = ToRosTime(StampSeconds(in.header(), in.measurement_time()));
  out->header.frame_id = kWorldFrame;
  out->child_frame_id = kLocalizationFrame;
  out->transform.translation.x = OrZero(in.pose().position().x());
  out->transform.translation.y = OrZero(in.pose().position().y());
  out->transform.translation.z = OrZero(in.pose().position().z());
  out->transform.rotation = ToRos(in.pose().orientation());
}

void ToRos(const apollo::transform::TransformStampeds& in,
           tf2_msgs::msg::TFMessage* out) {
  out->transforms.clear();
  out->transforms.reserve(in.transforms_size());
  for (const auto& t : in.transforms()) {
    geometry_msgs::msg::TransformStamped ros;
    double stamp = t.header().timestamp_sec();
    if (stamp <= 0) {
      stamp = in.header().timestamp_sec();
    }
    ros.header.stamp = ToRosTime(stamp);
    ros.header.frame_id = t.header().frame_id();
    ros.child_frame_id = t.child_frame_id();
    ros.transform.translation.x = OrZero(t.transform().translation().x());
    ros.transform.translation.y = OrZero(t.transform().translation().y());
    ros.transform.translation.z = OrZero(t.transform().translation().z());
    ros.transform.rotation = ToRos(t.transform().rotation());
    out->transforms.push_back(std::move(ros));
  }
}

// --------------------------------------------------- planning / perception

void ToRos(const apollo::planning::ADCTrajectory& in, nav_msgs::msg::Path* out) {
  out->header.stamp = ToRosTime(in.header().timestamp_sec());
  out->header.frame_id = kWorldFrame;
  out->poses.clear();
  const double t0 = in.header().timestamp_sec();
  for (const auto& tp : in.trajectory_point()) {
    geometry_msgs::msg::PoseStamped pose;
    pose.header.frame_id = kWorldFrame;
    pose.header.stamp = ToRosTime(t0 + tp.relative_time());
    pose.pose.position.x = tp.path_point().x();
    pose.pose.position.y = tp.path_point().y();
    pose.pose.position.z = tp.path_point().z();
    pose.pose.orientation = YawToQuaternion(tp.path_point().theta());
    out->poses.push_back(std::move(pose));
  }
}

void ToRos(const apollo::perception::PerceptionObstacles& in,
           visualization_msgs::msg::MarkerArray* out) {
  out->markers.clear();
  const auto stamp = ToRosTime(in.header().timestamp_sec());
  visualization_msgs::msg::Marker clear;
  clear.header.frame_id = kWorldFrame;
  clear.header.stamp = stamp;
  clear.action = visualization_msgs::msg::Marker::DELETEALL;
  out->markers.push_back(clear);

  for (const auto& obstacle : in.perception_obstacle()) {
    visualization_msgs::msg::Marker box;
    box.header.frame_id = kWorldFrame;
    box.header.stamp = stamp;
    box.ns = "obstacles";
    box.id = obstacle.id();
    box.type = visualization_msgs::msg::Marker::CUBE;
    box.action = visualization_msgs::msg::Marker::ADD;
    box.pose.position.x = OrZero(obstacle.position().x());
    box.pose.position.y = OrZero(obstacle.position().y());
    box.pose.position.z =
        OrZero(obstacle.position().z()) + obstacle.height() * 0.5;
    box.pose.orientation = YawToQuaternion(obstacle.theta());
    box.scale.x = std::max(obstacle.length(), 0.1);
    box.scale.y = std::max(obstacle.width(), 0.1);
    box.scale.z = std::max(obstacle.height(), 0.1);
    box.color.a = 0.6f;
    switch (obstacle.type()) {
      case apollo::perception::PerceptionObstacle::PEDESTRIAN:
        box.color.r = 1.0f;
        box.color.g = 0.8f;
        break;
      case apollo::perception::PerceptionObstacle::BICYCLE:
        box.color.r = 0.2f;
        box.color.g = 0.8f;
        box.color.b = 1.0f;
        break;
      case apollo::perception::PerceptionObstacle::VEHICLE:
        box.color.g = 1.0f;
        break;
      default:
        box.color.r = 0.7f;
        box.color.g = 0.7f;
        box.color.b = 0.7f;
        break;
    }
    out->markers.push_back(box);

    visualization_msgs::msg::Marker label = box;
    label.ns = "obstacle_ids";
    label.type = visualization_msgs::msg::Marker::TEXT_VIEW_FACING;
    label.pose.position.z += box.scale.z * 0.5 + 0.5;
    label.pose.orientation = geometry_msgs::msg::Quaternion();
    label.scale.x = label.scale.y = 0.0;
    label.scale.z = 0.8;
    label.color.r = label.color.g = label.color.b = label.color.a = 1.0f;
    label.text = std::to_string(obstacle.id()) + " " +
                 apollo::perception::PerceptionObstacle::Type_Name(
                     obstacle.type());
    out->markers.push_back(label);
  }
}

}  // namespace apollo_ros2_bridge
