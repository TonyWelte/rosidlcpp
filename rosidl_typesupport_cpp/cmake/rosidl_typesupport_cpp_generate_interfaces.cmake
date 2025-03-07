# Copyright 2016-2018 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(NOT TARGET ${rosidl_generate_interfaces_TARGET}__rosidl_generator_cpp)
  message(FATAL_ERROR
    "The 'rosidl_generator_cpp' extension must be executed before the "
    "'rosidl_typesupport_cpp' extension.")
endif()

set(_output_path
  "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cpp/${PROJECT_NAME}")
set(_generated_sources "")
foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
  get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
  get_filename_component(_parent_folder "${_parent_folder}" NAME)
  get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
  string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
  list(APPEND _generated_sources
    "${_output_path}/${_parent_folder}/${_header_name}__type_support.cpp"
  )
endforeach()

set(_dependency_files "")
set(_dependencies "")
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  foreach(_idl_file ${${_pkg_name}_IDL_FILES})
    set(_abs_idl_file "${${_pkg_name}_DIR}/../${_idl_file}")
    normalize_path(_abs_idl_file "${_abs_idl_file}")
    list(APPEND _dependency_files "${_abs_idl_file}")
    list(APPEND _dependencies "${_pkg_name}:${_abs_idl_file}")
  endforeach()
endforeach()

set(target_dependencies
  "${rosidl_typesupport_cpp_BIN}"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/action__type_support.cpp.template"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/idl__type_support.cpp.template"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/msg__type_support.cpp.template"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/srv__type_support.cpp.template"
  ${rosidl_generate_interfaces_ABS_IDL_FILES}
  ${_dependency_files})
foreach(dep ${target_dependencies})
  if(NOT EXISTS "${dep}")
    message(FATAL_ERROR "Target dependency '${dep}' does not exist")
  endif()
endforeach()

set(generator_arguments_file "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cpp__arguments.json")
rosidl_write_generator_arguments(
  "${generator_arguments_file}"
  PACKAGE_NAME "${PROJECT_NAME}"
  IDL_TUPLES "${rosidl_generate_interfaces_IDL_TUPLES}"
  ROS_INTERFACE_DEPENDENCIES "${_dependencies}"
  OUTPUT_DIR "${_output_path}"
  TEMPLATE_DIR "${rosidl_typesupport_cpp_TEMPLATE_DIR}"
  TARGET_DEPENDENCIES ${target_dependencies}
)

# By default, without the settings below, find_package(Python3) will attempt
# to find the newest python version it can, and additionally will find the
# most specific version.  For instance, on a system that has
# /usr/bin/python3.10, /usr/bin/python3.11, and /usr/bin/python3, it will find
# /usr/bin/python3.11, even if /usr/bin/python3 points to /usr/bin/python3.10.
# The behavior we want is to prefer the "system" installed version unless the
# user specifically tells us othewise through the Python3_EXECUTABLE hint.
# Setting CMP0094 to NEW means that the search will stop after the first
# python version is found.  Setting Python3_FIND_UNVERSIONED_NAMES means that
# the search will prefer /usr/bin/python3 over /usr/bin/python3.11.  And that
# latter functionality is only available in CMake 3.20 or later, so we need
# at least that version.
cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0094 NEW)
set(Python3_FIND_UNVERSIONED_NAMES FIRST)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

get_used_typesupports(typesupports "rosidl_typesupport_cpp")
add_custom_command(
  OUTPUT ${_generated_sources}
  COMMAND ${rosidl_typesupport_cpp_BIN}
  --generator-arguments-file "${generator_arguments_file}"
  --typesupports "${typesupports}"
  DEPENDS ${target_dependencies}
  COMMENT "Generating C++ type support dispatch for ROS interfaces"
  VERBATIM
)

set(_target_suffix "__rosidl_typesupport_cpp")

add_library(${rosidl_generate_interfaces_TARGET}${_target_suffix} ${rosidl_typesupport_cpp_LIBRARY_TYPE} ${_generated_sources})
if(rosidl_generate_interfaces_LIBRARY_NAME)
  set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PROPERTIES OUTPUT_NAME "${rosidl_generate_interfaces_LIBRARY_NAME}${_target_suffix}")
