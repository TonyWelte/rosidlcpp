cmake_minimum_required(VERSION 3.12)

project(rosidl_generator_py)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(CMAKE_CXX_STANDARD 23)

find_package(ament_cmake REQUIRED)

find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

ament_export_dependencies(ament_cmake)
ament_export_dependencies(rmw)

add_executable(rosidl_generator_py src/rosidlcpp_generator_py/rosidlcpp_generator_py.cpp)

target_include_directories(rosidl_generator_py PUBLIC include)
target_link_libraries(rosidl_generator_py PUBLIC fmt::fmt nlohmann_json::nlohmann_json rosidlcpp_parser::rosidlcpp_parser rosidlcpp_generator_core::rosidlcpp_generator_core)

ament_index_register_resource("rosidl_generator_packages")

ament_package(
  CONFIG_EXTRAS "cmake/rosidl_generator_py_get_typesupports.cmake"
    "cmake/register_py.cmake"
    "rosidl_generator_py-extras.cmake.in"
)

install(
  TARGETS rosidl_generator_py
  DESTINATION lib/rosidl_generator_py
)

install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
