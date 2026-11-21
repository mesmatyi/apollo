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

#include "gtest/gtest.h"

namespace apollo {
namespace cyber {
namespace ros2 {

TEST(NamingTest, ChannelToTopic) {
  EXPECT_EQ("/apollo/control", ChannelToTopic("/apollo/control"));
  EXPECT_EQ("/planning", ChannelToTopic("planning"));
  EXPECT_EQ("/apollo/sensor/lidar_16/PointCloud2",
            ChannelToTopic("/apollo/sensor/lidar-16/PointCloud2"));
  EXPECT_EQ("/apollo/x", ChannelToTopic("/apollo//x/"));
  EXPECT_EQ("/a/_3d_obj", ChannelToTopic("/a/3d_obj"));
  EXPECT_EQ("", ChannelToTopic(""));
  EXPECT_EQ("/", ChannelToTopic("/"));
  EXPECT_EQ("/apollo/tf", ChannelToTopic("/tf"));
  EXPECT_EQ("/apollo/tf_static", ChannelToTopic("tf_static"));
  EXPECT_EQ("/apollo/clock", ChannelToTopic("/clock"));
  EXPECT_EQ("/tf/extra", ChannelToTopic("/tf/extra"));
}

TEST(NamingTest, ToNodeName) {
  EXPECT_EQ("control", ToNodeName("control"));
  EXPECT_EQ("lidar_16_driver", ToNodeName("lidar-16.driver"));
  EXPECT_EQ("_2d_detector", ToNodeName("2d_detector"));
  EXPECT_EQ("apollo_node", ToNodeName(""));
}

}  // namespace ros2
}  // namespace cyber
}  // namespace apollo
