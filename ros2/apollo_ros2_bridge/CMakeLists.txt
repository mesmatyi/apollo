cmake_minimum_required(VERSION 3.16)
project(apollo_ros2_bridge LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(apollo REQUIRED)
find_package(cyber REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)

set(ROS_MSG_TARGETS
  ${geometry_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  ${tf2_msgs_TARGETS}
  ${visualization_msgs_TARGETS})

# Pure conversion functions (unit tested).
add_library(apollo_ros2_conversions SHARED src/conversions.cc)
target_include_directories(apollo_ros2_conversions PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_link_libraries(apollo_ros2_conversions PUBLIC
  apollo::apollo_proto ${ROS_MSG_TARGETS})

# Composable nodes.
add_library(apollo_ros2_bridge SHARED src/apollo_to_ros.cc src/ros_to_apollo.cc)
target_link_libraries(apollo_ros2_bridge
  apollo_ros2_conversions cyber::cyber rclcpp::rclcpp
  rclcpp_components::component)
rclcpp_components_register_node(apollo_ros2_bridge
  PLUGIN "apollo_ros2_bridge::ApolloToRos"
  EXECUTABLE apollo_to_ros)
rclcpp_components_register_node(apollo_ros2_bridge
  PLUGIN "apollo_ros2_bridge::RosToApollo"
  EXECUTABLE ros_to_apollo)

install(TARGETS apollo_ros2_conversions apollo_ros2_bridge
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(conversions_test test/conversions_test.cc)
  target_link_libraries(conversions_test apollo_ros2_conversions)
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(apollo cyber geometry_msgs nav_msgs sensor_msgs
  tf2_msgs visualization_msgs)
ament_package()
