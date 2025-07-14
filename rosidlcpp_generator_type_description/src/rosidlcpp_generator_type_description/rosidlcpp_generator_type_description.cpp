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
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_generator_type_description/json_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <nlohmann/json_fwd.hpp>

#include <rcutils/sha256.h>
#include <rosidl_runtime_c/type_hash.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <codecvt>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

auto escape_unicode(const std::string &input) -> std::string {
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  std::wstring wide_str = converter.from_bytes(input);

  std::ostringstream oss;
  for (wchar_t wc : wide_str) {
    if (wc < 0x80) {
      // ASCII characters remain unchanged
      oss << static_cast<char>(wc);
    } else if (wc < 0x10000) {
      // BMP characters: \uXXXX
      oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(wc);
    } else {
      // Characters outside BMP (not common in most cases)
      oss << "\\U" << std::hex << std::setw(8) << std::setfill('0') << static_cast<int>(wc);
    }
  }
  return oss.str();
}

auto to_type_name(const nlohmann::json &type) -> std::string {
  assert(type.contains("namespaces") && "Type is missing namespaces");

  return fmt::format("{}/{}", fmt::join(type["namespaces"], "/"), type["name"].get<std::string>());
}

void add_message(const nlohmann::json &msg, nlohmann::ordered_json &type_description_json) {
  type_description_json[to_type_name(msg["type"])] = msg;
}

void add_service(const nlohmann::json &srv, nlohmann::ordered_json &type_description_json) {
  type_description_json[to_type_name(srv["type"])] = {
      {"type", srv["type"]},
      {"members",
       {
           {{"type", srv["request_message"]["type"]}, {"name", "request_message"}},
           {{"type", srv["response_message"]["type"]}, {"name", "response_message"}},
           {{"type", srv["event_message"]["type"]}, {"name", "event_message"}},
       }}};

  add_message(srv["request_message"], type_description_json);
  add_message(srv["response_message"], type_description_json);
  add_message(srv["event_message"], type_description_json);
}

void add_action(const nlohmann::json &action, nlohmann::ordered_json &type_description_json) {
  type_description_json[to_type_name(action["type"])] = {{"type", action["type"]},
                                                         {"members",
                                                          {
                                                              {{"type", action["goal"]["type"]}, {"name", "goal"}},
                                                              {{"type", action["result"]["type"]}, {"name", "result"}},
                                                              {{"type", action["feedback"]["type"]}, {"name", "feedback"}},
                                                              {{"type", action["send_goal_service"]["type"]}, {"name", "send_goal_service"}},
                                                              {{"type", action["get_result_service"]["type"]}, {"name", "get_result_service"}},
                                                              {{"type", action["feedback_message"]["type"]}, {"name", "feedback_message"}},
                                                          }}};

  add_message(action["goal"], type_description_json);
  add_message(action["result"], type_description_json);
  add_message(action["feedback"], type_description_json);
  add_service(action["send_goal_service"], type_description_json);
  add_service(action["get_result_service"], type_description_json);
  add_message(action["feedback_message"], type_description_json);
}

auto field_type_type_name(const nlohmann::json &ftype) -> std::string {
  static const std::unordered_map<std::string, std::string> FIELD_VALUE_TYPE_MAP = {
      {"nested_type", "FIELD_TYPE_NESTED_TYPE"},
      {"int8", "FIELD_TYPE_INT8"},
      {"uint8", "FIELD_TYPE_UINT8"},
      {"int16", "FIELD_TYPE_INT16"},
      {"uint16", "FIELD_TYPE_UINT16"},
      {"int32", "FIELD_TYPE_INT32"},
      {"uint32", "FIELD_TYPE_UINT32"},
      {"int64", "FIELD_TYPE_INT64"},
      {"uint64", "FIELD_TYPE_UINT64"},
      {"float", "FIELD_TYPE_FLOAT"},
      {"double", "FIELD_TYPE_DOUBLE"},
      {"long", "LONG_DOUBLE"},
      {"char", "FIELD_TYPE_CHAR"},
      {"wchar", "FIELD_TYPE_WCHAR"},
      {"boolean", "FIELD_TYPE_BOOLEAN"},
      {"octet", "FIELD_TYPE_BYTE"}};

  nlohmann::json value_type = ftype;
  std::string name_suffix;

  if (ftype.contains("value_type")) {
    value_type = ftype["value_type"];
    if (ftype["name"].get<std::string>() == "array") {
      name_suffix = "_ARRAY";
    } else if (ftype.contains("maximum_size")) {
      name_suffix = "_BOUNDED_SEQUENCE";
    } else {
      name_suffix = "_UNBOUNDED_SEQUENCE";
    }
  }

  std::string value_type_name;
  if (value_type["name"].get<std::string>() == "string") {
    value_type_name = value_type.contains("maximum_size") ? "FIELD_TYPE_BOUNDED_STRING" : "FIELD_TYPE_STRING";
  } else if (value_type["name"].get<std::string>() == "wstring") {
    value_type_name = value_type.contains("maximum_size") ? "FIELD_TYPE_BOUNDED_WSTRING" : "FIELD_TYPE_WSTRING";
  } else if (value_type.contains("namespaces")) {
    value_type_name = "FIELD_TYPE_NESTED_TYPE";
  } else if (rosidlcpp_core::is_primitive(value_type)) {
    value_type_name = FIELD_VALUE_TYPE_MAP.at(value_type["name"].get<std::string>());
  }

  if (value_type_name.empty()) {
    throw std::runtime_error("Unknown field type: " + value_type.dump());
  }

  return value_type_name + name_suffix;
}

