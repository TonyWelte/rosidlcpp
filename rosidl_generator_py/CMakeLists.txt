cmake_minimum_required(VERSION 3.12)

project(rosidl_generator_py)

find_package(ament_cmake REQUIRED)

ament_index_register_resource("rosidl_generator_packages")

ament_package(
  CONFIG_EXTRAS "cmake/rosidl_generator_py_get_typesupports.cmake"
    "cmake/register_py.cmake"
    "rosidl_generator_py-extras.cmake.in"
)

install(
  DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME}
)
