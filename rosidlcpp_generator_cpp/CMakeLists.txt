cmake_minimum_required(VERSION 3.8)
project(rosidlcpp_generator_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(rosidlcpp_parser REQUIRED)

ament_index_register_resource("rosidl_generator_packages")

add_executable(rosidlcpp_generator_cpp src/main.cpp)

target_include_directories(rosidlcpp_generator_cpp PUBLIC include)
target_link_libraries(rosidlcpp_generator_cpp PUBLIC fmt::fmt nlohmann_json::nlohmann_json rosidlcpp_parser::rosidlcpp_parser)

ament_package(
  CONFIG_EXTRAS "rosidlcpp_generator_cpp-extras.cmake.in"
)

install(
  TARGETS rosidlcpp_generator_cpp
  DESTINATION lib/rosidlcpp_generator_cpp
)
install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  # TODO

endif()
