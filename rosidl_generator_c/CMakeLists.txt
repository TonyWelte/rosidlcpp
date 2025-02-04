cmake_minimum_required(VERSION 3.12)

project(rosidl_generator_c)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 23)

find_package(ament_cmake REQUIRED)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

add_executable(rosidl_generator_c src/rosidlcpp_generator_c/rosidlcpp_generator_c.cpp)

target_include_directories(rosidl_generator_c PUBLIC include)
target_link_libraries(
  rosidl_generator_c
  PUBLIC
  fmt::fmt
  nlohmann_json::nlohmann_json
  rosidlcpp_parser::rosidlcpp_parser
  rosidlcpp_generator_core::rosidlcpp_generator_core
)

ament_export_dependencies(
  fmt
  nlohmann_json
  rosidlcpp_parser
  rosidlcpp_generator_core
)

ament_index_register_resource("rosidl_generator_packages")

ament_package(
  CONFIG_EXTRAS "rosidl_generator_c-extras.cmake.in"
)

install(
  TARGETS rosidl_generator_c
  DESTINATION lib/rosidl_generator_c
)

install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
