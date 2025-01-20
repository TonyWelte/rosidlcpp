# Copyright 2015-2018 Open Source Robotics Foundation, Inc.
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

find_package(rcutils REQUIRED)
find_package(rosidl_cmake REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_interface REQUIRED)

set(rosidl_generate_interfaces_c_IDL_TUPLES
  ${rosidl_generate_interfaces_IDL_TUPLES})
set(_output_path "${CMAKE_CURRENT_BINARY_DIR}/rosidlcpp_generator_c/${PROJECT_NAME}")
set(_generated_headers "")
set(_generated_sources "")
foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
  get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
  get_filename_component(_parent_folder "${_parent_folder}" NAME)
  get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
  string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
  list(APPEND _generated_headers
    "${_output_path}/${_parent_folder}/${_header_name}.h"
    "${_output_path}/${_parent_folder}/detail/${_header_name}__functions.h"
    "${_output_path}/${_parent_folder}/detail/${_header_name}__struct.h"
    "${_output_path}/${_parent_folder}/detail/${_header_name}__type_support.h"
  )
  list(APPEND _generated_sources
    "${_output_path}/${_parent_folder}/detail/${_header_name}__description.c"
    "${_output_path}/${_parent_folder}/detail/${_header_name}__functions.c"
    "${_output_path}/${_parent_folder}/detail/${_header_name}__type_support.c"
  )
endforeach()

set(_dependency_files "")
set(_dependencies "")
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  foreach(_idl_file ${${_pkg_name}_IDL_FILES})
    rosidl_find_package_idl(_abs_idl_file "${_pkg_name}" "${_idl_file}")
    list(APPEND _dependency_files "${_abs_idl_file}")
    list(APPEND _dependencies "${_pkg_name}:${_abs_idl_file}")
  endforeach()
endforeach()

set(target_dependencies
  "${rosidlcpp_generator_c_BIN}"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/action__type_support.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/action__type_support.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/empty__description.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/full__description.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl__description.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl__functions.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl__functions.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl__struct.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl__type_support.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/idl__type_support.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/msg__functions.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/msg__functions.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/msg__struct.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/msg__type_support.h.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/srv__type_support.c.em"
  "${rosidlcpp_generator_c_TEMPLATE_DIR}/srv__type_support.h.em"
  ${rosidl_generate_interfaces_ABS_IDL_FILES}
  ${_dependency_files})
foreach(dep ${target_dependencies})
  if(NOT EXISTS "${dep}")
    message(FATAL_ERROR "Target dependency '${dep}' does not exist")
  endif()
endforeach()

get_target_property(_target_sources ${rosidl_generate_interfaces_TARGET} SOURCES)
set(generator_arguments_file "${CMAKE_CURRENT_BINARY_DIR}/rosidlcpp_generator_c__arguments.json")
rosidl_write_generator_arguments(
  "${generator_arguments_file}"
  PACKAGE_NAME "${PROJECT_NAME}"
  IDL_TUPLES "${rosidl_generate_interfaces_c_IDL_TUPLES}"
  ROS_INTERFACE_DEPENDENCIES "${_dependencies}"
  OUTPUT_DIR "${_output_path}"
  TEMPLATE_DIR "${rosidlcpp_generator_c_TEMPLATE_DIR}"
  TARGET_DEPENDENCIES ${target_dependencies}
  TYPE_DESCRIPTION_TUPLES "${${rosidl_generate_interfaces_TARGET}__DESCRIPTION_TUPLES}"
  ROS_INTERFACE_FILES "${_target_sources}"
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

set(disable_description_codegen_arg)
if(ROSIDL_GENERATOR_C_DISABLE_TYPE_DESCRIPTION_CODEGEN)
  set(disable_description_codegen_arg "--disable-description-codegen")
endif()

add_custom_command(
  OUTPUT ${_generated_headers} ${_generated_sources}
  COMMAND ${rosidlcpp_generator_c_BIN}
  --generator-arguments-file "${generator_arguments_file}"
  ${disable_description_codegen_arg}
  DEPENDS ${target_dependencies}
  COMMENT "Generating C code for ROS interfaces"
  VERBATIM
)

# generate header to switch between export and import for a specific package
set(_visibility_control_file
  "${_output_path}/msg/rosidl_generator_c__visibility_control.h")
string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)
configure_file(
  "${rosidl_generator_c_TEMPLATE_DIR}/rosidl_generator_c__visibility_control.h.in"
  "${_visibility_control_file}"
  @ONLY
)

