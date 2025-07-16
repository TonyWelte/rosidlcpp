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

#include <rosidlcpp_generator_c/rosidlcpp_generator_c.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <inja/inja.hpp>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <ostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

auto idl_structure_type_sequence_to_c_typename(const nlohmann::json& type) -> std::string {
  return rosidlcpp_core::type_to_c_typename(type) + "__Sequence";
}

auto get_includes(const nlohmann::json& message, const std::string& suffix) -> nlohmann::json {
  nlohmann::json includes_json = nlohmann::json::array();

  const std::string runtime_c_suffix = suffix != "__struct.h" ? std::string{std::string_view{suffix}.substr(1)} : ".h";

  // TODO: Use a custom map sorted by insertion order
  std::vector<std::pair<std::string, std::vector<std::string>>> header_to_members;
  auto append_header_to_members = [](std::vector<std::pair<std::string, std::vector<std::string>>>& header_to_members, const std::string& header, const std::string& member) {
    auto it = std::find_if(header_to_members.begin(), header_to_members.end(), [header](const auto& v) { return v.first == header; });
    if (it == header_to_members.end()) {
      it = header_to_members.insert(it, {header, {}});
    }
    it->second.push_back(member);
  };

  for (const auto& member : message["members"]) {
    if (rosidlcpp_core::is_sequence(member["type"])) {
      if (rosidlcpp_core::is_primitive(member["type"]["value_type"])) {
        append_header_to_members(header_to_members, "rosidl_runtime_c/primitives_sequence" + runtime_c_suffix, member["name"]);
        continue;
      }
    }

    auto type = member["type"];
    if (rosidlcpp_core::is_nestedtype(type)) {
      type = type["value_type"];
    }

    if (type["name"] == "string") {
      append_header_to_members(header_to_members, "rosidl_runtime_c/string" + runtime_c_suffix, member["name"]);
    } else if (type["name"] == "wstring") {
      append_header_to_members(header_to_members, "rosidl_runtime_c/u16string" + runtime_c_suffix, member["name"]);
    } else if (rosidlcpp_core::is_namespaced(type)) {
      if ((message["type"]["namespaces"].back() == "action" ||
           message["type"]["namespaces"].back() == "srv") &&
          (type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_REQUEST_MESSAGE_SUFFIX) ||
           type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_RESPONSE_MESSAGE_SUFFIX) ||
           type["name"].get<std::string>().ends_with(rosidlcpp_core::SERVICE_EVENT_MESSAGE_SUFFIX))) {
        auto type_name = type["name"].get<std::string>().substr(0, type["name"].get<std::string>().find('_'));
        if (suffix == "__struct.h" && type_name == message["type"]["name"].get<std::string>().substr(0, message["type"]["name"].get<std::string>().find('_'))) {
          continue;
        }
        type["name"] = type_name;
      }
      auto include_prefix = rosidlcpp_core::idl_structure_type_to_c_include_prefix(type, "detail");
      append_header_to_members(header_to_members, include_prefix + suffix, member["name"]);
    }
  }

  for (const auto& [header, members] : header_to_members) {
    includes_json.push_back({{"header_file", header}, {"member_names", members}});
  }

  return includes_json;
}

auto get_full_description_includes(const nlohmann::json& implicit_type_description, const nlohmann::json& toplevel_type_description) -> nlohmann::json {
  std::set<std::string> implicit_type_names;
  for (const auto& td : implicit_type_description) {
    implicit_type_names.insert(td["msg"]["type_description"]["type_name"].get<std::string>());
  }

  // TODO: Use a custom map sorted by insertion order
  std::vector<std::string> header_to_members;

  const auto& toplevel_msg = toplevel_type_description["msg"];

  for (const auto& referenced_td : toplevel_msg["referenced_type_descriptions"]) {
    if (implicit_type_names.find(referenced_td["type_name"].get<std::string>()) != implicit_type_names.end()) {
      continue;
    }

    auto type_parts = rosidlcpp_parser::split_string_view(referenced_td["type_name"].get<std::string>(), "/");
    nlohmann::json type = nlohmann::json::object();
    type["name"] = type_parts.back();
    type_parts.pop_back();
    type["namespaces"] = type_parts;

    auto include_prefix = rosidlcpp_core::idl_structure_type_to_c_include_prefix(type, "detail");
    header_to_members.push_back(include_prefix + "__functions.h");
  }

  return header_to_members;
}