auto field_type_type_id(const nlohmann::json &ftype) -> int {
  static const std::unordered_map<std::string, int> FIELD_TYPE_NAME_TO_ID = {
      {"FIELD_TYPE_NOT_SET", 0},
      {"FIELD_TYPE_NESTED_TYPE", 1},
      {"FIELD_TYPE_INT8", 2},
      {"FIELD_TYPE_UINT8", 3},
      {"FIELD_TYPE_INT16", 4},
      {"FIELD_TYPE_UINT16", 5},
      {"FIELD_TYPE_INT32", 6},
      {"FIELD_TYPE_UINT32", 7},
      {"FIELD_TYPE_INT64", 8},
      {"FIELD_TYPE_UINT64", 9},
      {"FIELD_TYPE_FLOAT", 10},
      {"FIELD_TYPE_DOUBLE", 11},
      {"FIELD_TYPE_LONG_DOUBLE", 12},
      {"FIELD_TYPE_CHAR", 13},
      {"FIELD_TYPE_WCHAR", 14},
      {"FIELD_TYPE_BOOLEAN", 15},
      {"FIELD_TYPE_BYTE", 16},
      {"FIELD_TYPE_STRING", 17},
      {"FIELD_TYPE_WSTRING", 18},
      {"FIELD_TYPE_FIXED_STRING", 19},
      {"FIELD_TYPE_FIXED_WSTRING", 20},
      {"FIELD_TYPE_BOUNDED_STRING", 21},
      {"FIELD_TYPE_BOUNDED_WSTRING", 22},
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

  return FIELD_TYPE_NAME_TO_ID.at(field_type_type_name(ftype));
}

auto field_type_capacity(const nlohmann::json &ftype) -> int {
  if (rosidlcpp_core::is_nestedtype(ftype) && ftype.contains("maximum_size")) {
    return ftype["maximum_size"].get<int>();
  }
  if (ftype.contains("size")) {
    return ftype["size"].get<int>();
  }

  return 0;
}

auto field_type_string_capacity(const nlohmann::json &ftype) -> int {
  nlohmann::json value_type = ftype.contains("value_type") ? ftype["value_type"] : ftype;

  if (rosidlcpp_core::is_string(value_type)) {
    if (value_type.contains("maximum_size")) {
      return value_type["maximum_size"].get<int>();
    }
  }

  return 0;
}

auto field_type_nested_type_name(const nlohmann::json &ftype) -> std::string {
  nlohmann::json value_type = ftype.contains("value_type") ? ftype["value_type"] : ftype;

  if (value_type.contains("namespaces")) {
    return fmt::format("{}/{}", fmt::join(value_type["namespaces"], "/"), value_type["name"].get<std::string>());
  }

  if (false /* Named type ? */) {  // TODO: Figure out what named types are
  }

  return "";
}

auto serialize_field_type(const nlohmann::json &type) -> nlohmann::ordered_json {
  return {
      {"type_id", field_type_type_id(type)},
      {"capacity", field_type_capacity(type)},
      {"string_capacity", field_type_string_capacity(type)},
      {"nested_type_name", field_type_nested_type_name(type)},
  };
}

auto format_default(const nlohmann::json &default_value) -> std::string {
  if (default_value.is_boolean()) {
    return default_value.get<bool>() ? "True" : "False";
  }

  if (default_value.is_array()) {
    std::vector<std::string> elements;
    for (const auto &element : default_value) {
      if (element.is_string()) {
        elements.push_back(fmt::format("'{}'", element.get<std::string>()));
      } else {
        elements.push_back(format_default(element));
      }
    }
    return fmt::format("({})", fmt::join(elements, ", "));
  }

  if (default_value.is_string()) {
    return default_value.get<std::string>();
  }

  return default_value.dump();
}

auto serialize_field(const nlohmann::json &member) -> nlohmann::ordered_json {
  return {
      {"name", member["name"].get<std::string>()},
      {"type", serialize_field_type(member["type"])},
      {"default_value", member.contains("default") ? format_default(member["default"]) : ""},
  };
}

auto serialize_individual_type_description(const nlohmann::json &type, const nlohmann::ordered_json &members) -> nlohmann::ordered_json {
  nlohmann::ordered_json result = {
      {"type_name", to_type_name(type)},
      {"fields", nlohmann::ordered_json::array()},
  };

  for (const auto &member : members) {
    result["fields"].push_back(serialize_field(member));
  }

  return result;
}

auto extract_full_type_description(const std::string &output_type_name, const std::map<std::string, nlohmann::ordered_json> &type_map) -> nlohmann::ordered_json {
  // Traverse reference graph to narrow down the references for the output type
  const auto &output_type = type_map.at(output_type_name);
  std::vector<std::string> output_references;

  std::vector<std::string> process_queue;
  for (const auto &field : output_type["fields"]) {
    if (!field["type"]["nested_type_name"].get<std::string>().empty()) {
      process_queue.push_back(field["type"]["nested_type_name"].get<std::string>());
    }
  }

  while (!process_queue.empty()) {
    auto process_type = process_queue.back();
    process_queue.pop_back();
    if (std::find(output_references.begin(), output_references.end(), process_type) == output_references.end()) {
      output_references.push_back(process_type);
      for (const auto &field : type_map.at(process_type)["fields"]) {
        if (!field["type"]["nested_type_name"].get<std::string>().empty()) {
          process_queue.push_back(field["type"]["nested_type_name"].get<std::string>());
        }
      }
    }
  }

  nlohmann::ordered_json referenced_type_descriptions = nlohmann::ordered_json::array();
  std::sort(output_references.begin(), output_references.end());
  for (const auto &type_name : output_references) {
    referenced_type_descriptions.push_back(type_map.at(type_name));
  }

  return nlohmann::ordered_json{
      {"type_description", output_type},
      {"referenced_type_descriptions", referenced_type_descriptions}};
}

auto calculate_type_hash(const nlohmann::ordered_json &type_description) -> std::string {
  nlohmann::ordered_json hashable_dict = type_description;

  for (auto &field : hashable_dict["type_description"]["fields"]) {
    field.erase("default_value");
  }
  for (auto &referenced_type : hashable_dict["referenced_type_descriptions"]) {
    for (auto &field : referenced_type["fields"]) {
      field.erase("default_value");
    }
  }

  const dump_parameters custom{
      .object_start = "{",
      .object_end = "}",
      .object_key_sep = ": ",
      .object_value_sep = ", ",
      .object_empty = "",
      .object_newline_indent = 0,
      .array_start = "[",
      .array_end = "]",
      .array_sep = ", ",
      .array_empty = "",
      .array_newline_indent = 0,
  };

  std::string hash_string = dump(hashable_dict, custom);
  std::vector<uint8_t> hash_bytes(hash_string.begin(), hash_string.end());

  rcutils_sha256_ctx_t sha_ctx;
  rcutils_sha256_init(&sha_ctx);
  rcutils_sha256_update(&sha_ctx, hash_bytes.data(), hash_bytes.size());
  std::array<uint8_t, ROSIDL_TYPE_HASH_SIZE> output_hash{};
  rcutils_sha256_final(&sha_ctx, output_hash.data());

  return fmt::format("RIHS01_{:02x}", fmt::join(output_hash, ""));
}

class GeneratorTypeDescription : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypeDescription(const rosidlcpp_core::GeneratorArguments &generator_arguments);

  auto run() -> int;

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};

