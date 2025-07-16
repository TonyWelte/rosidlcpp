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

#include <rosidlcpp_typesupport_fastrtps_cpp/rosidlcpp_typesupport_fastrtps_cpp.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <inja/inja.hpp>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
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
        append_header_to_members(header_to_members, "rosidl_runtime_c/primitives_sequence_functions.h", member["name"]);
        continue;
      }
    }

    auto type = member["type"];
    if (rosidlcpp_core::is_nestedtype(type)) {
      type = type["value_type"];
    }

    if (type["name"] == "string") {
      append_header_to_members(header_to_members, "rosidl_runtime_c/string_functions.h", member["name"]);
    } else if (type["name"] == "wstring") {
      append_header_to_members(header_to_members, "rosidl_runtime_c/u16string_functions.h", member["name"]);
    } else if (rosidlcpp_core::is_namespaced(type)) {
      if ((message["type"]["namespaces"].back() == "action" ||
           message["type"]["namespaces"].back() == "srv") &&
          (type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_REQUEST_MESSAGE_SUFFIX) ||
           type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_RESPONSE_MESSAGE_SUFFIX) ||
           type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_EVENT_MESSAGE_SUFFIX))) {
        auto type_name = type["name"].get<std::string>().substr(0, type["name"].get<std::string>().find('_'));
        type["name"] = type_name;
      }
      auto include_prefix_no_detail = rosidlcpp_core::idl_structure_type_to_c_include_prefix(type);
      append_header_to_members(header_to_members, include_prefix_no_detail + ".h", member["name"]);
      auto include_prefix = rosidlcpp_core::idl_structure_type_to_c_include_prefix(type, "detail");
      append_header_to_members(header_to_members, include_prefix + "__rosidl_typesupport_introspection_c.h", member["name"]);
    }
  }

  for (const auto& [header, members] : header_to_members) {
    includes_json.push_back({{"header_file", header}, {"member_names", members}});
  }

  return includes_json;
}

// TODO: Share with rosidl_generator_cpp
const std::unordered_map<std::string, std::string> MSG_TYPE_TO_CPP = {
    {"boolean", "bool"},
    {"octet", "unsigned char"},  // TODO change to std::byte with C++17
    {"char", "unsigned char"},
    {"wchar", "char16_t"},
    {"float", "float"},
    {"double", "double"},
    {"long double", "long double"},
    {"uint8", "uint8_t"},
    {"int8", "int8_t"},
    {"uint16", "uint16_t"},
    {"int16", "int16_t"},
    {"uint32", "uint32_t"},
    {"int32", "int32_t"},
    {"uint64", "uint64_t"},
    {"int64", "int64_t"},
    {"string",
     "std::basic_string<char, std::char_traits<char>, typename "
     "std::allocator_traits<ContainerAllocator>::template "
     "rebind_alloc<char>>"},
    {"wstring",
     "std::basic_string<char16_t, std::char_traits<char16_t>, typename "
     "std::allocator_traits<ContainerAllocator>::template "
     "rebind_alloc<char16_t>>"},
};