nlohmann::json get_upper_bounds(const nlohmann::json& message) {
  nlohmann::json upper_bounds = nlohmann::json::array();

  for (const auto& member : message["members"]) {
    auto type = member["type"];
    if (rosidlcpp_core::is_sequence(type) && type.contains("maximum_size")) {
      upper_bounds.push_back({{"field_name", member["name"]},
                              {"enum_name", rosidlcpp_core::type_to_c_typename(message["type"]) + "__" + member["name"].get<std::string>() + "__MAX_SIZE"},
                              {"enum_value", type["maximum_size"]}});
    }
    if (rosidlcpp_core::is_nestedtype(type)) {
      type = type["value_type"];
    }
    if (rosidlcpp_core::is_string(type) && type.contains("maximum_size")) {
      upper_bounds.push_back({{"field_name", member["name"]},
                              {"enum_name", rosidlcpp_core::type_to_c_typename(message["type"]) + "__" + member["name"].get<std::string>() + "__MAX_STRING_SIZE"},
                              {"enum_value", type["maximum_size"]}});
    }
  }
  return upper_bounds;
}

const std::unordered_map<std::string, std::string> BASIC_IDL_TYPES_TO_C = {
    {"float", "float"},
    {"double", "double"},
    {"long double", "long double"},
    {"char", "signed char"},
    {"wchar", "uint16_t"},
    {"boolean", "bool"},
    {"octet", "uint8_t"},
    {"uint8", "uint8_t"},
    {"int8", "int8_t"},
    {"uint16", "uint16_t"},
    {"int16", "int16_t"},
    {"uint32", "uint32_t"},
    {"int32", "int32_t"},
    {"uint64", "uint64_t"},
    {"int64", "int64_t"},
};

std::string basetype_to_c(const nlohmann::json& type) {
  auto it = BASIC_IDL_TYPES_TO_C.find(type["name"].get<std::string>());
  if (it != BASIC_IDL_TYPES_TO_C.end()) {
    return it->second;
  }
  if (type["name"] == "string") {
    return "rosidl_runtime_c__String";
  }
  if (type["name"] == "wstring") {
    return "rosidl_runtime_c__U16String";
  }
  if (rosidlcpp_core::is_namespaced(type)) {
    return rosidlcpp_core::type_to_c_typename(type);
  }

  throw std::runtime_error("Unknown basetype: " + type.dump());
}

std::string basic_value_to_c(const nlohmann::json& type, const nlohmann::json& value) {
  if (type["name"] == "boolean") {
    return value.get<bool>() ? "true" : "false";
  }
  if (type["name"] == "int8" || type["name"] == "uint8" ||
      type["name"] == "int16" || type["name"] == "uint16" ||
      type["name"] == "char" || type["name"] == "octet") {
    return std::to_string(value.get<int>());
  }
  if (type["name"] == "int32") {
    if (value.get<int>() == std::numeric_limits<int32_t>::min()) {
      return "(-2147483647l - 1)";
    }
    return std::to_string(value.get<int>()) + "l";
  }
  if (type["name"] == "uint32") {
    return std::to_string(value.get<unsigned int>()) + "ul";
  }
  if (type["name"] == "int64") {
    if (value.get<long long>() == std::numeric_limits<int64_t>::min()) {
      return "(-9223372036854775807ll - 1)";
    }
    return std::to_string(value.get<long long>()) + "ll";
  }
  if (type["name"] == "uint64") {
    return std::to_string(value.get<unsigned long long>()) + "ull";
  }
  if (type["name"] == "float") {
    return value.dump() + "f";
  }
  if (type["name"] == "double") {
    return value.dump() + "l";
  }
  throw std::runtime_error("Unknown basic type: " + type.dump());
}

