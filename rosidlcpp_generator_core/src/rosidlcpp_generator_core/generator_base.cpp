#include <rosidlcpp_generator_core/generator_base.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <inja/inja.hpp>

#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

namespace rosidlcpp_core {

GeneratorBase::GeneratorBase() : m_env{} {
  m_env.set_trim_blocks(true);
  m_env.set_lstrip_blocks(true);

  /*
   * Register callbacks
   */

  // Debug
  GENERATOR_BASE_REGISTER_VOID_FUNCTION("debug", 1, print_indented_json);

  // Formatting
  GENERATOR_BASE_REGISTER_FUNCTION("convert_camel_case_to_lower_case_underscore", 1, camel_to_snake);
  GENERATOR_BASE_REGISTER_FUNCTION("format", 2, format_string);
  GENERATOR_BASE_REGISTER_FUNCTION("format", 3, format_string);
  GENERATOR_BASE_REGISTER_FUNCTION("replace", 3, replace_string);

  // Constants
  register_variable("EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME", EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME);
  register_variable("SERVICE_EVENT_MESSAGE_SUFFIX", SERVICE_EVENT_MESSAGE_SUFFIX);
  register_variable("SERVICE_REQUEST_MESSAGE_SUFFIX", SERVICE_REQUEST_MESSAGE_SUFFIX);
  register_variable("SERVICE_RESPONSE_MESSAGE_SUFFIX", SERVICE_RESPONSE_MESSAGE_SUFFIX);
  register_variable("ACTION_GOAL_SUFFIX", ACTION_GOAL_SUFFIX);
  register_variable("ACTION_RESULT_SUFFIX", ACTION_RESULT_SUFFIX);
  register_variable("ACTION_FEEDBACK_SUFFIX", ACTION_FEEDBACK_SUFFIX);
  register_variable("ACTION_GOAL_SERVICE_SUFFIX", ACTION_GOAL_SERVICE_SUFFIX);
  register_variable("ACTION_RESULT_SERVICE_SUFFIX", ACTION_RESULT_SERVICE_SUFFIX);
  register_variable("ACTION_FEEDBACK_MESSAGE_SUFFIX", ACTION_FEEDBACK_MESSAGE_SUFFIX);

  // Utility
  GENERATOR_BASE_REGISTER_FUNCTION("span", 3, span);
  GENERATOR_BASE_REGISTER_FUNCTION("push_back", 2, push_back);
  GENERATOR_BASE_REGISTER_FUNCTION("insert", 3, insert);
  GENERATOR_BASE_REGISTER_FUNCTION("string_contains", 2, string_contains);

  m_env.add_callback("set_global_variable", 2, [&](inja::Arguments& args) {
    auto name = args.at(0)->get<std::string>();
    auto value = *args.at(1);
    m_global_storage[name] = value;
    return m_global_storage[name];
  });
  m_env.add_callback("get_global_variable", 1, [&](inja::Arguments& args) {
    auto name = args.at(0)->get<std::string>();
    return m_global_storage[name];
  });

  GENERATOR_BASE_REGISTER_FUNCTION("unique", 1, get_unique);
  GENERATOR_BASE_REGISTER_FUNCTION("split_string", 2, rosidlcpp_parser::split_string);
  GENERATOR_BASE_REGISTER_FUNCTION("custom_range", 3, custom_range);

  // Types
  GENERATOR_BASE_REGISTER_FUNCTION("is_primitive", 1, is_primitive);
  GENERATOR_BASE_REGISTER_FUNCTION("is_string", 1, is_string);
  GENERATOR_BASE_REGISTER_FUNCTION("is_character", 1, is_character);
  GENERATOR_BASE_REGISTER_FUNCTION("is_float", 1, is_float);
  GENERATOR_BASE_REGISTER_FUNCTION("is_nestedtype", 1, is_nestedtype);
  GENERATOR_BASE_REGISTER_FUNCTION("is_integer", 1, is_integer);
  GENERATOR_BASE_REGISTER_FUNCTION("is_signed_integer", 1, is_signed_integer);
  GENERATOR_BASE_REGISTER_FUNCTION("is_unsigned_integer", 1, is_unsigned_integer);
  GENERATOR_BASE_REGISTER_FUNCTION("is_namespaced", 1, is_namespaced);
  GENERATOR_BASE_REGISTER_FUNCTION("is_action_type", 1, is_action_type);
  GENERATOR_BASE_REGISTER_FUNCTION("is_service_type", 1, is_service_type);

  // C API
  register_variable("GET_DESCRIPTION_FUNC", "get_type_description");
  register_variable("GET_HASH_FUNC", "get_type_hash");
  register_variable("GET_INDIVIDUAL_SOURCE_FUNC", "get_individual_type_description_source");
  register_variable("GET_SOURCES_FUNC", "get_type_description_sources");
  GENERATOR_BASE_REGISTER_FUNCTION("idl_structure_type_to_c_typename", 1, type_to_c_typename);
}

std::vector<std::pair<std::string, std::string>> parse_pairs(const std::vector<std::string> list) {
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& v : list) {
    std::istringstream ss(v);

    std::string first, second;
    std::getline(ss, first, ':');
    std::getline(ss, second, ':');
    result.push_back({first, second});
  }
  return result;
}

GeneratorArguments parse_arguments(const std::string& filepath) {
  std::ifstream f(filepath);
  nlohmann::json data = nlohmann::json::parse(f);

  GeneratorArguments result;

  result.package_name = data["package_name"];
  result.output_dir = data["output_dir"];
  if (data.contains("template_dir")) {
    result.template_dir = data["template_dir"];
  }

  if (data.contains("idl_tuples")) {
    result.idl_tuples = parse_pairs(data["idl_tuples"]);
  }
  if (data.contains("ros_interface_files")) {
    result.ros_interface_files = data["ros_interface_files"];
  }
  if (data.contains("ros_interface_dependencies")) {
    result.ros_interface_dependencies = data["ros_interface_dependencies"];
  }
  if (data.contains("target_dependencies")) {
    result.target_dependencies = parse_pairs(data["target_dependencies"]);
  }
  if (data.contains("type_description_tuples")) {
    result.type_description_tuples =
        parse_pairs(data["type_description_tuples"]);
  }
  if (data.contains("include_paths")) {
    result.include_paths = parse_pairs(data["include_paths"]);
  }

  return result;
}

}  // namespace rosidlcpp_core