list(APPEND _generated_msg_headers "${_visibility_control_file}")

set(_target_suffix "__rosidlcpp_generator_c")

add_library(${rosidl_generate_interfaces_TARGET}${_target_suffix} SHARED # TODO: Re-add option
  ${_generated_headers} ${_generated_sources})
add_library(${PROJECT_NAME}::${rosidl_generate_interfaces_TARGET}${_target_suffix} ALIAS
  ${rosidl_generate_interfaces_TARGET}${_target_suffix})
if(rosidl_generate_interfaces_LIBRARY_NAME)
  set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PROPERTIES OUTPUT_NAME "${rosidl_generate_interfaces_LIBRARY_NAME}${_target_suffix}")
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix} PROPERTIES
    C_STANDARD 11
    COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
endif()
set_property(TARGET ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PROPERTY DEFINE_SYMBOL "ROSIDL_GENERATOR_C_BUILDING_DLL_${PROJECT_NAME}")
target_include_directories(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidlcpp_generator_c>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
)
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  # Depend on targets generated by this generator in dependency packages
  target_link_libraries(
    ${rosidl_generate_interfaces_TARGET}${_target_suffix} PUBLIC
    ${${_pkg_name}_TARGETS${_target_suffix}})
endforeach()

target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PUBLIC
  rosidl_runtime_c::rosidl_runtime_c
  rosidl_typesupport_interface::rosidl_typesupport_interface
  rcutils::rcutils)
add_dependencies(
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  ${rosidl_generate_interfaces_TARGET}__rosidl_generator_type_description)

# Make top level generation target depend on this generated library
add_dependencies(
  ${rosidl_generate_interfaces_TARGET}
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
)

if(NOT rosidl_generate_interfaces_SKIP_INSTALL)
  install(
    DIRECTORY ${_output_path}/
    DESTINATION "include/${PROJECT_NAME}/${PROJECT_NAME}"
    PATTERN "*.h"
  )

  # Export old-style CMake variables
  ament_export_include_directories("include/${PROJECT_NAME}")
  ament_export_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix})

  # Export modern CMake targets
  ament_export_targets(export_${rosidl_generate_interfaces_TARGET}${_target_suffix})
  rosidl_export_typesupport_targets(${_target_suffix}
    ${rosidl_generate_interfaces_TARGET}${_target_suffix})

  install(
    TARGETS ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    EXPORT export_${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )

  ament_export_dependencies(
    "rosidl_runtime_c"
    "rosidl_typesupport_interface"
    "rcutils")
endif()

if(BUILD_TESTING AND rosidl_generate_interfaces_ADD_LINTER_TESTS)
  find_package(ament_cmake_cppcheck REQUIRED)
  ament_cppcheck(
    TESTNAME "cppcheck_rosidl_generated_c"
    "${_output_path}")

  find_package(ament_cmake_cpplint REQUIRED)
  get_filename_component(_cpplint_root "${_output_path}" DIRECTORY)
  ament_cpplint(
    TESTNAME "cpplint_rosidl_generated_c"
    # the generated code might contain longer lines for templated types
    MAX_LINE_LENGTH 999
    ROOT "${_cpplint_root}"
    "${_output_path}")

  find_package(ament_cmake_uncrustify REQUIRED)
  ament_uncrustify(
    TESTNAME "uncrustify_rosidl_generated_c"
    # the generated code might contain longer lines for templated types
    # a value of zero tells uncrustify to ignore line length
    MAX_LINE_LENGTH 0
    "${_output_path}")
endif()