std::string value_to_c(const nlohmann::json& type, const nlohmann::json& value) {
  if (type["name"] == "string") {
    return "\"" + rosidlcpp_core::escape_string(value) + "\"";
  }
  if (type["name"] == "wstring") {
    return "u\"" + rosidlcpp_core::escape_string(value) + "\"";
  }
  return basic_value_to_c(type, value);
}

std::string idl_type_to_c(const nlohmann::json& type) {
  std::string c_type;
  if (rosidlcpp_core::is_array(type)) {
    throw std::runtime_error("The array size is part of the variable");
  }
  if (rosidlcpp_core::is_sequence(type)) {
    if (rosidlcpp_core::is_primitive(type["value_type"])) {
      c_type = "rosidl_runtime_c__" + type["value_type"]["name"].get<std::string>();
    } else {
      c_type = basetype_to_c(type["value_type"]);
    }
    c_type += "__Sequence";
    return c_type;
  }
  return basetype_to_c(type);
}

std::string idl_declaration_to_c(const nlohmann::json& type, const std::string& name) {
  if (rosidlcpp_core::is_string(type)) {
    return idl_type_to_c(type) + " " + name;
  }
  if (rosidlcpp_core::is_array(type)) {
    return idl_type_to_c(type["value_type"]) + " " + name + "[" + std::to_string(type["size"].get<int>()) + "]";
  }
  return idl_type_to_c(type) + " " + name;
}

nlohmann::json extract_full_type_description(const std::string& output_type_name, const nlohmann::json& type_map) {
  // Traverse reference graph to narrow down the references for the output type
  const auto& output_type = type_map.at(output_type_name);
  std::set<std::string> output_references;
  std::vector<std::string> process_queue;

  for (const auto& field : output_type["fields"]) {
    if (field["type"].contains("nested_type_name") && field["type"]["nested_type_name"] != "") {
      process_queue.push_back(field["type"]["nested_type_name"]);
    }
  }

  while (!process_queue.empty()) {
    std::string process_type = process_queue.back();
    process_queue.pop_back();
    if (output_references.find(process_type) == output_references.end()) {
      output_references.insert(process_type);
      for (const auto& field : type_map.at(process_type)["fields"]) {
        if (field["type"].contains("nested_type_name") && field["type"]["nested_type_name"] != "") {
          process_queue.push_back(field["type"]["nested_type_name"]);
        }
      }
    }
  }

  nlohmann::json referenced_type_descriptions = nlohmann::json::array();
  for (const auto& type_name : output_references) {
    referenced_type_descriptions.push_back(type_map.at(type_name));
  }

  return {
      {"type_description", output_type},
      {"referenced_type_descriptions", referenced_type_descriptions}};
}

nlohmann::json extract_subinterface(const nlohmann::json& type_description_msg, const std::string& field_name) {
  // Filter full TypeDescription to produce a TypeDescription for one of its fields' types.
  std::string output_type_name;
  for (const auto& field : type_description_msg["type_description"]["fields"]) {
    if (field["name"] == field_name) {
      output_type_name = field["type"]["nested_type_name"];
      break;
    }
  }
  if (output_type_name.empty()) {
    throw std::runtime_error("Given field is not a nested type");
  }

  // Create a lookup map for matching names to type descriptions
  const auto& toplevel_type = type_description_msg["type_description"];
  const auto& referenced_types = type_description_msg["referenced_type_descriptions"];
  std::unordered_map<std::string, nlohmann::json> type_map;

  type_map[toplevel_type["type_name"]] = toplevel_type;
  for (const auto& individual_type : referenced_types) {
    type_map[individual_type["type_name"]] = individual_type;
  }

  return extract_full_type_description(output_type_name, type_map);
}

auto get_implicit_type_description(const nlohmann::json& services, const nlohmann::json& actions, const nlohmann::json& type_description_info) -> nlohmann::json {
  nlohmann::json implicit_type_description = nlohmann::json::array();
  std::ranges::for_each(services, [&](const auto&) {
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "request_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "response_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "event_message")},
        {"type", "message"},
    });
  });
  std::ranges::for_each(actions, [&](const auto&) {
    const auto send_goal_service = extract_subinterface(type_description_info["type_description_msg"], "send_goal_service");
    const auto get_result_service = extract_subinterface(type_description_info["type_description_msg"], "get_result_service");
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "goal")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "result")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "feedback")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", send_goal_service},
        {"type", "service"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(send_goal_service, "request_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(send_goal_service, "response_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(send_goal_service, "event_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", get_result_service},
        {"type", "service"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(get_result_service, "request_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(get_result_service, "response_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(get_result_service, "event_message")},
        {"type", "message"},
    });
    implicit_type_description.push_back({
        {"msg", extract_subinterface(type_description_info["type_description_msg"], "feedback_message")},
        {"type", "message"},
    });
  });

  return implicit_type_description;
}

