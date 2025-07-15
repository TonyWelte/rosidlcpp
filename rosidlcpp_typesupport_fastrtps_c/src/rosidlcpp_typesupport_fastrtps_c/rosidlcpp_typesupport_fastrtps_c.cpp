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

#include <rosidlcpp_typesupport_fastrtps_c/rosidlcpp_typesupport_fastrtps_c.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <inja/inja.hpp>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

auto get_includes(const nlohmann::json& message) -> nlohmann::json {
  nlohmann::json includes_json = nlohmann::json::array();

  // TODO: Use a custom map sorted by insertion order
  std::vector<std::pair<std::string, std::vector<std::string>>> header_to_members;
  auto append_header_to_members = [](std::vector<std::pair<std::string, std::vector<std::string>>>& header_to_members, const std::string& header, const std::string& member) {
    auto it = std::ranges::find_if(header_to_members, [header](const auto& v) { return v.first == header; });
    if (it == header_to_members.end()) {
      it = header_to_members.insert(it, {header, {}});
    }
    it->second.push_back(member);
  };

  for (const auto& member : message["members"]) {
    if (rosidlcpp_core::is_sequence(member["type"])) {
      if (rosidlcpp_core::is_primitive(member["type"]["value_type"])) {
        append_header_to_members(header_to_members, "rosidl_runtime_c/primitives_sequence.h", member["name"]);
        append_header_to_members(header_to_members, "rosidl_runtime_c/primitives_sequence_functions.h", member["name"]);
        continue;
      }
    }

    auto type = member["type"];
    if (rosidlcpp_core::is_nestedtype(type)) {
      type = type["value_type"];
    }

    if (type["name"] == "string") {
      append_header_to_members(header_to_members, "rosidl_runtime_c/string.h", member["name"]);
      append_header_to_members(header_to_members, "rosidl_runtime_c/string_functions.h", member["name"]);
    } else if (type["name"] == "wstring") {
      append_header_to_members(header_to_members, "rosidl_runtime_c/u16string.h", member["name"]);
      append_header_to_members(header_to_members, "rosidl_runtime_c/u16string_functions.h", member["name"]);
    } else if (rosidlcpp_core::is_namespaced(type)) {
      if (type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_REQUEST_MESSAGE_SUFFIX) ||
          type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_RESPONSE_MESSAGE_SUFFIX) ||
          type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_EVENT_MESSAGE_SUFFIX)) {
        continue;
      }
      if (type["name"].get<std::string>().ends_with(rosidlcpp_core::ACTION_GOAL_SUFFIX) ||
          type["name"].get<std::string>().ends_with(rosidlcpp_core::ACTION_RESULT_SUFFIX) ||
          type["name"].get<std::string>().ends_with(rosidlcpp_core::ACTION_FEEDBACK_SUFFIX)) {
        auto type_name = type["name"].get<std::string>().substr(0, type["name"].get<std::string>().find('_'));
        type["name"] = type_name;
      }
      auto include_prefix = rosidlcpp_core::idl_structure_type_to_c_include_prefix(type, "detail");
      append_header_to_members(header_to_members, include_prefix + "__functions.h", member["name"]);
    }
  }

  std::sort(header_to_members.begin(), header_to_members.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& [header, members] : header_to_members) {
    includes_json.push_back({{"header_file", header}, {"member_names", members}});
  }

  return includes_json;
}

