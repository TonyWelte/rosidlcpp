cmake_minimum_required(VERSION 3.12)

project(rosidlcpp_generator_c)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 23)


find_package(ament_cmake_ros REQUIRED)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

include(cmake/CPM.cmake)
CPMAddPackage(
  NAME thread-pool
  GITHUB_REPOSITORY DeveloperPaul123/thread-pool
  GIT_TAG 0.7.0
  OPTIONS
    "TP_BUILD_TESTS OFF"
    "TP_BUILD_BENCHMARKS OFF"
    "TP_BUILD_EXAMPLES OFF"  
)

add_executable(rosidlcpp_generator_c src/rosidlcpp_generator_c/rosidlcpp_generator_c.cpp)

target_include_directories(rosidlcpp_generator_c PUBLIC include)
target_link_libraries(
  rosidlcpp_generator_c
  PUBLIC
  fmt::fmt
  nlohmann_json::nlohmann_json
  rosidlcpp_parser::rosidlcpp_parser
  rosidlcpp_generator_core::rosidlcpp_generator_core
  dp::thread-pool
)

ament_export_dependencies(
  fmt
  nlohmann_json
  rosidlcpp_parser
  rosidlcpp_generator_core
)

ament_index_register_resource("rosidl_generator_packages")

if(BUILD_SHARED_LIBS)
  set(${PROJECT_NAME}_LIBRARY_TYPE "SHARED")
else()
  set(${PROJECT_NAME}_LIBRARY_TYPE "STATIC")
endif()

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package(
  CONFIG_EXTRAS "rosidlcpp_generator_c-extras.cmake.in"
)

install(
  TARGETS rosidlcpp_generator_c
  DESTINATION lib/rosidlcpp_generator_c
)

install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