auto get_toplevel_type_description(const nlohmann::json& messages, const nlohmann::json& services, const nlohmann::json& actions, const nlohmann::json& type_description_info) -> nlohmann::json {
  if (!messages.empty()) {
    return {
        {"msg", type_description_info["type_description_msg"]},
        {"type", "message"}};
  }
  if (!services.empty()) {
    return {
        {"msg", type_description_info["type_description_msg"]},
        {"type", "service"}};
  }
  if (!actions.empty()) {
    return {
        {"msg", type_description_info["type_description_msg"]},
        {"type", "action"}};
  }
  throw std::runtime_error("get_toplevel_type_description called without any interface");
}

auto get_hash_lookup(const nlohmann::json& type_description_hashes) -> nlohmann::json {
  nlohmann::json hash_lookup = nlohmann::json::object();
  for (const auto& type_description_hash : type_description_hashes) {
    hash_lookup[type_description_hash["type_name"]] = type_description_hash["hash_string"];
  }
  return hash_lookup;
}

auto parse_rihs_string(const std::string& rihs_str) -> std::tuple<int, std::string> {
  static const std::regex rihs01_pattern(R"(RIHS([0-9a-f]{2})_([0-9a-f]{64}))");
  std::smatch match;
  if (!std::regex_match(rihs_str, match, rihs01_pattern)) {
    throw std::invalid_argument("Type hash string " + rihs_str + " does not match expected RIHS format.");
  }
  int version = std::stoi(match[1].str());
  std::string value = match[2].str();
  return std::make_tuple(version, value);
}

auto type_hash_to_c_definition(const std::string& hash_string, int indent = 2) -> std::string {
  const int bytes_per_row = 8;
  const int rows = 4;

  int version{};
  std::string value{};
  std::tie(version, value) = parse_rihs_string(hash_string);
  assert(version == 1 && "This function only knows how to generate RIHS01 definitions.");

  std::string result = fmt::format("{{{}, {{\n", version);
  for (int row = 0; row < rows; ++row) {
    result += std::string(indent + 1, ' ');
    for (int i = row * bytes_per_row; i < (row + 1) * bytes_per_row; ++i) {
      result += fmt::format(" 0x{}{},", value[i * 2], value[i * 2 + 1]);
    }
    result += '\n';
  }
  result += std::string(indent, ' ') + "}}";
  return result;
}