auto generate_member_for_cdr_serialize(const nlohmann::json& member, const std::string& suffix) -> std::vector<std::string> {
  std::vector<std::string> strlist;
  strlist.emplace_back("// Member: " + member["name"].get<std::string>());
  if (rosidlcpp_core::is_nestedtype(member["type"])) {
    strlist.emplace_back("{");
    if (rosidlcpp_core::is_array(member["type"])) {
      if (!rosidlcpp_core::is_namespaced(member["type"]["value_type"]) && member["type"]["value_type"]["name"] != "wstring") {
        strlist.emplace_back("  cdr << ros_message." + member["name"].get<std::string>() + ";");
      } else {
        strlist.emplace_back("  for (size_t i = 0; i < " + std::to_string(member["type"]["size"].get<size_t>()) + "; i++) {");
        if (rosidlcpp_core::is_namespaced(member["type"]["value_type"])) {
          strlist.emplace_back("    " + fmt::format("{}", fmt::join(member["type"]["value_type"]["namespaces"], "::")) + "::typesupport_fastrtps_cpp::cdr_serialize" + suffix + "(");
          strlist.emplace_back("      ros_message." + member["name"].get<std::string>() + "[i],");
          strlist.emplace_back("      cdr);");
        } else {
          strlist.emplace_back("    rosidl_typesupport_fastrtps_cpp::cdr_serialize(cdr, ros_message." + member["name"].get<std::string>() + "[i]);");
        }
        strlist.emplace_back("  }");
      }
    } else {
      if (rosidlcpp_core::is_bounded(member["type"]) || rosidlcpp_core::is_namespaced(member["type"]["value_type"]) || member["type"]["value_type"]["name"] == "wstring") {
        strlist.emplace_back("  size_t size = ros_message." + member["name"].get<std::string>() + ".size();");
        if (rosidlcpp_core::is_bounded(member["type"])) {
          strlist.emplace_back("  if (size > " + std::to_string(member["type"]["maximum_size"].get<size_t>()) + ") {");
          strlist.emplace_back("    throw std::runtime_error(\"array size exceeds upper bound\");");
          strlist.emplace_back("  }");
        }
      }
      if ((!rosidlcpp_core::is_namespaced(member["type"]["value_type"]) && member["type"]["value_type"]["name"] != "wstring") && !rosidlcpp_core::is_bounded(member["type"])) {
        strlist.emplace_back("  cdr << ros_message." + member["name"].get<std::string>() + ";");
      } else {
        strlist.emplace_back("  cdr << static_cast<uint32_t>(size);");
        if (rosidlcpp_core::is_primitive(member["type"]["value_type"]) && member["type"]["value_type"]["name"] != "boolean" && member["type"]["value_type"]["name"] != "wchar") {
          strlist.emplace_back("  if (size > 0) {");
          strlist.emplace_back("    cdr.serialize_array(&(ros_message." + member["name"].get<std::string>() + "[0]), size);");
          strlist.emplace_back("  }");
        } else {
          strlist.emplace_back("  for (size_t i = 0; i < size; i++) {");
          if (rosidlcpp_core::is_primitive(member["type"]["value_type"]) && member["type"]["value_type"]["name"] == "boolean") {
            strlist.emplace_back("    cdr << (ros_message." + member["name"].get<std::string>() + "[i] ? true : false);");
          } else if (rosidlcpp_core::is_primitive(member["type"]["value_type"]) && member["type"]["value_type"]["name"] == "wchar") {
            strlist.emplace_back("    cdr << static_cast<wchar_t>(ros_message." + member["name"].get<std::string>() + "[i]);");
          } else if (member["type"]["value_type"]["name"] == "wstring") {
            strlist.emplace_back("    rosidl_typesupport_fastrtps_cpp::cdr_serialize(cdr, ros_message." + member["name"].get<std::string>() + "[i]);");
          } else if (!rosidlcpp_core::is_namespaced(member["type"]["value_type"])) {
            strlist.emplace_back("    cdr << ros_message." + member["name"].get<std::string>() + "[i];");
          } else {
            strlist.emplace_back("    " + fmt::format("{}", fmt::join(member["type"]["value_type"]["namespaces"], "::"), member["type"]["value_type"]["name"]) + "::typesupport_fastrtps_cpp::cdr_serialize" + suffix + "(");
            strlist.emplace_back("      ros_message." + member["name"].get<std::string>() + "[i],");
            strlist.emplace_back("      cdr);");
          }
          strlist.emplace_back("  }");
        }
      }
    }
    strlist.emplace_back("}");
  } else if (rosidlcpp_core::is_primitive(member["type"]) && member["type"]["name"] == "boolean") {
    strlist.emplace_back("cdr << (ros_message." + member["name"].get<std::string>() + " ? true : false);");
  } else if (rosidlcpp_core::is_primitive(member["type"]) && member["type"]["name"] == "wchar") {
    strlist.emplace_back("cdr << static_cast<wchar_t>(ros_message." + member["name"].get<std::string>() + ");");
  } else if (member["type"]["name"] == "wstring") {
    strlist.emplace_back("{");
    strlist.emplace_back("  rosidl_typesupport_fastrtps_cpp::cdr_serialize(cdr, ros_message." + member["name"].get<std::string>() + ");");
    strlist.emplace_back("}");
  } else if (!rosidlcpp_core::is_namespaced(member["type"])) {
    strlist.emplace_back("cdr << ros_message." + member["name"].get<std::string>() + ";");
  } else {
    strlist.emplace_back(fmt::format("{}", fmt::join(member["type"]["namespaces"], "::"), member["type"]["name"]) + "::typesupport_fastrtps_cpp::cdr_serialize" + suffix + "(");
    strlist.emplace_back("  ros_message." + member["name"].get<std::string>() + ",");
    strlist.emplace_back("  cdr);");
  }
  return strlist;
}

