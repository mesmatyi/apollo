# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
"""Starts the Apollo <-> ROS 2 bridge.

ros2 launch apollo_ros2_bridge bridge.launch.py
ros2 launch apollo_ros2_bridge bridge.launch.py container:=/apollo_container

With `container:=<name>` the bridge nodes are loaded into an existing
component container (e.g. the one running Apollo perception), so they read
Apollo messages without serialization.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import ComposableNodeContainer, LoadComposableNodes
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    params = os.path.join(get_package_share_directory('apollo_ros2_bridge'),
                          'config', 'bridge.yaml')
    config = LaunchConfiguration('config')
    container = LaunchConfiguration('container')
    nodes = [
        ComposableNode(package='apollo_ros2_bridge',
                       plugin='apollo_ros2_bridge::ApolloToRos',
                       name='apollo_to_ros', parameters=[config]),
        ComposableNode(package='apollo_ros2_bridge',
                       plugin='apollo_ros2_bridge::RosToApollo',
                       name='ros_to_apollo', parameters=[config]),
    ]
    standalone = PythonExpression(["'", container, "' == ''"])
    return LaunchDescription([
        DeclareLaunchArgument('config', default_value=params,
                              description='bridge parameter file'),
        DeclareLaunchArgument('container', default_value='',
                              description='load into this existing container'),
        ComposableNodeContainer(
            name='apollo_ros2_bridge', namespace='',
            package='rclcpp_components', executable='component_container_mt',
            composable_node_descriptions=nodes, output='screen',
            condition=IfCondition(standalone)),
        LoadComposableNodes(
            target_container=container, composable_node_descriptions=nodes,
            condition=UnlessCondition(standalone)),
    ])