const std::unordered_map<std::string, int> FIELD_TYPE_NAME_TO_ID = {
    {"FIELD_TYPE_NOT_SET", 0},

    // Nested type defined in other .msg/.idl files.
    {"FIELD_TYPE_NESTED_TYPE", 1},

    // Basic Types
    // Integer Types
    {"FIELD_TYPE_INT8", 2},
    {"FIELD_TYPE_UINT8", 3},
    {"FIELD_TYPE_INT16", 4},
    {"FIELD_TYPE_UINT16", 5},
    {"FIELD_TYPE_INT32", 6},
    {"FIELD_TYPE_UINT32", 7},
    {"FIELD_TYPE_INT64", 8},
    {"FIELD_TYPE_UINT64", 9},

    // Floating-Point Types
    {"FIELD_TYPE_FLOAT", 10},
    {"FIELD_TYPE_DOUBLE", 11},
    {"FIELD_TYPE_LONG_DOUBLE", 12},

    // Char and WChar Types
    {"FIELD_TYPE_CHAR", 13},
    {"FIELD_TYPE_WCHAR", 14},

    // Boolean Type
    {"FIELD_TYPE_BOOLEAN", 15},

    // Byte/Octet Type
    {"FIELD_TYPE_BYTE", 16},

    // String Types
    {"FIELD_TYPE_STRING", 17},
    {"FIELD_TYPE_WSTRING", 18},

    // Fixed String Types
    {"FIELD_TYPE_FIXED_STRING", 19},
    {"FIELD_TYPE_FIXED_WSTRING", 20},

    // Bounded String Types
    {"FIELD_TYPE_BOUNDED_STRING", 21},
    {"FIELD_TYPE_BOUNDED_WSTRING", 22},

    // Fixed Sized Array Types
    {"FIELD_TYPE_NESTED_TYPE_ARRAY", 49},
    {"FIELD_TYPE_INT8_ARRAY", 50},
    {"FIELD_TYPE_UINT8_ARRAY", 51},
    {"FIELD_TYPE_INT16_ARRAY", 52},
    {"FIELD_TYPE_UINT16_ARRAY", 53},
    {"FIELD_TYPE_INT32_ARRAY", 54},
    {"FIELD_TYPE_UINT32_ARRAY", 55},
    {"FIELD_TYPE_INT64_ARRAY", 56},
    {"FIELD_TYPE_UINT64_ARRAY", 57},
    {"FIELD_TYPE_FLOAT_ARRAY", 58},
    {"FIELD_TYPE_DOUBLE_ARRAY", 59},
    {"FIELD_TYPE_LONG_DOUBLE_ARRAY", 60},
    {"FIELD_TYPE_CHAR_ARRAY", 61},
    {"FIELD_TYPE_WCHAR_ARRAY", 62},
    {"FIELD_TYPE_BOOLEAN_ARRAY", 63},
    {"FIELD_TYPE_BYTE_ARRAY", 64},
    {"FIELD_TYPE_STRING_ARRAY", 65},
    {"FIELD_TYPE_WSTRING_ARRAY", 66},
    {"FIELD_TYPE_FIXED_STRING_ARRAY", 67},
    {"FIELD_TYPE_FIXED_WSTRING_ARRAY", 68},
    {"FIELD_TYPE_BOUNDED_STRING_ARRAY", 69},
    {"FIELD_TYPE_BOUNDED_WSTRING_ARRAY", 70},

    // Bounded Sequence Types
    {"FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE", 97},
    {"FIELD_TYPE_INT8_BOUNDED_SEQUENCE", 98},
    {"FIELD_TYPE_UINT8_BOUNDED_SEQUENCE", 99},
    {"FIELD_TYPE_INT16_BOUNDED_SEQUENCE", 100},
    {"FIELD_TYPE_UINT16_BOUNDED_SEQUENCE", 101},
    {"FIELD_TYPE_INT32_BOUNDED_SEQUENCE", 102},
    {"FIELD_TYPE_UINT32_BOUNDED_SEQUENCE", 103},
    {"FIELD_TYPE_INT64_BOUNDED_SEQUENCE", 104},
    {"FIELD_TYPE_UINT64_BOUNDED_SEQUENCE", 105},
    {"FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE", 106},
    {"FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE", 107},
    {"FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE", 108},
    {"FIELD_TYPE_CHAR_BOUNDED_SEQUENCE", 109},
    {"FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE", 110},
    {"FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE", 111},
    {"FIELD_TYPE_BYTE_BOUNDED_SEQUENCE", 112},
    {"FIELD_TYPE_STRING_BOUNDED_SEQUENCE", 113},
    {"FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE", 114},
    {"FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE", 115},
    {"FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE", 116},
    {"FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE", 117},
    {"FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE", 118},

    // Unbounded Sequence Types
    {"FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE", 145},
    {"FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE", 146},
    {"FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE", 147},
    {"FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE", 148},
    {"FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE", 149},
    {"FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE", 150},
    {"FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE", 151},
    {"FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE", 152},
    {"FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE", 153},
    {"FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE", 154},
    {"FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE", 155},
    {"FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE", 156},
    {"FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE", 157},
    {"FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE", 158},
    {"FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE", 159},
    {"FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE", 160},
    {"FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE", 161},
    {"FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE", 162},
    {"FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE", 163},
    {"FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE", 164},
    {"FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE", 165},
    {"FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE", 166},
};