auto generate_member_for_cdr_serialize(const nlohmann::json& member, const std::string& suffix) -> std::vector<std::string> {
  std::vector<std::string> strlist;
  strlist.emplace_back("// Field name: " + member["name"].get<std::string>());
  strlist.emplace_back("{");

  auto type = member["type"];
  if (rosidlcpp_core::is_nestedtype(type)) {
    type = type["value_type"];
  }

  if (rosidlcpp_core::is_nestedtype(member["type"])) {
    if (rosidlcpp_core::is_array(member["type"])) {
      strlist.emplace_back(fmt::format("  size_t size = {};", member["type"]["size"].get<size_t>()));
      strlist.emplace_back("  auto array_ptr = ros_message->" + member["name"].get<std::string>() + ";");
    } else {
      strlist.emplace_back("  size_t size = ros_message->" + member["name"].get<std::string>() + ".size;");
      strlist.emplace_back("  auto array_ptr = ros_message->" + member["name"].get<std::string>() + ".data;");
      if (rosidlcpp_core::is_bounded(member["type"])) {
        strlist.emplace_back("  if (size > " + std::to_string(member["type"]["maximum_size"].get<size_t>()) + ") {");
        strlist.emplace_back("    fprintf(stderr, \"array size exceeds upper bound\\n\");");
        strlist.emplace_back("    return false;");
        strlist.emplace_back("  }");
      }
      strlist.emplace_back("  cdr << static_cast<uint32_t>(size);");
    }
    if (type["name"] == "string") {
      strlist.emplace_back("  for (size_t i = 0; i < size; ++i) {");
      strlist.emplace_back("    const rosidl_runtime_c__String * str = &array_ptr[i];");
      strlist.emplace_back("    if (str->capacity == 0 || str->capacity <= str->size) {");
      strlist.emplace_back("      fprintf(stderr, \"string capacity not greater than size\\n\");");
      strlist.emplace_back("      return false;");
      strlist.emplace_back("    }");
      strlist.emplace_back("    if (str->data[str->size] != '\\0') {");
      strlist.emplace_back("      fprintf(stderr, \"string not null-terminated\\n\");");
      strlist.emplace_back("      return false;");
      strlist.emplace_back("    }");
      strlist.emplace_back("    cdr << str->data;");
      strlist.emplace_back("  }");
    } else if (type["name"] == "wstring") {
      strlist.emplace_back("  for (size_t i = 0; i < size; ++i) {");
      strlist.emplace_back("    const rosidl_runtime_c__U16String * str = &array_ptr[i];");
      strlist.emplace_back("    if (str->capacity == 0 || str->capacity <= str->size) {");
      strlist.emplace_back("      fprintf(stderr, \"string capacity not greater than size\\n\");");
      strlist.emplace_back("      return false;");
      strlist.emplace_back("    }");
      strlist.emplace_back("    if (str->data[str->size] != '\\0') {");
      strlist.emplace_back("      fprintf(stderr, \"string not null-terminated\\n\");");
      strlist.emplace_back("      return false;");
      strlist.emplace_back("    }");
      strlist.emplace_back("    rosidl_typesupport_fastrtps_c::cdr_serialize(cdr, *str);");
      strlist.emplace_back("  }");
    } else if (rosidlcpp_core::is_primitive(type) && type["name"] == "wchar") {
      strlist.emplace_back("  for (size_t i = 0; i < size; ++i) {");
      strlist.emplace_back("    cdr_serialize" + suffix + "_" + fmt::format("{}__{}(", fmt::join(type["namespaces"], "__"), type["name"]));
      strlist.emplace_back("      &array_ptr[i], cdr);");
      strlist.emplace_back("  }");
    } else if (rosidlcpp_core::is_primitive(type)) {
      strlist.emplace_back("  cdr.serialize_array(array_ptr, size);");
    } else {
      strlist.emplace_back("  for (size_t i = 0; i < size; ++i) {");
      strlist.emplace_back("    cdr_serialize" + suffix + "_" + fmt::format("{}__{}(", fmt::join(type["namespaces"], "__"), type["name"]));
      strlist.emplace_back("      &array_ptr[i], cdr);");
      strlist.emplace_back("  }");
    }
  } else if (type["name"] == "string") {
    strlist.emplace_back("  const rosidl_runtime_c__String * str = &ros_message->" + member["name"].get<std::string>() + ";");
    strlist.emplace_back("  if (str->capacity == 0 || str->capacity <= str->size) {");
    strlist.emplace_back("    fprintf(stderr, \"string capacity not greater than size\\n\");");
    strlist.emplace_back("    return false;");
    strlist.emplace_back("  }");
    strlist.emplace_back("  if (str->data[str->size] != '\\0') {");
    strlist.emplace_back("    fprintf(stderr, \"string not null-terminated\\n\");");
    strlist.emplace_back("    return false;");
    strlist.emplace_back("  }");
    strlist.emplace_back("  cdr << str->data;");
  } else if (type["name"] == "wstring") {
    strlist.emplace_back("  rosidl_typesupport_fastrtps_c::cdr_serialize(cdr, ros_message->" + member["name"].get<std::string>() + ");");
  } else if (rosidlcpp_core::is_primitive(type) && type["name"] == "boolean") {
    strlist.emplace_back("  cdr << (ros_message->" + member["name"].get<std::string>() + " ? true : false);");
  } else if (rosidlcpp_core::is_primitive(type) && type["name"] == "wchar") {
    strlist.emplace_back("  cdr << static_cast<wchar_t>(ros_message->" + member["name"].get<std::string>() + ");");
  } else if (rosidlcpp_core::is_primitive(type)) {
    strlist.emplace_back("  cdr << ros_message->" + member["name"].get<std::string>() + ";");
  } else {
    strlist.emplace_back("  cdr_serialize" + suffix + "_" + fmt::format("{}__{}(", fmt::join(type["namespaces"], "__"), type["name"]));
    strlist.emplace_back("    &ros_message->" + member["name"].get<std::string>() + ", cdr);");
  }
  strlist.emplace_back("}");

  return strlist;
}
auto generate_member_for_get_serialized_size(const nlohmann::json& member, const std::string& suffix) -> std::vector<std::string> {
  std::vector<std::string> strlist;
  strlist.emplace_back("// Field name: " + member["name"].get<std::string>());

  if (rosidlcpp_core::is_nestedtype(member["type"])) {
    strlist.emplace_back("{");
    if (rosidlcpp_core::is_array(member["type"])) {
      strlist.emplace_back("  size_t array_size = " + std::to_string(member["type"]["size"].get<size_t>()) + ";");
      strlist.emplace_back("  auto array_ptr = ros_message->" + member["name"].get<std::string>() + ";");
    } else {
      strlist.emplace_back("  size_t array_size = ros_message->" + member["name"].get<std::string>() + ".size;");
      strlist.emplace_back("  auto array_ptr = ros_message->" + member["name"].get<std::string>() + ".data;");
      strlist.emplace_back("  current_alignment += padding +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, padding);");
    }
    if (rosidlcpp_core::is_string(member["type"]["value_type"])) {
      strlist.emplace_back("  for (size_t index = 0; index < array_size; ++index) {");
      strlist.emplace_back("    current_alignment += padding +");
      strlist.emplace_back("      eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +");
      if (member["type"]["value_type"]["name"] == "wstring") {
        strlist.emplace_back("      wchar_size *");
      }
      strlist.emplace_back("      (array_ptr[index].size + 1);");
      strlist.emplace_back("  }");
    } else if (rosidlcpp_core::is_primitive(member["type"]["value_type"])) {
      strlist.emplace_back("  (void)array_ptr;");
      strlist.emplace_back("  size_t item_size = sizeof(array_ptr[0]);");
      strlist.emplace_back("  current_alignment += array_size * item_size +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);");
    } else {
      strlist.emplace_back("  for (size_t index = 0; index < array_size; ++index) {");
      strlist.emplace_back("    current_alignment += get_serialized_size" + suffix + "_" + fmt::format("{}__{}", fmt::join(member["type"]["value_type"]["namespaces"], "__"), member["type"]["value_type"]["name"]) + "(");
      strlist.emplace_back("      &array_ptr[index], current_alignment);");
      strlist.emplace_back("  }");
    }
    strlist.emplace_back("}");
  } else {
    if (rosidlcpp_core::is_string(member["type"])) {
      strlist.emplace_back("current_alignment += padding +");
      strlist.emplace_back("  eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +");
      if (member["type"]["name"] == "wstring") {
        strlist.emplace_back("  wchar_size *");
      }
      strlist.emplace_back("  (ros_message->" + member["name"].get<std::string>() + ".size + 1);");
    } else if (rosidlcpp_core::is_primitive(member["type"])) {
      strlist.emplace_back("{");
      strlist.emplace_back("  size_t item_size = sizeof(ros_message->" + member["name"].get<std::string>() + ");");
      strlist.emplace_back("  current_alignment += item_size +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);");
      strlist.emplace_back("}");
    } else {
      strlist.emplace_back("current_alignment += get_serialized_size" + suffix + "_" + fmt::format("{}__{}", fmt::join(member["type"]["namespaces"], "__"), member["type"]["name"]) + "(");
      strlist.emplace_back("  &(ros_message->" + member["name"].get<std::string>() + "), current_alignment);");
    }
  }
  return strlist;
}
auto generate_member_for_max_serialized_size(const nlohmann::json& member, const std::string& suffix) -> std::vector<std::string> {
  std::vector<std::string> strlist;
  strlist.emplace_back("// Field name: " + member["name"].get<std::string>());
  strlist.emplace_back("{");

  if (rosidlcpp_core::is_nestedtype(member["type"])) {
    if (rosidlcpp_core::is_array(member["type"])) {
      strlist.emplace_back("  size_t array_size = " + std::to_string(member["type"]["size"].get<size_t>()) + ";");
    } else if (rosidlcpp_core::is_bounded(member["type"])) {
      strlist.emplace_back("  size_t array_size = " + std::to_string(member["type"]["maximum_size"].get<size_t>()) + ";");
    } else {
      strlist.emplace_back("  size_t array_size = 0;");
      strlist.emplace_back("  full_bounded = false;");
    }
    if (rosidlcpp_core::is_sequence(member["type"])) {
      strlist.emplace_back("  is_plain = false;");
      strlist.emplace_back("  current_alignment += padding +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, padding);");
    }
  } else {
    strlist.emplace_back("  size_t array_size = 1;");
  }

  auto type = member["type"];
  if (rosidlcpp_core::is_nestedtype(type)) {
    type = type["value_type"];
  }

  if (rosidlcpp_core::is_string(type)) {
    strlist.emplace_back("  full_bounded = false;");
    strlist.emplace_back("  is_plain = false;");
    strlist.emplace_back("  for (size_t index = 0; index < array_size; ++index) {");
    strlist.emplace_back("    current_alignment += padding +");
    strlist.emplace_back("      eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +");
    if (type.contains("maximum_size")) {
      if (type["name"] == "wstring") {
        strlist.emplace_back("      wchar_size *");
      }
      strlist.emplace_back("      " + std::to_string(type["maximum_size"].get<size_t>()) + " +");
    }
    if (type["name"] == "wstring") {
      strlist.emplace_back("      wchar_size *");
    }
    strlist.emplace_back("      1;");
    strlist.emplace_back("  }");
  } else if (rosidlcpp_core::is_primitive(type)) {
    if (type["name"] == "boolean" || type["name"] == "octet" || type["name"] == "char" || type["name"] == "uint8" || type["name"] == "int8") {
      strlist.emplace_back("  last_member_size = array_size * sizeof(uint8_t);");
      strlist.emplace_back("  current_alignment += array_size * sizeof(uint8_t);");
    } else if (type["name"] == "wchar" || type["name"] == "int16" || type["name"] == "uint16") {
      strlist.emplace_back("  last_member_size = array_size * sizeof(uint16_t);");
      strlist.emplace_back("  current_alignment += array_size * sizeof(uint16_t) +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));");
    } else if (type["name"] == "int32" || type["name"] == "uint32" || type["name"] == "float") {
      strlist.emplace_back("  last_member_size = array_size * sizeof(uint32_t);");
      strlist.emplace_back("  current_alignment += array_size * sizeof(uint32_t) +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));");
    } else if (type["name"] == "int64" || type["name"] == "uint64" || type["name"] == "double") {
      strlist.emplace_back("  last_member_size = array_size * sizeof(uint64_t);");
      strlist.emplace_back("  current_alignment += array_size * sizeof(uint64_t) +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));");
    } else if (type["name"] == "long double") {
      strlist.emplace_back("  last_member_size = array_size * sizeof(long double);");
      strlist.emplace_back("  current_alignment += array_size * sizeof(long double) +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(long double));");
    }
  } else {
    strlist.emplace_back("  last_member_size = 0;");
    strlist.emplace_back("  for (size_t index = 0; index < array_size; ++index) {");
    strlist.emplace_back("    bool inner_full_bounded;");
    strlist.emplace_back("    bool inner_is_plain;");
    strlist.emplace_back("    size_t inner_size;");
    strlist.emplace_back("    inner_size =");
    strlist.emplace_back("      " + fmt::format("max_serialized_size{}_{}__{}(", suffix, fmt::join(type["namespaces"], "__"), type["name"]));
    strlist.emplace_back("      inner_full_bounded, inner_is_plain, current_alignment);");
    strlist.emplace_back("    last_member_size += inner_size;");
    strlist.emplace_back("    current_alignment += inner_size;");
    strlist.emplace_back("    full_bounded &= inner_full_bounded;");
    strlist.emplace_back("    is_plain &= inner_is_plain;");
    strlist.emplace_back("  }");
  }
  strlist.emplace_back("}");
  return strlist;
}
auto sort_forward_declared_types(std::vector<nlohmann::json> types) -> std::vector<nlohmann::json> {
  std::sort(types.begin(), types.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
    for (size_t i = 0; i < a["namespaces"].size(); i++) {
      if (a["namespaces"][i].get<std::string>() != b["namespaces"][i].get<std::string>()) {
        return a["namespaces"][i].get<std::string>() < b["namespaces"][i].get<std::string>();
      }
    }
    return a["name"].get<std::string>() < b["name"].get<std::string>();
  });
  return types;
}