auto generate_member_for_get_serialized_size(const nlohmann::json& member, const std::string& suffix) -> std::vector<std::string> {
  std::vector<std::string> strlist;
  strlist.emplace_back("// Member: " + member["name"].get<std::string>());

  if (rosidlcpp_core::is_nestedtype(member["type"])) {
    strlist.emplace_back("{");
    if (rosidlcpp_core::is_array(member["type"])) {
      strlist.emplace_back("  size_t array_size = " + std::to_string(member["type"]["size"].get<size_t>()) + ";");
    } else {
      strlist.emplace_back("  size_t array_size = ros_message." + member["name"].get<std::string>() + ".size();");
      if (rosidlcpp_core::is_bounded(member["type"])) {
        strlist.emplace_back("  if (array_size > " + std::to_string(member["type"]["maximum_size"].get<size_t>()) + ") {");
        strlist.emplace_back("    throw std::runtime_error(\"array size exceeds upper bound\");");
        strlist.emplace_back("  }");
      }
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
      strlist.emplace_back("      (ros_message." + member["name"].get<std::string>() + "[index].size() + 1);");
      strlist.emplace_back("  }");
    } else if (rosidlcpp_core::is_primitive(member["type"]["value_type"])) {
      strlist.emplace_back("  size_t item_size = sizeof(ros_message." + member["name"].get<std::string>() + "[0]);");
      strlist.emplace_back("  current_alignment += array_size * item_size +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);");
    } else {
      strlist.emplace_back("  for (size_t index = 0; index < array_size; ++index) {");
      strlist.emplace_back("    current_alignment +=");
      strlist.emplace_back("      " + fmt::format("{}", fmt::join(member["type"]["value_type"]["namespaces"], "::")) + "::typesupport_fastrtps_cpp::get_serialized_size" + suffix + "(");
      strlist.emplace_back("      ros_message." + member["name"].get<std::string>() + "[index], current_alignment);");
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
      strlist.emplace_back("  (ros_message." + member["name"].get<std::string>() + ".size() + 1);");
    } else if (rosidlcpp_core::is_primitive(member["type"])) {
      strlist.emplace_back("{");
      strlist.emplace_back("  size_t item_size = sizeof(ros_message." + member["name"].get<std::string>() + ");");
      strlist.emplace_back("  current_alignment += item_size +");
      strlist.emplace_back("    eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);");
      strlist.emplace_back("}");
    } else {
      strlist.emplace_back("current_alignment +=");
      strlist.emplace_back("  " + fmt::format("{}", fmt::join(member["type"]["namespaces"], "::"), member["type"]["name"]) + "::typesupport_fastrtps_cpp::get_serialized_size" + suffix + "(");
      strlist.emplace_back("  ros_message." + member["name"].get<std::string>() + ", current_alignment);");
    }
  }

  return strlist;
}
auto generate_member_for_max_serialized_size(const nlohmann::json& member, const std::string& suffix) -> std::vector<std::string> {
  std::vector<std::string> strlist;
  strlist.emplace_back("// Member: " + member["name"].get<std::string>());
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
    strlist.emplace_back("    size_t inner_size =");
    strlist.emplace_back("      " + fmt::format("{}", fmt::join(type["namespaces"], "::")) + "::typesupport_fastrtps_cpp::max_serialized_size" + suffix + "_" + type["name"].get<std::string>() + "(");
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

GeneratorTypesupportFastrtpsCpp::GeneratorTypesupportFastrtpsCpp(const rosidlcpp_core::GeneratorArguments& generator_arguments) : GeneratorBase(), m_arguments(generator_arguments) {
  set_input_path(m_arguments.template_dir + "/");
  set_output_path(m_arguments.output_dir + "/");

  GENERATOR_BASE_REGISTER_FUNCTION("get_includes", 1, get_includes);
  GENERATOR_BASE_REGISTER_FUNCTION("generate_member_for_cdr_serialize", 2, generate_member_for_cdr_serialize);
  GENERATOR_BASE_REGISTER_FUNCTION("generate_member_for_get_serialized_size", 2, generate_member_for_get_serialized_size);
  GENERATOR_BASE_REGISTER_FUNCTION("generate_member_for_max_serialized_size", 2, generate_member_for_max_serialized_size);
}

void GeneratorTypesupportFastrtpsCpp::run() {
  // Load templates
  auto template_idl = parse_template("./idl__type_support.cpp.template");
  auto template_idl_rosidl = parse_template("./idl__rosidl_typesupport_fastrtps_cpp.hpp.template");

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
    std::filesystem::create_directories(m_arguments.output_dir + "/" + msg_directory + "/detail/dds_fastrtps");
    write_template(template_idl, ros_json,
                   fmt::format("{}/detail/dds_fastrtps/{}__type_support.cpp", msg_directory,
                               rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_rosidl, ros_json,
                   fmt::format("{}/detail/{}__rosidl_typesupport_fastrtps_cpp.hpp", msg_directory,
                               rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

auto main(int argc, char** argv) -> int {
  /**
   * CLI Arguments
   */
  argparse::ArgumentParser argument_parser("rosidlcpp_typesupport_fastrtps_cpp");
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
  GeneratorTypesupportFastrtpsCpp generator(generator_arguments);
  generator.run();
  return 0;
}
