cmake_minimum_required(VERSION 3.12)

project(rosidlcpp_typesupport_c)

# Default to C++23
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if($ENV{ROS_DISTRO} STREQUAL "jazzy")
  find_package(ament_cmake_ros REQUIRED)
else()
  find_package(ament_cmake_ros_core REQUIRED)
endif()
find_package(rcpputils REQUIRED)
find_package(rcutils REQUIRED)
find_package(rosidl_runtime_c REQUIRED)

ament_export_dependencies(rcutils)
ament_export_dependencies(rcpputils)
ament_export_dependencies(rosidl_runtime_c)
ament_export_dependencies(rosidl_typesupport_interface)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

add_executable(rosidlcpp_typesupport_c src/rosidlcpp_typesupport_c/rosidlcpp_typesupport_c.cpp)

target_include_directories(rosidlcpp_typesupport_c PUBLIC include)
target_link_libraries(
  rosidlcpp_typesupport_c
  PUBLIC
  fmt::fmt
  nlohmann_json::nlohmann_json
  rosidlcpp_parser::rosidlcpp_parser
  rosidlcpp_generator_core::rosidlcpp_generator_core
)

ament_index_register_resource("rosidl_generator_packages")

if(BUILD_TESTING)
  # TODO
endif()

if(BUILD_SHARED_LIBS)
  set(${PROJECT_NAME}_LIBRARY_TYPE "SHARED")
else()
  set(${PROJECT_NAME}_LIBRARY_TYPE "STATIC")
endif()

ament_package(
  CONFIG_EXTRAS "rosidlcpp_typesupport_c-extras.cmake.in"
)

install(
  TARGETS rosidlcpp_typesupport_c
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)
install(
  TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
