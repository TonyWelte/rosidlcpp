cmake_minimum_required(VERSION 3.12)

project(rosidl_typesupport_introspection_c)

# TODO: Specify C vs C++ for library and executable
# Default to C++23
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake_python REQUIRED)
find_package(ament_cmake_ros REQUIRED)

ament_export_dependencies(rosidl_runtime_c)
ament_export_dependencies(rosidl_typesupport_interface)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

add_executable(rosidlcpp_typesupport_introspection_c src/rosidlcpp_typesupport_introspection_c/rosidlcpp_typesupport_introspection_c.cpp)

target_include_directories(rosidlcpp_typesupport_introspection_c PUBLIC include)
target_link_libraries(
  rosidlcpp_typesupport_introspection_c
  PUBLIC
  fmt::fmt
  nlohmann_json::nlohmann_json
  rosidlcpp_parser::rosidlcpp_parser
  rosidlcpp_generator_core::rosidlcpp_generator_core
)

add_library(${PROJECT_NAME} src/identifier.c)
if(WIN32)
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE "ROSIDL_TYPESUPPORT_INTROSPECTION_C_BUILDING_DLL")
endif()
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")

# Export old-style CMake variables
ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_libraries(${PROJECT_NAME})

# Export modern CMake targets
ament_export_targets(${PROJECT_NAME})

ament_index_register_resource("rosidl_typesupport_c")
ament_index_register_resource("rosidl_runtime_packages")

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

if(BUILD_SHARED_LIBS)
  set(${PROJECT_NAME}_LIBRARY_TYPE "SHARED")
else()
  set(${PROJECT_NAME}_LIBRARY_TYPE "STATIC")
endif()

ament_package(
  CONFIG_EXTRAS "rosidl_typesupport_introspection_c-extras.cmake.in"
)

install(
  TARGETS rosidlcpp_typesupport_introspection_c
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
