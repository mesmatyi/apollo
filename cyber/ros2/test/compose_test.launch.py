# Copyright 2026 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""An Apollo component in a standard ROS 2 component container, configured
the ROS 2 way: a gflag as a parameter, simulation time and a remapped
channel. Used by cli_interop_test.sh."""

from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    component = ComposableNode(
        package='cyber',
        plugin='CommonComponentSample',
        name='common',
        parameters=[{
            'component_config':
                'name: "common" '
                'readers { channel: "/apollo/prediction" } '
                'readers { channel: "/apollo/test" }',
            'gflags.minloglevel': 2,
            'use_sim_time': True,
        }],
        remappings=[('/apollo/prediction', '/renamed/prediction')])
    container = ComposableNodeContainer(
        name='apollo_compose_test',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        composable_node_descriptions=[component],
        output='screen')
    return LaunchDescription([container])