GeneratorTypesupportFastrtpsC::GeneratorTypesupportFastrtpsC(rosidlcpp_core::GeneratorArguments generator_arguments) : GeneratorBase(), m_arguments(std::move(generator_arguments)) {
  set_input_path(m_arguments.template_dir + "/");
  set_output_path(m_arguments.output_dir + "/");

  GENERATOR_BASE_REGISTER_FUNCTION("get_includes", 1, get_includes);
  GENERATOR_BASE_REGISTER_FUNCTION("generate_member_for_cdr_serialize", 2, generate_member_for_cdr_serialize);
  GENERATOR_BASE_REGISTER_FUNCTION("generate_member_for_get_serialized_size", 2, generate_member_for_get_serialized_size);
  GENERATOR_BASE_REGISTER_FUNCTION("generate_member_for_max_serialized_size", 2, generate_member_for_max_serialized_size);
  GENERATOR_BASE_REGISTER_FUNCTION("sort_forward_declared_types", 1, sort_forward_declared_types);
}

void GeneratorTypesupportFastrtpsC::run() {
  // Load templates
  auto template_idl = parse_template("./idl__type_support_c.cpp.template");
  auto template_idl_rosidl = parse_template("./idl__rosidl_typesupport_fastrtps_c.h.template");

  // Generate message specific files
  for (const auto& [path, file_path] : m_arguments.idl_tuples) {
    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    ros_json["package_name"] = m_arguments.package_name;

    const auto msg_directory = ros_json["interface_path"]["filedir"].get<std::string>();
    const auto msg_type = ros_json["interface_path"]["filename"].get<std::string>();
    std::filesystem::create_directories(m_arguments.output_dir + "/" + msg_directory + "/detail");
    write_template(template_idl, ros_json,
                   fmt::format("{}/detail/{}__type_support_c.cpp", msg_directory,
                               rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_rosidl, ros_json,
                   fmt::format("{}/detail/{}__rosidl_typesupport_fastrtps_c.h", msg_directory,
                               rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

int main(int argc, char** argv) {
  /**
   * CLI Arguments
   */
  argparse::ArgumentParser argument_parser("rosidlcpp_typesupport_fastrtps_c");
  argument_parser.add_argument("--generator-arguments-file").required().help("The location of the file containing the generator arguments");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    std::cerr << argument_parser;
    return 1;
  }

  auto generator_arguments_file = argument_parser.get<std::string>("--generator-arguments-file");
  auto generator_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  /**
   * Generation
   */
  GeneratorTypesupportFastrtpsC generator(generator_arguments);
  generator.run();
  return 0;
}
