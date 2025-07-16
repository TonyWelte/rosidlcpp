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

#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <nlohmann/json_fwd.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr std::string_view STRING_MODULE = "module";
constexpr std::string_view STRING_STRUCT = "struct";
constexpr std::string_view STRING_TYPEDEF = "typedef";
constexpr std::string_view STRING_CONST = "const";
constexpr std::string_view STRING_INCLUDE = "#include";

constexpr std::string_view VALID_NUMERIC = "1234567890-";
constexpr std::string_view VALID_TYPE_CHAR = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_:";
constexpr std::string_view VALID_NAME_CHAR = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_";
constexpr std::string_view WHITE_CHAR = " \t\n";

using json = nlohmann::json;

using TypedefMap = std::unordered_map<std::string, std::string>;

namespace rosidlcpp_parser {

auto split_string_view(std::string_view value, std::string_view sep) -> std::vector<std::string> {
  std::vector<std::string> result;

  auto cursor = value.find(sep);
  while (cursor != std::string::npos) {
    result.emplace_back(value.substr(0, cursor));
    value.remove_prefix(cursor + sep.size());
    cursor = value.find(sep);
  }
  result.emplace_back(value);

  return result;
}

auto split_string(const std::string& value, const std::string& sep) -> std::vector<std::string> {
  return split_string_view(value, sep);
}

auto consume_white_space(std::string_view& content_view) -> void {
  auto new_start = content_view.find_first_not_of(" \t\n");
  if (new_start != std::string_view::npos) {
    content_view.remove_prefix(new_start);
  } else {
    content_view.remove_prefix(content_view.size());
  }
}

auto consume_comment(std::string_view& content_view) -> void {
  if (content_view.substr(0, 2) == "//") {
    auto end_of_line = content_view.find_first_of('\n');
    if (end_of_line == std::string_view::npos) {
      content_view.remove_prefix(content_view.size());
    } else {
      content_view.remove_prefix(end_of_line + 1);
    }
  } else if (content_view.substr(0, 2) == "/*") {
    auto end_of_comment = content_view.find("*/");
    if (end_of_comment == std::string_view::npos) {
      throw std::runtime_error("Unterminated comment in content view");
    }
    content_view.remove_prefix(end_of_comment + 2);
  }
}

auto consume_white_space_and_comment(std::string_view& content_view) -> void {
  size_t old_size{};
  do {
    old_size = content_view.size();
    consume_white_space(content_view);
    consume_comment(content_view);
  } while (old_size != content_view.size());
}

auto remove_white_space(std::string_view content_view) -> std::string_view {
  auto start = content_view.find_first_not_of(WHITE_CHAR);
  auto end = content_view.find_last_not_of(WHITE_CHAR);

  if (start == std::string_view::npos) {
    return "";
  }

  return content_view.substr(start, end - start + 1);
}

auto parse_name(std::string_view& content_view) -> std::string_view {
  auto end_of_name = content_view.find_first_not_of(VALID_NAME_CHAR);
  auto name = content_view.substr(0, end_of_name);

  content_view.remove_prefix(end_of_name);

  return name;
}

auto interpret_type(std::string_view type_string, TypedefMap typedefs) -> json {
  json result;
  if (type_string.starts_with("string<")) {  // string<bounds>
    result["name"] = "string";
    type_string.remove_prefix(std::string_view{"string<"}.size());
    type_string.remove_suffix(1);  // Remove last '>'
    result["maximum_size"] = std::stoi(std::string{remove_white_space(type_string)});
  } else if (type_string.starts_with("wstring<")) {  // wstring<bounds>
    result["name"] = "wstring";
    type_string.remove_prefix(std::string_view{"wstring<"}.size());
    type_string.remove_suffix(1);  // Remove last '>'
    result["maximum_size"] = std::stoi(std::string{remove_white_space(type_string)});
  } else if (type_string.starts_with("sequence<")) {  // sequence<type> or sequence<type, bounds>
    result["name"] = "sequence";

    type_string.remove_prefix(std::string_view("sequence<").size());
    type_string.remove_suffix(1);  // Remove last '>'

    auto comma_pos = type_string.find_first_of(',');  // Deal with bounded sequences
    if (comma_pos != std::string_view::npos) {
      result["maximum_size"] = std::stoi(std::string{remove_white_space(type_string.substr(comma_pos + 1))});
    } else {
    }
    type_string = remove_white_space(type_string.substr(0, comma_pos));
    result["value_type"] = interpret_type(type_string, typedefs);
  } else if (type_string.ends_with("]")) {  // array
    // Parse array type
    auto open_bracket_pos = type_string.find_first_of('[');
    result["size"] = std::stoi(std::string{type_string.substr(open_bracket_pos + 1, type_string.size() - 2)});
    result["name"] = "array";
    type_string = type_string.substr(0, open_bracket_pos);
    result["value_type"] = interpret_type(type_string, typedefs);
  } else if (typedefs.contains(std::string{type_string})) {
    result = interpret_type(typedefs[std::string{type_string}], typedefs);
  } else {  // Check for namespaced type
    auto namespaced_type = split_string_view(type_string, "::");
    if (namespaced_type.size() > 1) {
      result["name"] = namespaced_type.back();
      namespaced_type.pop_back();
      result["namespaces"] = namespaced_type;
    } else {
      result["name"] = type_string;
    }
  }

  return result;
}

auto parse_type(std::string_view& content_view) -> std::string {
  auto end_of_type = content_view.find_first_not_of(VALID_TYPE_CHAR);
  if (end_of_type == std::string_view::npos) {
    throw std::runtime_error("Malformed type");
  }

  std::string type_name{content_view.substr(0, end_of_type)};
  content_view.remove_prefix(end_of_type);
  end_of_type = 0;

  consume_white_space_and_comment(content_view);

  if (content_view[0] == '<') {
    auto tmp_end_of_type = content_view.find_first_of('>') + 1;

    if (tmp_end_of_type == std::string_view::npos) {
      throw std::runtime_error("Malformed type: missing '>'");
    }

    // Check if there is a '<' in the type. If there is, we need to find the next '>'
    if (content_view.substr(end_of_type + 1, tmp_end_of_type - end_of_type).find_first_of('<') != std::string_view::npos) {
      tmp_end_of_type = content_view.find_first_of('>', tmp_end_of_type) + 1;
    }

    end_of_type = tmp_end_of_type;
  }

  // Handle multi word types
  // unsigned short
  // unsigned long
  // long long
  // unsigned long long
  // long double
  if (content_view.starts_with("unsigned long long")) {
    end_of_type = std::string_view("unsigned long long").size();
  } else if (content_view.starts_with("long long")) {
    end_of_type = std::string_view("long long").size();
  } else if (content_view.starts_with("unsigned long")) {
    end_of_type = std::string_view("unsigned long").size();
  } else if (content_view.starts_with("unsigned short")) {
    end_of_type = std::string_view("unsigned short").size();
  } else if (content_view.starts_with("long double")) {
    end_of_type = std::string_view("long double").size();
  }

  type_name += content_view.substr(0, end_of_type);
  content_view.remove_prefix(end_of_type);

  return type_name;
}

auto parse_value_list(std::string_view& content_view) -> json {
  assert(content_view[0] == '[' && "Value list should start with '['");

  content_view.remove_prefix(1);  // Remove [
  consume_white_space_and_comment(content_view);

  json result = json::array();
  while (content_view[0] != ']') {
    result.push_back(parse_value(content_view));
    consume_white_space(content_view);

    if (content_view[0] == ',') {
      content_view.remove_prefix(1);
      consume_white_space(content_view);
    }
  }

  content_view.remove_prefix(1);  // Remove ]

  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_string_part(std::string_view& content_view) -> std::string {
  assert(content_view[0] == '"' && "Value string should start with '\"'");

  content_view.remove_prefix(1);
  auto string_limit = content_view.find_first_of('"');
  if (string_limit == 0) {
    content_view.remove_prefix(1);
    return "";
  }

  auto result = std::string{content_view.substr(0, string_limit - 1 /* last character added at the end */)};
  while (content_view[string_limit - 1] == '\\') {  // '"' is escaped
    auto old_limit = string_limit;
    string_limit += content_view.substr(string_limit + 1).find_first_of('"') + 1;

    result += content_view.substr(old_limit, string_limit - old_limit - 1);
  }
  result += content_view.substr(string_limit - 1, 1);  // Add last character

  content_view.remove_prefix(string_limit + 1);

  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_string(std::string_view& content_view) -> std::string /* not string_view */ {
  std::string result;
  size_t old_size = content_view.size();
  while (content_view[0] == '"') {
    result += parse_string_part(content_view);
    consume_white_space_and_comment(content_view);

    if (old_size == content_view.size()) {
      throw std::runtime_error("Failed to parse string");
    }
    old_size = content_view.size();
  }

  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_string_part_python(std::string_view& content_view) -> std::string {
  assert(content_view[0] == '\'' && "Value string should start with \"'\"");

  content_view.remove_prefix(1);
  auto string_limit = content_view.find_first_of('\'');
  if (string_limit == 0) {
    content_view.remove_prefix(1);
    return "";
  }

  while (content_view[string_limit - 1] == '\\') {  // '"' is escaped
    string_limit += content_view.substr(string_limit + 1).find_first_of('\'') + 1;
  }

  auto result = std::string{fmt::format("{}", content_view.substr(0, string_limit))};
  content_view.remove_prefix(string_limit + 1);

  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_string_python(std::string_view& content_view) -> std::string /* not string_view */ {
  if (content_view[0] == '"') {
    return parse_string(content_view);
  }

  std::string result;
  size_t old_size = content_view.size();
  while (content_view[0] == '\'') {
    result += parse_string_part_python(content_view);
    consume_white_space_and_comment(content_view);

    if (old_size == content_view.size()) {
      throw std::runtime_error("Failed to parse string");
    }
    old_size = content_view.size();
  }

  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_numeric(std::string_view& content_view) -> json {
  bool is_negative = content_view[0] == '-';

  // if (is_negative)
  //     content_view.remove_prefix(1);

  auto end_of_numeric = content_view.find_first_not_of(VALID_NUMERIC);

  if (content_view[end_of_numeric] == 'e' || content_view[end_of_numeric] == '.') {  // is float
    double result{};
    auto [ptr, ec] = std::from_chars(content_view.data(), content_view.data() + content_view.size(), result);
    if (ec != std::errc()) {
      throw std::runtime_error("Failed to parse floating point value");
    }
    content_view.remove_prefix(std::distance(content_view.data(), ptr));
    consume_white_space_and_comment(content_view);
    return result;
  } else if (is_negative) {  // is signed integer
    long long result{};
    auto [ptr, ec] = std::from_chars(content_view.data(), content_view.data() + content_view.size(), result);
    if (ec != std::errc()) {
      throw std::runtime_error("Failed to parse integer value");
    }
    content_view.remove_prefix(std::distance(content_view.data(), ptr));
    consume_white_space_and_comment(content_view);
    return result;
  }
  {  // assume is unsigned integer (even if it's signed, it will fit in unsigned)
    unsigned long long result{};
    auto [ptr, ec] = std::from_chars(content_view.data(), content_view.data() + content_view.size(), result);
    if (ec != std::errc()) {
      throw std::runtime_error("Failed to parse integer value");
    }
    content_view.remove_prefix(std::distance(content_view.data(), ptr));
    consume_white_space_and_comment(content_view);
    return result;
  }
}

auto parse_value(std::string_view& content_view) -> json {
  if (content_view[0] == '[') {
    return parse_value_list(content_view);
  } else if (content_view[0] == '"') {
    return parse_string(content_view);
  } else if (content_view[0] == '\'') {
    return parse_string_python(content_view);
  } else if (content_view.substr(0, 4) == "TRUE" || content_view.substr(0, 4) == "True") {  // Real value is TRUE but array string value is True
    content_view.remove_prefix(4);
    consume_white_space_and_comment(content_view);
    return true;
  } else if (content_view.substr(0, 5) == "FALSE" || content_view.substr(0, 5) == "False") {
    content_view.remove_prefix(5);
    consume_white_space_and_comment(content_view);
    return false;
  } else if (std::isdigit(content_view[0]) || content_view[0] == '-') {
    return parse_numeric(content_view);
  } else {
    throw std::runtime_error("Malformed value");
  }
}

auto parse_constant(std::string_view& content_view, TypedefMap typedefs) -> json {
  assert(content_view.substr(0, STRING_CONST.size()) == STRING_CONST && "Bad constant parsing call");

  content_view.remove_prefix(STRING_CONST.size());
  consume_white_space_and_comment(content_view);

  json result;

  result["type"] = interpret_type(parse_type(content_view), typedefs);
  consume_white_space_and_comment(content_view);

  result["name"] = parse_name(content_view);
  consume_white_space_and_comment(content_view);

  auto equal_sign_pos = content_view.find_first_of('=');
  content_view.remove_prefix(equal_sign_pos + 1);
  consume_white_space_and_comment(content_view);

  result["value"] = parse_value(content_view);
  consume_white_space_and_comment(content_view);

  if (content_view[0] != ';') {
    throw std::runtime_error("Failed to parse contant");
  }

  content_view.remove_prefix(1);
  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_member(std::string_view& content_view, TypedefMap typedefs) -> json {
  json result;

  result["comments"] = json::array();

  result["type"] = interpret_type(parse_type(content_view), typedefs);

  consume_white_space_and_comment(content_view);

  result["name"] = parse_name(content_view);

  consume_white_space_and_comment(content_view);

  if (content_view[0] == '[') {  // Array definition
    auto end_of_array_definition = content_view.find_first_of(']') + 1;
    result["type"]["value_type"] = result["type"];
    result["type"]["name"] = "array";
    result["type"]["size"] = std::stoi(std::string{remove_white_space(content_view.substr(1, end_of_array_definition - 2))});
    content_view.remove_prefix(end_of_array_definition);
  }

  consume_white_space_and_comment(content_view);

  assert(content_view[0] == ';' && "Malformed member definition!?");

  content_view.remove_prefix(1);

  return result;
}

auto parse_attribute(std::string_view& content_view) -> json {
  assert(content_view[0] == '@' && "Not an attribute!?");

  json result;

  content_view.remove_prefix(1);
  result["name"] = parse_name(content_view);

  consume_white_space_and_comment(content_view);

  if (content_view[0] != '(') {
    return result;
  }

  content_view.remove_prefix(1);
  consume_white_space_and_comment(content_view);

  size_t old_size = content_view.size();
  while (content_view[0] != ')') {
    auto name = parse_name(content_view);
    consume_white_space_and_comment(content_view);
    content_view.remove_prefix(1);  // Skip "=" sign
    consume_white_space_and_comment(content_view);
    auto value = parse_value(content_view);
    result["content"][name] = value;

    if (content_view[0] == ',') {
      content_view.remove_prefix(1);
      consume_white_space_and_comment(content_view);
    }

    if (old_size == content_view.size()) {
      throw std::runtime_error("Failed to parse attribute");
    }
    old_size = content_view.size();
  }

  content_view.remove_prefix(1);  // Remove ")"

  consume_white_space_and_comment(content_view);

  return result;
}

auto parse_typedef(std::string_view& content_view) -> std::pair<std::string, std::string> {
  assert(content_view.substr(0, STRING_TYPEDEF.size()) == STRING_TYPEDEF && "Not a typedef!?");

  content_view.remove_prefix(STRING_TYPEDEF.size() + 1);

  consume_white_space_and_comment(content_view);
  std::string type{parse_type(content_view)};

  consume_white_space_and_comment(content_view);
  std::string name{parse_name(content_view)};

  if (content_view[0] == '[') {
    auto end_of_array_definition = content_view.find_first_of(']') + 1;
    type += content_view.substr(0, end_of_array_definition);
    content_view.remove_prefix(end_of_array_definition);
  }

  consume_white_space_and_comment(content_view);
  if (content_view.empty() || content_view.front() != ';') {
    throw std::runtime_error("Malformed typedef");
  }

  content_view.remove_prefix(1);
  consume_white_space_and_comment(content_view);

  return {name, type};
}

auto parse_structure(std::string_view& content_view, TypedefMap typedefs) -> json {
  assert(content_view.substr(0, STRING_STRUCT.size()) == STRING_STRUCT && "Not a struct!?");

  content_view.remove_prefix(STRING_STRUCT.size() + 1);

  consume_white_space_and_comment(content_view);
  auto name = parse_name(content_view);

  auto module_bloc_start = content_view.find_first_of('{');

  // Move to the first module element
  content_view.remove_prefix(module_bloc_start + 1);
  consume_white_space_and_comment(content_view);

  json module_json = json::object();
  module_json["name"] = name;
  module_json["comments"] = json::array();

  nlohmann::json annotations = json::object();

  size_t old_size = content_view.size();
  while (content_view.front() != '}') {
    if (content_view[0] == '@') {
      auto annotation = parse_attribute(content_view);
      annotations[annotation["name"]].push_back(annotation["content"]);
    } else if (content_view.substr(0, STRING_CONST.size()) == STRING_CONST) {
      module_json["constants"].push_back(parse_constant(content_view, typedefs));
      annotations.clear();
    } else {
      module_json["members"].push_back(parse_member(content_view, typedefs));

      // Process annotations
      if (annotations.contains("default")) {
        if (module_json["members"].back()["type"].contains("value_type")) {
          module_json["members"].back()["default"] = parse_default_list(annotations["default"][0]["value"].get<std::string>());
        } else {
          module_json["members"].back()["default"] = annotations["default"][0]["value"];
        }
      }
      for (const auto& verbatim : annotations.value("verbatim", json::array())) {
        if (verbatim["language"] == "comment") {
          for (const auto& line : rosidlcpp_parser::split_string_view(verbatim["text"].get<std::string>(), "\\n")) {
            module_json["members"].back()["comments"].push_back(line);
          }
        }
      }
      if (annotations.contains("key")) {
        module_json["members"].back()["key"] = true;
      }
      // TODO: Do something with other annotations
      annotations.clear();
    }
    consume_white_space_and_comment(content_view);

    if (old_size == content_view.size()) {
      throw std::runtime_error("Malformed struct");
    }
    old_size = content_view.size();
  }

  content_view.remove_prefix(1);  // Remove '}'
  consume_white_space_and_comment(content_view);
  content_view.remove_prefix(1);  // Remove ';'
  consume_white_space_and_comment(content_view);

  return module_json;
}

auto parse_module(std::string_view& content_view, TypedefMap typedefs) -> json {
  assert(content_view.substr(0, STRING_MODULE.size()) == STRING_MODULE && "Not a module!?");

  content_view.remove_prefix(STRING_MODULE.size() + 1);

  consume_white_space_and_comment(content_view);
  auto name = parse_name(content_view);

  auto module_bloc_start = content_view.find_first_of('{');

  // Move to the first module element
  content_view.remove_prefix(module_bloc_start + 1);
  consume_white_space_and_comment(content_view);

  json module_json = json::object();
  module_json["name"] = name;

  nlohmann::json annotations = json::object();

  size_t old_size = content_view.size();
  while (content_view.front() != '}') {
    // Parse module content
    if (content_view.substr(0, STRING_MODULE.size()) == STRING_MODULE) {
      module_json["modules"].push_back(parse_module(content_view, typedefs));
    } else if (content_view.substr(0, STRING_STRUCT.size()) == STRING_STRUCT) {
      module_json["structures"].push_back(parse_structure(content_view, typedefs));
      for (const auto& verbatim : annotations.value("verbatim", json::array())) {
        if (verbatim["language"] == "comment") {
          for (const auto& line : rosidlcpp_parser::split_string_view(verbatim["text"].get<std::string>(), "\\n")) {
            module_json["structures"].back()["comments"].push_back(line);
          }
        }
      }
      // TODO: Do something with other annotations
      annotations.clear();
    } else if (content_view[0] == '@') {
      auto annotation = parse_attribute(content_view);
      annotations[annotation["name"]].push_back(annotation["content"]);
    } else if (content_view.substr(0, STRING_CONST.size()) == STRING_CONST) {
      module_json["constants"].push_back(parse_constant(content_view, typedefs));
      for (const auto& verbatim : annotations.value("verbatim", json::array())) {
        if (verbatim["language"] == "comment") {
          for (const auto& line : rosidlcpp_parser::split_string_view(verbatim["text"].get<std::string>(), "\\n")) {
            module_json["constants"].back()["comments"].push_back(line);
          }
        }
      }
      // TODO: Do something with other annotations
      annotations.clear();
    } else if (content_view.substr(0, STRING_TYPEDEF.size()) == STRING_TYPEDEF) {
      const auto [name, type] = parse_typedef(content_view);
      typedefs.insert_or_assign(name, type);  // insert_or_assign doesn't support std::pair
    }

    consume_white_space_and_comment(content_view);

    if (old_size == content_view.size()) {
      throw std::runtime_error("Failed to parse module");
    }
    old_size = content_view.size();
  }

  content_view.remove_prefix(1);  // Remove '}'
  consume_white_space_and_comment(content_view);
  content_view.remove_prefix(1);  // Remove ';'
  consume_white_space_and_comment(content_view);

  return module_json;
}

auto parse_include(std::string_view& content_view) -> std::string {
  assert(content_view.substr(0, STRING_INCLUDE.size()) == STRING_INCLUDE && "Not an include!?");

  content_view.remove_prefix(STRING_INCLUDE.size());
  consume_white_space(content_view);

  return parse_string_part(content_view);
}

auto parse_default_list(std::string_view default_value) -> json {
  assert(default_value.front() == '(' && "Default value should start with '('");
  assert(default_value.back() == ')' && "Default value should end with ')'");

  default_value.remove_prefix(1);  // Remove '('

  json result = json::array();

  while (!default_value.empty()) {
    result.push_back(parse_value(default_value));

    if (default_value.front() != ',' && default_value.front() != ')') {
      throw std::runtime_error(fmt::format("{}: {}", "Failed to parse default list", default_value));
    }

    default_value.remove_prefix(1);
    consume_white_space(default_value);
  }

  return result;
}

auto parse_idl_file(const std::string& filename) -> json {
  json result;

  TypedefMap typedefs;
  typedefs["short"] = "int16";
  typedefs["unsigned short"] = "uint16";
  typedefs["long"] = "int32";
  typedefs["unsigned long"] = "uint32";
  typedefs["long long"] = "int64";
  typedefs["unsigned long long"] = "uint64";
  typedefs["long double"] = "long double";  // TODO: Check

  std::ifstream file(filename);
  std::stringstream ss;
  ss << file.rdbuf();
  std::string content = ss.str();

  std::string_view content_view(content);

  consume_white_space_and_comment(content_view);

  try {
    size_t old_size = content_view.size();
    while (!content_view.empty()) {
      if (content_view.substr(0, STRING_INCLUDE.size()) == STRING_INCLUDE) {
        result["includes"].push_back(parse_include(content_view));
      } else if (content_view.substr(0, STRING_MODULE.size()) == STRING_MODULE) {
        result["modules"].push_back(parse_module(content_view, typedefs));
      }

      if (old_size == content_view.size()) {
        throw std::runtime_error("Failed to parse file");
      }
      old_size = content_view.size();
    }

  } catch (const std::runtime_error& error) {
    std::cerr << error.what() << '\n'
              << "Remaining unparsed content: \n"
              << content_view << '\n';
  }

  return result;
}

template <typename Container>
auto join(const Container& container, std::string_view sep) -> std::string {
  std::string result = container[0];
  for (auto it = std::next(container.begin()); it != container.end(); ++it) {
    result += sep;
    result += *it;
  }
  return result;
}

auto make_service_event(const nlohmann::json& service_type) -> nlohmann::json {
  nlohmann::json event = nlohmann::json::object();
  event["constants"] = json::array();
  event["comments"] = json::array();
  event["members"] = json::array();
  event["members"].push_back({
      {"name", "info"},
      {"type", {{"namespaces", {"service_msgs", "msg"}}, {"name", "ServiceEventInfo"}}},
      {"comments", json::array()},
  });
  event["members"].push_back({
      {"name", "request"},
      {"type", {{"name", "sequence"}, {"value_type", {{"name", service_type["name"].get<std::string>() + "_Request"}, {"namespaces", service_type["namespaces"]}}}, {"maximum_size", 1}}},
      {"comments", json::array()},
  });
  event["members"].push_back({
      {"name", "response"},
      {"type", {{"name", "sequence"}, {"value_type", {{"name", service_type["name"].get<std::string>() + "_Response"}, {"namespaces", service_type["namespaces"]}}}, {"maximum_size", 1}}},
      {"comments", json::array()},
  });
  event["type"]["name"] = service_type["name"].get<std::string>() + "_Event";
  event["type"]["namespaces"] = service_type["namespaces"];

  return event;
}

auto make_action_send_goal(const nlohmann::json& action_type) -> nlohmann::json {
  nlohmann::json send_goal = nlohmann::json::object();
  send_goal["request_message"] = {
      {"members", json::array()},
      {"constants", json::array()},
      {"comments", json::array()},
      {"type", {{"namespaces", action_type["namespaces"]}, {"name", action_type["name"].get<std::string>() + "_SendGoal_Request"}}}};
  send_goal["request_message"]["members"].push_back(
      {{"name", "goal_id"},
       {"type", {{"namespaces", {"unique_identifier_msgs", "msg"}}, {"name", "UUID"}}},
       {"comments", nlohmann::json::array()}});
  send_goal["request_message"]["members"].push_back(
      {{"name", "goal"},
       {"type", {{"namespaces", action_type["namespaces"]}, {"name", action_type["name"].get<std::string>() + "_Goal"}}},
       {"comments", nlohmann::json::array()}});
  send_goal["response_message"] = {
      {"members", json::array()},
      {"constants", json::array()},
      {"comments", json::array()},
      {"type", {{"namespaces", action_type["namespaces"]}, {"name", action_type["name"].get<std::string>() + "_SendGoal_Response"}}}};
  send_goal["response_message"]["members"].push_back(
      {{"name", "accepted"},
       {"type", {{"name", "boolean"}}},
       {"comments", nlohmann::json::array()}});
  send_goal["response_message"]["members"].push_back(
      {{"name", "stamp"},
       {"type", {{"namespaces", {"builtin_interfaces", "msg"}}, {"name", "Time"}}},
       {"comments", nlohmann::json::array()}});
  send_goal["type"] = {
      {"namespaces", action_type["namespaces"]},
      {"name", action_type["name"].get<std::string>() + "_SendGoal"}};

  send_goal["event_message"] = make_service_event(send_goal["type"]);

  return send_goal;
}

auto make_action_get_result_service(const nlohmann::json& action_type) -> nlohmann::json {
  nlohmann::json get_result = nlohmann::json::object();
  get_result["request_message"] = {
      {"members", json::array()},
      {"constants", json::array()},
      {"type", {{"namespaces", action_type["namespaces"]}, {"name", action_type["name"].get<std::string>() + "_GetResult_Request"}}},
      {"comments", nlohmann::json::array()}};
  get_result["request_message"]["members"].push_back(
      {{"name", "goal_id"},
       {"type", {{"namespaces", {"unique_identifier_msgs", "msg"}}, {"name", "UUID"}}},
       {"comments", nlohmann::json::array()}});
  get_result["response_message"] = {
      {"members", json::array()},
      {"constants", json::array()},
      {"type", {{"namespaces", action_type["namespaces"]}, {"name", action_type["name"].get<std::string>() + "_GetResult_Response"}}},
      {"comments", nlohmann::json::array()}};
  get_result["response_message"]["members"].push_back(
      {{"name", "status"},
       {"type", {{"name", "int8"}}},
       {"comments", nlohmann::json::array()}});
  get_result["response_message"]["members"].push_back(
      {{"name", "result"},
       {"type", {{"namespaces", action_type["namespaces"]}, {"name", action_type["name"].get<std::string>() + "_Result"}}},
       {"comments", nlohmann::json::array()}});
  get_result["type"] = {
      {"namespaces", action_type["namespaces"]},
      {"name", action_type["name"].get<std::string>() + "_GetResult"}};

  get_result["event_message"] = make_service_event(get_result["type"]);

  return get_result;
}

auto make_action_feedback_message(const nlohmann::json& action_type) -> nlohmann::json {
  nlohmann::json feedback = nlohmann::json::object();
  feedback["members"] = json::array();
  feedback["constants"] = json::array();
  feedback["comments"] = json::array();
  feedback["members"].push_back(
      {{"name", "goal_id"},
       {"type", {{"namespaces", {"unique_identifier_msgs", "msg"}}, {"name", "UUID"}}},
       {"comments", nlohmann::json::array()}});
  feedback["members"].push_back(
      {{"name", "feedback"},
       {"type", {{"name", action_type["name"].get<std::string>() + "_Feedback"}, {"namespaces", action_type["namespaces"]}}},
       {"comments", nlohmann::json::array()}});
  feedback["type"] = {
      {"namespaces", action_type["namespaces"]},
      {"name", action_type["name"].get<std::string>() + "_FeedbackMessage"}};

  return feedback;
}

auto has_non_ascii(const std::string& str) -> bool {
  return std::ranges::any_of(str, [](unsigned char c) { return c > 127; });
}

template <typename Function>
auto recursive_check(const nlohmann::json& data, Function check_function) -> bool {
  for (const auto& [key, value] : data.items()) {
    if (value.is_object()) {
      if (recursive_check(value, check_function)) {
        return true;
      }
    } else if (value.is_array()) {
      for (const auto& element : value) {
        if (recursive_check(element, check_function)) {
          return true;
        }
      }
    } else {
      if (value.is_string() && check_function(value.template get<std::string>())) {
        return true;
      }
    }
  }

  return false;
}

auto check_non_ascii(const nlohmann::json& data) -> bool {
  return recursive_check(data, has_non_ascii);
}

std::optional<json> get_constants(const json& current_node, const std::string& message_name) {
  if (current_node.contains("modules")) {
    for (const auto& module : current_node["modules"]) {
      if (module.contains("name") && module["name"] == message_name + "_Constants") {
        if (module.contains("constants")) {
          return module["constants"];
        }
      }
    }
  }
  return std::nullopt;
}

auto convert_idljson_to_rosjson(const nlohmann::json& idl_json, std::string_view file_path) -> nlohmann::json {
  nlohmann::json result;

  // Set file path
  result["interface_path"]["filepath"] = file_path;
  result["interface_path"]["filename"] = std::filesystem::path(file_path).stem().string();
  result["interface_path"]["filedir"] = std::filesystem::path(file_path).parent_path().string();

  result["type"]["name"] = result["interface_path"]["filename"];
  result["type"]["namespaces"] = {
      idl_json["modules"][0]["name"],
      idl_json["modules"][0]["modules"][0]["name"]};

  result["messages"] = json::array();
  result["services"] = json::array();
  result["actions"] = json::array();

  // Process structures
  auto& current_node = idl_json["modules"][0]["modules"][0];
  if (current_node["structures"].size() == 1) {  // Message
    result["messages"][0]["interface_path"] = result["interface_path"];
    result["messages"][0]["message"] = current_node["structures"][0];
    result["messages"][0]["message"]["constants"] = get_constants(current_node, result["messages"][0]["message"]["name"]).value_or(json::array());
    result["messages"][0]["message"]["type"] = {
        {"name", current_node["structures"][0]["name"]},
        {"namespaces", result["type"]["namespaces"]},
    };
  } else if (current_node["structures"].size() == 2) {  // Service
    result["services"][0]["interface_path"] = result["interface_path"];
    // Handle request and response messages
    int request_structure_index = current_node["structures"][0]["name"].get<std::string>().ends_with("_Request") ? 0 : 1;
    result["services"][0]["request_message"] = current_node["structures"][request_structure_index];
    result["services"][0]["response_message"] = current_node["structures"][1 - request_structure_index];
    result["services"][0]["request_message"]["constants"] = get_constants(current_node, result["services"][0]["request_message"]["name"]).value_or(json::array());
    result["services"][0]["response_message"]["constants"] = get_constants(current_node, result["services"][0]["response_message"]["name"]).value_or(json::array());
    result["services"][0]["request_message"]["type"] = {
        {"name", current_node["structures"][request_structure_index]["name"]},
        {"namespaces", result["type"]["namespaces"]},
    };
    result["services"][0]["response_message"]["type"] = {
        {"name", current_node["structures"][1 - request_structure_index]["name"]},
        {"namespaces", result["type"]["namespaces"]},
    };
    auto service_name = current_node["structures"][request_structure_index]["name"].get<std::string>();
    service_name = service_name.substr(0, service_name.size() - std::string_view("_Request").size());
    result["services"][0]["type"] = {
        {"namespaces", result["type"]["namespaces"]},
        {"name", service_name}};

    result["services"][0]["event_message"] = make_service_event(result["services"][0]["type"]);
  } else if (current_node["structures"].size() == 3) {  // Action
    result["actions"][0]["interface_path"] = result["interface_path"];
    // Handle Goal, Result and Feedback messages
    for (auto& structure : current_node["structures"]) {
      if (structure["name"].get<std::string>().ends_with("_Goal")) {
        result["actions"][0]["goal"] = structure;
        result["actions"][0]["goal"]["constants"] = get_constants(current_node, result["actions"][0]["goal"]["name"]).value_or(json::array());
        result["actions"][0]["goal"]["type"] = {
            {"name", structure["name"]},
            {"namespaces", result["type"]["namespaces"]},
        };
      } else if (structure["name"].get<std::string>().ends_with("_Result")) {
        result["actions"][0]["result"] = structure;
        result["actions"][0]["result"]["constants"] = get_constants(current_node, result["actions"][0]["result"]["name"]).value_or(json::array());
        result["actions"][0]["result"]["type"] = {
            {"name", structure["name"]},
            {"namespaces", result["type"]["namespaces"]},
        };
      } else if (structure["name"].get<std::string>().ends_with("_Feedback")) {
        result["actions"][0]["feedback"] = structure;
        result["actions"][0]["feedback"]["constants"] = get_constants(current_node, result["actions"][0]["feedback"]["name"]).value_or(json::array());
        result["actions"][0]["feedback"]["type"] = {
            {"name", structure["name"]},
            {"namespaces", result["type"]["namespaces"]},
        };
      }
    }

    auto action_name = result["actions"][0]["goal"]["type"]["name"].get<std::string>();
    action_name = action_name.substr(0, action_name.size() - std::string_view("_Goal").size());
    result["actions"][0]["type"] = {
        {"namespaces", result["type"]["namespaces"]},
        {"name", action_name}};

    result["actions"][0]["send_goal_service"] = make_action_send_goal(result["actions"][0]["type"]);
    result["actions"][0]["get_result_service"] = make_action_get_result_service(result["actions"][0]["type"]);
    result["actions"][0]["feedback_message"] = make_action_feedback_message(result["actions"][0]["type"]);
  }

  // Check for non-ASCII characters
  result["has_non_ascii"] = check_non_ascii(result);

  return result;
}

}  // namespace rosidlcpp_parser