const std::unordered_map<int, std::string> FIELD_TYPE_ID_TO_NAME = [] {
  std::unordered_map<int, std::string> map;
  for (const auto& [key, value] : FIELD_TYPE_NAME_TO_ID) {
    map[value] = key;
  }
  return map;
}();

auto static_seq_n(const std::string& varname, int n) -> std::string {
  if (n > 0) {
    return fmt::format("{{{}, {}, {}}}", varname, n, n);
  }
  return std::string("{NULL, 0, 0}");
}

auto static_seq(const std::string& varname, const nlohmann::json& value) -> std::string {
  if (value.is_string()) {
    std::string n_str = value.get<std::string>();
    if (!n_str.empty()) {
      return fmt::format("{{{}, {}, {}}}", varname, n_str.size(), n_str.size());
    }
  } else if (value.is_array() && value.size() > 0) {
    return fmt::format("{{{}, {}, {}}}", varname, value.size(), value.size());
  }
  return std::string("{NULL, 0, 0}");
}

auto utf8_encode(const std::string& value_string) -> std::string {
  std::string utf8_encoded;
  for (const auto& c : rosidlcpp_core::escape_string(value_string)) {
    if (static_cast<unsigned char>(c) < 0x80) {
      utf8_encoded += c;
    } else {
      utf8_encoded += fmt::format("\\x{:02x}", static_cast<unsigned char>(c));
    }
  }
  return utf8_encoded;
}

auto field_type_id_to_name(int field_type_id) -> std::string {
  return FIELD_TYPE_ID_TO_NAME.at(field_type_id);
}

GeneratorC::GeneratorC(rosidlcpp_core::GeneratorArguments generator_arguments, bool disable_description_codegen) : GeneratorBase(), m_arguments(std::move(generator_arguments)), m_disable_description_codegen(disable_description_codegen) {
  set_input_path(m_arguments.template_dir + "/");
  set_output_path(m_arguments.output_dir + "/");

  GENERATOR_BASE_REGISTER_FUNCTION("get_includes", 2, get_includes);
  GENERATOR_BASE_REGISTER_FUNCTION("value_to_c", 2, value_to_c);
  GENERATOR_BASE_REGISTER_FUNCTION("basetype_to_c", 1, basetype_to_c);
  GENERATOR_BASE_REGISTER_FUNCTION("get_upper_bounds", 1, get_upper_bounds);
  GENERATOR_BASE_REGISTER_FUNCTION("idl_declaration_to_c", 2, idl_declaration_to_c);
  GENERATOR_BASE_REGISTER_FUNCTION("idl_structure_type_sequence_to_c_typename", 1, idl_structure_type_sequence_to_c_typename);
  GENERATOR_BASE_REGISTER_FUNCTION("idl_type_to_c", 1, idl_type_to_c);

  GENERATOR_BASE_REGISTER_FUNCTION("extract_subinterface", 2, extract_subinterface);
  GENERATOR_BASE_REGISTER_FUNCTION("get_implicit_type_descriptions", 3, get_implicit_type_description);
  GENERATOR_BASE_REGISTER_FUNCTION("get_toplevel_type_description", 4, get_toplevel_type_description);
  GENERATOR_BASE_REGISTER_FUNCTION("get_hash_lookup", 1, get_hash_lookup);
  GENERATOR_BASE_REGISTER_FUNCTION("type_hash_to_c_definition", 2, type_hash_to_c_definition);

  GENERATOR_BASE_REGISTER_FUNCTION("get_full_description_includes", 2, get_full_description_includes);

  GENERATOR_BASE_REGISTER_FUNCTION("static_seq_n", 2, static_seq_n);
  GENERATOR_BASE_REGISTER_FUNCTION("static_seq", 2, static_seq);
  GENERATOR_BASE_REGISTER_FUNCTION("utf8_encode", 1, utf8_encode);
  GENERATOR_BASE_REGISTER_FUNCTION("FIELD_TYPE_ID_TO_NAME", 1, field_type_id_to_name);
}

