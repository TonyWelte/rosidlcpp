cmake_minimum_required(VERSION 3.12)

project(rosidlcpp_generator_type_description)

set(CMAKE_CXX_STANDARD 23)

find_package(ament_cmake_ros REQUIRED)

find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

ament_index_register_resource("rosidl_generator_packages")

add_executable(rosidlcpp_generator_type_description src/rosidlcpp_generator_type_description/rosidlcpp_generator_type_description.cpp)

target_include_directories(rosidlcpp_generator_type_description PUBLIC include)
target_link_libraries(rosidlcpp_generator_type_description PUBLIC fmt::fmt nlohmann_json::nlohmann_json rosidlcpp_parser::rosidlcpp_parser rosidlcpp_generator_core::rosidlcpp_generator_core)

ament_package(
  CONFIG_EXTRAS "${PROJECT_NAME}-extras.cmake.in"
)

install(
  TARGETS rosidlcpp_generator_type_description
  DESTINATION lib/rosidlcpp_generator_type_description
)
install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
