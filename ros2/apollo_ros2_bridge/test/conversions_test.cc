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

#include "gtest/gtest.h"

namespace apollo_ros2_bridge {
namespace {

TEST(ConversionsTest, PointCloudRoundTrip) {
  apollo::drivers::PointCloud cloud;
  cloud.mutable_header()->set_timestamp_sec(1700000000.25);
  cloud.set_frame_id("velodyne128");
  for (int i = 0; i < 4; ++i) {
    auto* p = cloud.add_point();
    p->set_x(i);
    p->set_y(2 * i);
    p->set_z(-i);
    p->set_intensity(10 * i);
    p->set_timestamp(1700000000250000000ull + i);
  }
  sensor_msgs::msg::PointCloud2 ros;
  ToRos(cloud, &ros);
  EXPECT_EQ("velodyne128", ros.header.frame_id);
  EXPECT_EQ(1700000000, ros.header.stamp.sec);
  EXPECT_EQ(250000000u, ros.header.stamp.nanosec);
  EXPECT_EQ(1u, ros.height);
  EXPECT_EQ(4u, ros.width);
  EXPECT_EQ(5u, ros.fields.size());

  apollo::drivers::PointCloud back;
  ASSERT_TRUE(FromRos(ros, &back));
  ASSERT_EQ(4, back.point_size());
  EXPECT_FLOAT_EQ(3.f, back.point(3).x());
  EXPECT_FLOAT_EQ(6.f, back.point(3).y());
  EXPECT_FLOAT_EQ(-3.f, back.point(3).z());
  EXPECT_EQ(30u, back.point(3).intensity());
  // float64 seconds lose sub-microsecond precision around 1.7e9 s
  EXPECT_NEAR(1700000000250000003.0,
              static_cast<double>(back.point(3).timestamp()), 1e3);
  EXPECT_EQ("velodyne128", back.frame_id());
}

TEST(ConversionsTest, LocalizationToOdometryAndTf) {
  apollo::localization::LocalizationEstimate loc;
  loc.mutable_header()->set_timestamp_sec(10.5);
  auto* pose = loc.mutable_pose();
  pose->mutable_position()->set_x(1.0);
  pose->mutable_position()->set_y(2.0);
  pose->mutable_position()->set_z(3.0);
  // 90 degrees yaw
  pose->mutable_orientation()->set_qw(std::cos(M_PI / 4));
  pose->mutable_orientation()->set_qx(0);
  pose->mutable_orientation()->set_qy(0);
  pose->mutable_orientation()->set_qz(std::sin(M_PI / 4));
  // Moving north (world +y) == forward (body +x) when facing north.
  pose->mutable_linear_velocity()->set_x(0);
  pose->mutable_linear_velocity()->set_y(5.0);
  pose->mutable_linear_velocity()->set_z(0);

  nav_msgs::msg::Odometry odom;
  ToRos(loc, &odom);
  EXPECT_EQ("world", odom.header.frame_id);
  EXPECT_EQ("localization", odom.child_frame_id);
  EXPECT_DOUBLE_EQ(2.0, odom.pose.pose.position.y);
  EXPECT_NEAR(5.0, odom.twist.twist.linear.x, 1e-9);
  EXPECT_NEAR(0.0, odom.twist.twist.linear.y, 1e-9);

  geometry_msgs::msg::TransformStamped tf;
  ToRos(loc, &tf);
  EXPECT_DOUBLE_EQ(3.0, tf.transform.translation.z);
  EXPECT_NEAR(std::sin(M_PI / 4), tf.transform.rotation.z, 1e-12);
}

TEST(ConversionsTest, MissingOrientationIsIdentity) {
  apollo::localization::LocalizationEstimate loc;
  geometry_msgs::msg::TransformStamped tf;
  ToRos(loc, &tf);
  EXPECT_DOUBLE_EQ(1.0, tf.transform.rotation.w);
}

TEST(ConversionsTest, TransformsAndTrajectory) {
  apollo::transform::TransformStampeds tfs;
  tfs.mutable_header()->set_timestamp_sec(5.0);
  auto* t = tfs.add_transforms();
  t->mutable_header()->set_frame_id("localization");
  t->set_child_frame_id("novatel");
  t->mutable_transform()->mutable_translation()->set_x(0.5);
  t->mutable_transform()->mutable_rotation()->set_qw(1.0);
  t->mutable_transform()->mutable_rotation()->set_qx(0.0);
  t->mutable_transform()->mutable_rotation()->set_qy(0.0);
  t->mutable_transform()->mutable_rotation()->set_qz(0.0);
  tf2_msgs::msg::TFMessage tf_msg;
  ToRos(tfs, &tf_msg);
  ASSERT_EQ(1u, tf_msg.transforms.size());
  EXPECT_EQ("localization", tf_msg.transforms[0].header.frame_id);
  EXPECT_EQ("novatel", tf_msg.transforms[0].child_frame_id);
  EXPECT_EQ(5, tf_msg.transforms[0].header.stamp.sec);

  apollo::planning::ADCTrajectory traj;
  traj.mutable_header()->set_timestamp_sec(100.0);
  for (int i = 0; i < 3; ++i) {
    auto* tp = traj.add_trajectory_point();
    tp->mutable_path_point()->set_x(i);
    tp->mutable_path_point()->set_theta(M_PI);
    tp->set_relative_time(0.1 * i);
  }
  nav_msgs::msg::Path path;
  ToRos(traj, &path);
  ASSERT_EQ(3u, path.poses.size());
  EXPECT_DOUBLE_EQ(2.0, path.poses[2].pose.position.x);
  EXPECT_NEAR(1.0, path.poses[2].pose.orientation.z, 1e-12);
}

TEST(ConversionsTest, ObstaclesToMarkers) {
  apollo::perception::PerceptionObstacles obstacles;
  auto* o = obstacles.add_perception_obstacle();
  o->set_id(42);
  o->set_type(apollo::perception::PerceptionObstacle::VEHICLE);
  o->mutable_position()->set_x(10);
  o->mutable_position()->set_y(20);
  o->mutable_position()->set_z(0);
  o->set_length(4.5);
  o->set_width(1.8);
  o->set_height(1.5);
  visualization_msgs::msg::MarkerArray markers;
  ToRos(obstacles, &markers);
  ASSERT_EQ(3u, markers.markers.size());
  EXPECT_EQ(visualization_msgs::msg::Marker::DELETEALL, markers.markers[0].action);
  EXPECT_EQ(42, markers.markers[1].id);
  EXPECT_DOUBLE_EQ(4.5, markers.markers[1].scale.x);
  EXPECT_DOUBLE_EQ(0.75, markers.markers[1].pose.position.z);
  EXPECT_EQ("42 VEHICLE", markers.markers[2].text);
}

TEST(ConversionsTest, GnssToNavSatFix) {
  apollo::drivers::gnss::GnssBestPose gnss;
  gnss.set_latitude(37.4);
  gnss.set_longitude(-122.1);
  gnss.set_height_msl(10.0);
  gnss.set_undulation(-30.0f);
  gnss.set_latitude_std_dev(0.5f);
  sensor_msgs::msg::NavSatFix fix;
  ToRos(gnss, &fix);
  EXPECT_DOUBLE_EQ(37.4, fix.latitude);
  EXPECT_DOUBLE_EQ(-20.0, fix.altitude);
  EXPECT_DOUBLE_EQ(0.25, fix.position_covariance[4]);
}

}  // namespace
}  // namespace apollo_ros2_bridge
