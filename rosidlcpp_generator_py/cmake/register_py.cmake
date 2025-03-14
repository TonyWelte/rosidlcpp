macro(rosidlcpp_generator_py_extras BIN TEMPLATE_DIR)
  if(NOT USE_ROSIDL_GENERATORS)
    find_package(ament_cmake_core QUIET REQUIRED)
    # Make sure extension points are registered in order
    find_package(rosidl_generator_c QUIET REQUIRED)
    find_package(rosidl_typesupport_c QUIET REQUIRED)

    # Replace rosidl_generator_py with rosidlcpp_generator_py
    find_package(rosidl_generator_py QUIET)  # Needs to be loaded for its generator can be replace
    list(
      TRANSFORM AMENT_EXTENSIONS_rosidl_generate_idl_interfaces
      REPLACE "rosidl_generator_py:rosidl_generator_py_generate_interfaces.cmake"
      "rosidlcpp_generator_py:rosidlcpp_generator_py_generate_interfaces.cmake"
    )

    normalize_path(BIN "${BIN}")
    set(rosidlcpp_generator_py_BIN "${BIN}")

    normalize_path(TEMPLATE_DIR "${TEMPLATE_DIR}")
    set(rosidlcpp_generator_py_TEMPLATE_DIR "${TEMPLATE_DIR}")
  endif()
endmacro()