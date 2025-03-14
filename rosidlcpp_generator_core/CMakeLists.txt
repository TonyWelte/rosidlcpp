cmake_minimum_required(VERSION 3.12)
project(rosidlcpp_generator_core)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 23)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
find_package(rosidlcpp_parser REQUIRED)

add_library(${PROJECT_NAME} SHARED src/${PROJECT_NAME}/generator_base.cpp src/${PROJECT_NAME}/generator_utils.cpp)
target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json fmt::fmt rosidlcpp_parser::rosidlcpp_parser)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_libraries(${PROJECT_NAME})

ament_export_dependencies(ament_cmake nlohmann_json fmt rosidlcpp_parser)

# Export modern CMake targets
ament_export_targets(export_${PROJECT_NAME})

ament_package()
