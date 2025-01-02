macro(rosidlcpp_generator_py_extras BIN TEMPLATE_DIR)
  find_package(ament_cmake_core QUIET REQUIRED)
  # Make sure extension points are registered in order
  find_package(rosidl_generator_c QUIET REQUIRED)
  find_package(rosidl_typesupport_c QUIET REQUIRED)

  ament_register_extension(
    "rosidl_generate_idl_interfaces"
    "rosidlcpp_generator_py"
    "rosidlcpp_generator_py_generate_interfaces.cmake")

  normalize_path(BIN "${BIN}")
  set(rosidlcpp_generator_py_BIN "${BIN}")

  normalize_path(TEMPLATE_DIR "${TEMPLATE_DIR}")
  set(rosidlcpp_generator_py_TEMPLATE_DIR "${TEMPLATE_DIR}")
endmacro()