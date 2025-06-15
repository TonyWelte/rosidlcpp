cmake_minimum_required(VERSION 3.12)

project(rosidlcpp_typesupport_fastrtps_c)

if(DEFINED ENV{FASTRTPS_STATIC_DISABLE})
  set(FASTRTPS_STATIC_DISABLE $ENV{FASTRTPS_STATIC_DISABLE}
    CACHE BOOL "If fastrtps Static should be disabled.")
else()
  set(FASTRTPS_STATIC_DISABLE FALSE
    CACHE BOOL "If fastrtps Static should be disabled.")
endif()

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++23
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

if(FASTRTPS_STATIC_DISABLE)
  ament_package()
  message(STATUS "fastrtps static rmw implementation explicitly disabled - skipping '${PROJECT_NAME}'")
  return()
endif()

find_package(ament_cmake_python REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(fastcdr REQUIRED CONFIG)

ament_export_dependencies(rosidl_runtime_c)
ament_export_dependencies(fastcdr)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

add_executable(rosidlcpp_typesupport_fastrtps_c src/rosidlcpp_typesupport_fastrtps_c/rosidlcpp_typesupport_fastrtps_c.cpp)

target_include_directories(rosidlcpp_typesupport_fastrtps_c PUBLIC include)
target_link_libraries(
  rosidlcpp_typesupport_fastrtps_c
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

ament_package(
  CONFIG_EXTRAS "rosidlcpp_typesupport_fastrtps_c-extras.cmake.in"
)

install(
  TARGETS rosidlcpp_typesupport_fastrtps_c
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
