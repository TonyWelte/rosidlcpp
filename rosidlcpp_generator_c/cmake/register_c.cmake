# Copyright 2015 Open Source Robotics Foundation, Inc.
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

macro(rosidlcpp_generator_c_extras BIN TEMPLATE_DIR)
  if(NOT USE_ROSIDL_GENERATORS)
    find_package(ament_cmake_core QUIET REQUIRED)
    find_package(rosidl_generator_type_description QUIET REQUIRED)

    # Replace rosidl_generator_c with rosidlcpp_generator_c
    find_package(rosidl_generator_c QUIET)  # Needs to be loaded for its generator can be replace
    list(
      TRANSFORM AMENT_EXTENSIONS_rosidl_generate_idl_interfaces
      REPLACE "rosidl_generator_c:rosidl_generator_c_generate_interfaces.cmake"
      "rosidlcpp_generator_c:rosidlcpp_generator_c_generate_interfaces.cmake"
    )

    normalize_path(BIN "${BIN}")
    set(rosidlcpp_generator_c_BIN "${BIN}")

    normalize_path(TEMPLATE_DIR "${TEMPLATE_DIR}")
    set(rosidlcpp_generator_c_TEMPLATE_DIR "${TEMPLATE_DIR}")
  endif()
endmacro()