void GeneratorC::run() {
  // Load templates
  auto template_idl_description_c = parse_template("./idl__description.c.template");
  auto template_idl_functions_c = parse_template("./idl__functions.c.template");
  auto template_idl_functions_h = parse_template("./idl__functions.h.template");
  auto template_idl_struct_h = parse_template("./idl__struct.h.template");
  auto template_idl_type_support_c = parse_template("./idl__type_support.c.template");
  auto template_idl_type_support_h = parse_template("./idl__type_support.h.template");
  auto template_idl_h = parse_template("./idl.h.template");

  // Combined ros_json
  nlohmann::json pkg_json;
  pkg_json["package_name"] = m_arguments.package_name;
  pkg_json["messages"] = nlohmann::json::array();
  pkg_json["services"] = nlohmann::json::array();
  pkg_json["actions"] = nlohmann::json::array();

  // Generate message specific files
  for (const auto& [path, file_path] : m_arguments.idl_tuples) {
    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    // Type descriptions
    auto type_description_it = std::find_if(m_arguments.type_description_tuples.begin(), m_arguments.type_description_tuples.end(), [&file_path](const auto& v) { return v.first == file_path; });
    if (type_description_it == m_arguments.type_description_tuples.end()) {
      throw std::runtime_error("Type descriptions not found");
    }

    std::ifstream type_description_file(type_description_it->second);
    auto type_description = nlohmann::json::parse(type_description_file);
    ros_json["type_description_info"] = type_description;

    ros_json["disable_description_codegen"] = m_disable_description_codegen;

    // Raw source content
    auto it = std::ranges::find_if(m_arguments.ros_interface_files, [&](const auto& v) { return v.ends_with("/" + ros_json["type"]["name"].get<std::string>() + "." + ros_json["type"]["namespaces"].back().get<std::string>()); });
    std::string raw_source_file_path = it != m_arguments.ros_interface_files.end() ? *it : full_path;
    std::ifstream raw_source_file(raw_source_file_path);
    std::stringstream raw_source_stream;
    raw_source_stream << raw_source_file.rdbuf();

    // Get the raw source content as a list of lines
    ros_json["raw_source"] = nlohmann::json::object();
    ros_json["raw_source"]["content"] = nlohmann::json::array();
    std::string line;
    while (std::getline(raw_source_stream, line)) {
      // Handle windows-style line endings
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      ros_json["raw_source"]["content"].push_back(line);
    }
    ros_json["raw_source"]["encoding"] = std::filesystem::path(raw_source_file_path).extension().string().substr(1);  // Remove the leading dot from the extension

    ros_json["package_name"] = m_arguments.package_name;

    const auto msg_directory = ros_json["interface_path"]["filedir"].get<std::string>();
    const auto msg_type = ros_json["interface_path"]["filename"].get<std::string>();

    std::filesystem::create_directories(m_arguments.output_dir + "/" + msg_directory + "/detail");
    write_template(template_idl_description_c, ros_json, fmt::format("{}/detail/{}__description.c", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_functions_c, ros_json, fmt::format("{}/detail/{}__functions.c", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_functions_h, ros_json, fmt::format("{}/detail/{}__functions.h", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_struct_h, ros_json, fmt::format("{}/detail/{}__struct.h", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_type_support_c, ros_json, fmt::format("{}/detail/{}__type_support.c", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_type_support_h, ros_json, fmt::format("{}/detail/{}__type_support.h", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_h, ros_json, fmt::format("{}/{}.h", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

auto main(int argc, char** argv) -> int {
  /**
   * CLI Arguments
   */
  argparse::ArgumentParser argument_parser("rosidlcpp_generator_c");
  argument_parser.add_argument("--generator-arguments-file").required().help("The location of the file containing the generator arguments");
  argument_parser.add_argument("--disable-description-codegen").default_value(false);

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    std::cerr << argument_parser;
    return 1;
  }

  auto generator_arguments_file = argument_parser.get<std::string>("--generator-arguments-file");
  auto generator_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  auto disable_description_codegen = argument_parser.get<bool>("--disable-description-codegen");

  /**
   * Generation
   */
  GeneratorC generator(generator_arguments, disable_description_codegen);
  generator.run();
  return 0;
}