GeneratorTypeDescription::GeneratorTypeDescription(const rosidlcpp_core::GeneratorArguments &generator_arguments) : GeneratorBase(), m_arguments(generator_arguments) {
}

auto GeneratorTypeDescription::run() -> int {
  std::unordered_map<std::string, std::string> include_map;

  for (const auto &[package_name, include_base_path] : m_arguments.include_paths) {
    include_map[package_name] = include_base_path;
  }

  nlohmann::ordered_json individual_types;
  // Generate message specific files
  for (const auto &[path, file_path] : m_arguments.idl_tuples) {
    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    ros_json["package_name"] = m_arguments.package_name;

    for (const auto &msg : ros_json.value("messages", nlohmann::json::array())) {
      add_message(msg["message"], individual_types);
    }
    for (const auto &srv : ros_json.value("services", nlohmann::json::array())) {
      add_service(srv, individual_types);
    }
    for (const auto &action : ros_json.value("actions", nlohmann::json::array())) {
      add_action(action, individual_types);
    }
  }

  std::vector<std::string> pending_includes;
  for (const auto &[name, structure] : individual_types.items()) {
    for (const auto &member : structure["members"]) {
      nlohmann::json member_type;
      if (member["type"].contains("namespaces")) {
        member_type = member["type"];
      } else if (member["type"].contains("value_type") && member["type"]["value_type"].contains("namespaces")) {
        member_type = member["type"]["value_type"];
      } else {
        continue;
      }

      if (!individual_types.contains(to_type_name(member_type))) {
        pending_includes.push_back(to_type_name(member_type));  // TODO: Fix
      }
    }
  }

  std::map<std::string, nlohmann::ordered_json> serialized_type_lookup;
  for (const auto &[key, value] : individual_types.items()) {
    serialized_type_lookup[key] = serialize_individual_type_description(value["type"], value["members"]);
  }

  // Hash lookup
  std::unordered_map<std::string, std::string> hash_lookup;
  for (const auto &process_include : pending_includes) {
    std::filesystem::path p_path = process_include + ".json";
    std::string pkg = p_path.begin()->string();
    std::string pkg_dir = include_map[pkg];
    std::filesystem::path include_path = std::filesystem::path(pkg_dir) / std::filesystem::relative(p_path, pkg);

    std::ifstream include_file(include_path);
    if (!include_file.is_open()) {
      throw std::runtime_error("Could not open include file: " + include_path.string());
    }

    nlohmann::ordered_json include_json;
    include_file >> include_json;

    auto type_description_msg = include_json["type_description_msg"];
    try {
      for (const auto &val : include_json["type_hashes"]) {
        hash_lookup[val["type_name"].get<std::string>()] = val["hash_string"].get<std::string>();
      }
    } catch (const std::exception &e) {
      throw std::runtime_error("Key 'type_hashes' not found in " + include_path.string());
    }

    serialized_type_lookup[type_description_msg["type_description"]["type_name"].get<std::string>()] = type_description_msg["type_description"];
    for (const auto &referenced_type : type_description_msg["referenced_type_descriptions"]) {
      serialized_type_lookup[referenced_type["type_name"].get<std::string>()] = referenced_type;
    }
  }

  // Create fully-unrolled type description
  std::vector<nlohmann::ordered_json> type_description;
  for (const auto &[type_name, individual_type] : individual_types.items()) {
    nlohmann::ordered_json full_type_description = extract_full_type_description(type_name, serialized_type_lookup);
    type_description.push_back(full_type_description);
    hash_lookup[type_name] = calculate_type_hash(full_type_description);
  }

  std::filesystem::create_directories(m_arguments.output_dir + "/msg");
  std::filesystem::create_directories(m_arguments.output_dir + "/srv");
  std::filesystem::create_directories(m_arguments.output_dir + "/action");

  for (const auto &full_type_description : type_description) {
    std::string top_type_name = full_type_description["type_description"]["type_name"];
    std::vector<nlohmann::ordered_json> hashes;
    hashes.push_back({{"type_name", top_type_name}, {"hash_string", hash_lookup[top_type_name]}});

    for (const auto &referenced_type : full_type_description["referenced_type_descriptions"]) {
      std::string type_name = referenced_type["type_name"];
      hashes.push_back({{"type_name", type_name}, {"hash_string", hash_lookup[type_name]}});
    }

    nlohmann::ordered_json json_content = {
        {"type_description_msg", full_type_description},
        {"type_hashes", hashes},
    };

    auto tmp = top_type_name.find_first_of('/');
    auto type_name2 = top_type_name.substr(tmp + 1);

    std::ofstream json_file{m_arguments.output_dir + "/" + type_name2 + ".json"};

    dump_parameters json_params{
        .object_start = "{",
        .object_end = "}",
        .object_key_sep = ": ",
        .object_value_sep = ",",
        .object_empty = "",
        .object_newline_indent = 2,
        .array_start = "[",
        .array_end = "]",
        .array_sep = ",",
        .array_empty = "",
        .array_newline_indent = 2,
    };
    dump(json_content, json_file, json_params);
  }

  return 0;
}

auto main(int argc, char *argv[]) -> int {
  /**
   * CLI Arguments
   */
  argparse::ArgumentParser argument_parser("rosidlcpp_generator_type_description");
  argument_parser.add_argument("--generator-arguments-file").required().help("The location of the file containing the generator arguments");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::cerr << argument_parser;
    return 1;
  }

  auto generator_arguments_file = argument_parser.get<std::string>("--generator-arguments-file");
  auto generator_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  /**
   * Generation
   */
  GeneratorTypeDescription generator(generator_arguments);
  return generator.run();
}