cmake_minimum_required(VERSION 3.12)

project(rosidl_generator_type_description)

find_package(ament_cmake_ros REQUIRED)

ament_index_register_resource("rosidl_generator_packages")

ament_package(
  CONFIG_EXTRAS "${PROJECT_NAME}-extras.cmake.in"
)

install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
