cmake_minimum_required(VERSION 3.12)
project(rosidlcpp_parser)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 23)

# find dependencies
find_package(ament_cmake REQUIRED)

find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

add_library(rosidlcpp_parser SHARED src/rosidlcpp_parser/rosidlcpp_parser.cpp)
target_link_libraries(rosidlcpp_parser nlohmann_json::nlohmann_json fmt::fmt)
target_include_directories(rosidlcpp_parser PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")

add_executable(main src/main.cpp)
target_include_directories(main PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(main rosidlcpp_parser)

install(TARGETS ${PROJECT_NAME} main
  EXPORT export_${PROJECT_NAME}
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(rosidlcpp_parser_test test/unit/rosidlcpp_parser_test.cpp)
  target_link_libraries(rosidlcpp_parser_test rosidlcpp_parser)
  target_include_directories(rosidlcpp_parser_test PUBLIC include)
endif()

ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_libraries(${PROJECT_NAME})

# Export modern CMake targets
ament_export_targets(export_${PROJECT_NAME})

ament_package()
