// Copyright 2025 Anthony Welte
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
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
  GENERATOR_BASE_REGISTER_CONSTANT("ROS_DISTRO", std::getenv("ROS_DISTRO"));
  GENERATOR_BASE_REGISTER_CONSTANT("EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME", EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME);
  GENERATOR_BASE_REGISTER_CONSTANT("SERVICE_EVENT_MESSAGE_SUFFIX", SERVICE_EVENT_MESSAGE_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("SERVICE_REQUEST_MESSAGE_SUFFIX", SERVICE_REQUEST_MESSAGE_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("SERVICE_RESPONSE_MESSAGE_SUFFIX", SERVICE_RESPONSE_MESSAGE_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("ACTION_GOAL_SUFFIX", ACTION_GOAL_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("ACTION_RESULT_SUFFIX", ACTION_RESULT_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("ACTION_FEEDBACK_SUFFIX", ACTION_FEEDBACK_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("ACTION_GOAL_SERVICE_SUFFIX", ACTION_GOAL_SERVICE_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("ACTION_RESULT_SERVICE_SUFFIX", ACTION_RESULT_SERVICE_SUFFIX);
  GENERATOR_BASE_REGISTER_CONSTANT("ACTION_FEEDBACK_MESSAGE_SUFFIX", ACTION_FEEDBACK_MESSAGE_SUFFIX);

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
  GENERATOR_BASE_REGISTER_CONSTANT("GET_DESCRIPTION_FUNC", "get_type_description");
  GENERATOR_BASE_REGISTER_CONSTANT("GET_HASH_FUNC", "get_type_hash");
  GENERATOR_BASE_REGISTER_CONSTANT("GET_INDIVIDUAL_SOURCE_FUNC", "get_individual_type_description_source");
  GENERATOR_BASE_REGISTER_CONSTANT("GET_SOURCES_FUNC", "get_type_description_sources");

  GENERATOR_BASE_REGISTER_FUNCTION("idl_structure_type_to_c_typename", 1, type_to_c_typename);
  GENERATOR_BASE_REGISTER_FUNCTION("basetype_to_c", 1, basetype_to_c);
  GENERATOR_BASE_REGISTER_FUNCTION("idl_type_to_c", 1, idl_type_to_c);

  // C++ API
  GENERATOR_BASE_REGISTER_FUNCTION("MSG_TYPE_TO_CPP", 1, cpp_typename);

  // Keys
  GENERATOR_BASE_REGISTER_FUNCTION("has_any_key", 1, has_any_key);
}

Template GeneratorBase::parse_template(std::string_view template_path) {
  return m_env.parse_template(std::string{template_path});
}

void GeneratorBase::register_callback(std::string_view name, int arg_count, const FunctionType& function) {
  m_env.add_callback(std::string{name}, arg_count, function);
}
void GeneratorBase::register_void_callback(std::string_view name, int arg_count, const VoidFunctionType& function) {
  m_env.add_void_callback(std::string{name}, arg_count, function);
}

void GeneratorBase::write_template(const inja::Template& template_object, const nlohmann::json& data, std::string_view output_file, bool add_bom_if_needed) {
  m_env.write_template(template_object, data, output_file, add_bom_if_needed);
}

/**
 * @brief Update the file if the contents are different from the new content.
 */
bool compare_and_write(const std::filesystem::path& file_path, const std::string& new_content) {
  std::ifstream in_file(file_path);
  std::string file_content((std::istreambuf_iterator<char>(in_file)), std::istreambuf_iterator<char>());

  if (file_content == new_content) {
    // Contents are the same, no need to update the file
    return false;
  } else {
    // Contents are different, update the file
    std::ofstream out_file(file_path);
    if (out_file.is_open()) {
      out_file << new_content;
      out_file.close();
      return true;
    } else {
      std::cerr << "Unable to open file for writing: " << file_path << std::endl;
      return false;
    }
  }
}

void GeneratorEnvironment::write_template(const inja::Template& template_object, const nlohmann::json& data, std::string_view output_file, bool add_bom_if_needed) {
  std::string result = render(template_object, data);

  if (add_bom_if_needed && rosidlcpp_parser::has_non_ascii(result)) {
    result = "\ufeff// NOLINT: This file starts with a BOM since it contain non-ASCII characters\n" + result;
  }

  // Check if the new content is different from the existing content
  // If it is different, write the new content to the file
  // If it is the same, do not write to the file
  // There is no measurable performance difference between this and updating the
  // file update date. This solution is safer.
  compare_and_write(std::filesystem::path{output_path} / output_file, result);
};

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