endif()
set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PROPERTIES
    DEFINE_SYMBOL "ROSIDL_TYPESUPPORT_CPP_BUILDING_DLL"
    CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_precompile_headers(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PRIVATE
    # msg__type_support.cpp.em
    <cstddef>
    [["rosidl_runtime_c/message_type_support_struct.h"]]
    # rosidl_generator_c/ressource/idl__functions.h.em
    <stdbool.h>
    <stdlib.h>
    [["rosidl_runtime_c/action_type_support_struct.h"]]
    [["rosidl_runtime_c/message_type_support_struct.h"]]
    [["rosidl_runtime_c/service_type_support_struct.h"]]
    [["rosidl_runtime_c/type_description/type_description__struct.h"]]
    [["rosidl_runtime_c/type_description/type_source__struct.h"]]
    [["rosidl_runtime_c/type_hash.h"]]
    [["rosidl_runtime_c/visibility_control.h"]]
    # rosidl_generator_cpp/ressource/idl__struct.hpp.em
    <algorithm>
    <array>
    <memory>
    <string>
    <vector>
    [["rosidl_runtime_cpp/bounded_vector.hpp"]]
    [["rosidl_runtime_cpp/message_initialization.hpp"]]
)

# if only a single typesupport is used this package will directly reference it
# therefore it needs to link against the selected typesupport
if(NOT typesupports MATCHES ";")
  target_include_directories(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/${typesupports}>")
  target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PRIVATE
    ${rosidl_generate_interfaces_TARGET}__${typesupports})
else()
  if("${rosidl_typesupport_cpp_LIBRARY_TYPE}" STREQUAL "STATIC")
    message(FATAL_ERROR "Multiple typesupports [${typesupports}] but static "
      "linking was requested")
  endif()
endif()

# Depend on the target created by rosidl_generator_cpp
target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PUBLIC
  ${rosidl_generate_interfaces_TARGET}__rosidl_generator_c
  ${rosidl_generate_interfaces_TARGET}__rosidl_generator_cpp)

target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PRIVATE
  rosidl_runtime_c::rosidl_runtime_c
  rosidl_runtime_cpp::rosidl_runtime_cpp
  rosidl_typesupport_cpp::rosidl_typesupport_cpp
  rosidl_typesupport_c::rosidl_typesupport_c
  rosidl_typesupport_interface::rosidl_typesupport_interface)

foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PUBLIC
    ${${_pkg_name}_TARGETS${_target_suffix}})
endforeach()

# Make top level generation target depend on this library
add_dependencies(
  ${rosidl_generate_interfaces_TARGET}
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
)

if(NOT rosidl_generate_interfaces_SKIP_INSTALL)
  install(
    TARGETS ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    EXPORT ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )

  # Export old-style CMake variables
  ament_export_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix})

  # Export modern CMake targets
  ament_export_targets(${rosidl_generate_interfaces_TARGET}${_target_suffix})
  rosidl_export_typesupport_targets(${_target_suffix}
    ${rosidl_generate_interfaces_TARGET}${_target_suffix})

  ament_export_dependencies(
    "rosidl_runtime_c"
    "rosidl_runtime_cpp"
    "rosidl_typesupport_c"
    "rosidl_typesupport_cpp"
    "rosidl_typesupport_interface")
endif()

if(BUILD_TESTING AND rosidl_generate_interfaces_ADD_LINTER_TESTS)
  find_package(ament_cmake_cppcheck REQUIRED)
  ament_cppcheck(
    TESTNAME "cppcheck_rosidl_typesupport_cpp"
    "${_output_path}")

  find_package(ament_cmake_cpplint REQUIRED)
  get_filename_component(_cpplint_root "${_output_path}" DIRECTORY)
  ament_cpplint(
    TESTNAME "cpplint_rosidl_typesupport_cpp"
    # the generated code might contain longer lines for templated types
    MAX_LINE_LENGTH 999
    ROOT "${_cpplint_root}"
    "${_output_path}")

  find_package(ament_cmake_uncrustify REQUIRED)
  ament_uncrustify(
    TESTNAME "uncrustify_rosidl_typesupport_cpp"
    # the generated code might contain longer lines for templated types
    # a value of zero tells uncrustify to ignore line length
    MAX_LINE_LENGTH 0
    "${_output_path}")
endif()
