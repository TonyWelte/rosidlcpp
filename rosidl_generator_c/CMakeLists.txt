cmake_minimum_required(VERSION 3.12)

project(rosidl_generator_c C)

find_package(ament_cmake REQUIRED)

ament_index_register_resource("rosidl_generator_packages")

ament_package(
  CONFIG_EXTRAS "rosidl_generator_c-extras.cmake.in"
)

install(
  DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME}
)
