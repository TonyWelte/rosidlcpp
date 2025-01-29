cmake_minimum_required(VERSION 3.8)
project(rosidlcpp_generator_py)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(CMAKE_CXX_STANDARD 23)

# find dependencies
find_package(ament_cmake REQUIRED)

find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

find_package(rosidlcpp_parser REQUIRED)
find_package(rosidlcpp_generator_core REQUIRED)

ament_export_dependencies(ament_cmake)
ament_export_dependencies(rmw)

add_executable(rosidlcpp_generator_py src/rosidlcpp_generator_py/rosidlcpp_generator_py.cpp)

target_include_directories(rosidlcpp_generator_py PUBLIC include)
target_link_libraries(rosidlcpp_generator_py PUBLIC fmt::fmt nlohmann_json::nlohmann_json rosidlcpp_parser::rosidlcpp_parser rosidlcpp_generator_core::rosidlcpp_generator_core)

install(
  TARGETS rosidlcpp_generator_py
  DESTINATION lib/rosidlcpp_generator_py
)
install(
  DIRECTORY resource
  DESTINATION share/${PROJECT_NAME}
)

ament_package()