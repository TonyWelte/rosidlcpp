#include <rosidlcpp_generator_core/generator_utils.hpp>

#include <cctype>
#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

namespace rosidlcpp_core {

/**
 * Debug tools
 */

void print_indented_json(const nlohmann::json& value) {
  std::cout << value.dump(4) << '\n';
}

/**
 * String tools
 */

auto escape_string(const std::string& str) -> std::string {
  std::string escaped_str;
  for (const auto& c : str) {
    if (c == '"') {
      escaped_str += R"(\")";
    } else if (c == '\\') {
      escaped_str += "\\\\";
    } else {
      escaped_str += c;
    }
  }
  return escaped_str;
}

auto string_contains(const std::string& str, const std::string& substr) -> bool {
  return str.find(substr) != std::string::npos;
}

std::string format_string(const std::string& format, const nlohmann::json& arg1) {
  if (arg1.is_string()) {
    return fmt::format(fmt::runtime(format), arg1.get<std::string>());
  }
  if (arg1.is_number_integer()) {
    return fmt::format(fmt::runtime(format), arg1.get<int>());
  }
  if (arg1.is_number_float()) {
    return fmt::format(fmt::runtime(format), arg1.get<double>());
  }
  return std::string{"unknown"};
}

auto format_string(const std::string& format, const nlohmann::json& arg1, const nlohmann::json& arg2) -> std::string {
  if (arg1.is_string()) {
    if (arg2.is_string()) {
      return fmt::format(fmt::runtime(format), arg1.get<std::string>(),
                         arg2.get<std::string>());
    }
    if (arg2.is_number_integer()) {
      return fmt::format(fmt::runtime(format), arg1.get<std::string>(),
                         arg2.get<long long>());
    }
    if (arg2.is_number_float()) {
      return fmt::format(fmt::runtime(format), arg1.get<std::string>(),
                         arg2.get<double>());
    }
  }
  if (arg1.is_number_integer()) {
    if (arg2.is_string()) {
      return fmt::format(fmt::runtime(format), arg1.get<long long>(),
                         arg2.get<std::string>());
    }
    if (arg2.is_number_integer()) {
      return fmt::format(fmt::runtime(format), arg1.get<long long>(),
                         arg2.get<long long>());
    }
    if (arg2.is_number_float()) {
      return fmt::format(fmt::runtime(format), arg1.get<long long>(),
                         arg2.get<double>());
    }
  }
  if (arg1.is_number_float()) {
    if (arg2.is_string()) {
      return fmt::format(fmt::runtime(format), arg1.get<double>(),
                         arg2.get<std::string>());
    }
    if (arg2.is_number_integer()) {
      return fmt::format(fmt::runtime(format), arg1.get<double>(),
                         arg2.get<long long>());
    }
    if (arg2.is_number_float()) {
      return fmt::format(fmt::runtime(format), arg1.get<double>(),
                         arg2.get<double>());
    }
  }
  return std::string{"unknown"};
}

auto replace_string(std::string str, const std::string& substr, const std::string& replacement) -> std::string {
  std::size_t pos = 0;
  while ((pos = str.find(substr, pos)) != std::string::npos) {
    str.replace(pos, substr.length(), replacement);
    pos += replacement.length();
  }
  return str;
}

auto camel_to_snake(const std::string& input) -> std::string {
  std::string result;
  bool wasPrevUppercase = false;  // Track if previous character was uppercase

  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
    bool isCurrUppercase =
        isupper(c);  // Check if the current character is uppercase

    if (isCurrUppercase) {
      // Add an underscore if the previous character was not uppercase OR
      // if the next character is lowercase (to handle transitions like
      // RGBAColor → rgba_color)
      if (i > 0 && (!wasPrevUppercase ||
                    (i + 1 < input.length() && islower(input[i + 1])))) {
        result += '_';
      }
      result += tolower(c);  // Convert current uppercase letter to lowercase
    } else {
      result += c;  // Append lowercase letter as-is
    }

    wasPrevUppercase = isCurrUppercase;  // Update the tracking flag
  }

  return result;
}

/**
 * List tools
 */

auto span(const nlohmann::json& list, int start, int end) -> nlohmann::json {
  return nlohmann::json(list.begin() + start, list.begin() + end);
}

auto push_back(nlohmann::json list, const nlohmann::json& value) -> nlohmann::json {
  if (value.is_array()) {
    for (const auto& v : value) {
      list.push_back(v);
    }
  } else {
    list.push_back(value);
  }
  return list;
}

auto insert(nlohmann::json list, int index, const nlohmann::json& value) -> nlohmann::json {
  list.insert(list.begin() + index, value);
  return list;
}

auto custom_range(int start, int end, int step) -> std::vector<int> {
  std::vector<int> result;
  int v = start;
  while (step > 0 ? v < end : v > end) {
    result.push_back(v);
    v += step;
  }
  return result;
}

auto type_to_c_typename(const nlohmann::json& type) -> std::string {
  return fmt::format("{}__{}", fmt::join(type["namespaces"], "__"), type["name"].get<std::string>());
}

auto is_sequence(const nlohmann::json& type) -> bool {
  return type["name"] == "sequence";
}

auto is_array(const nlohmann::json& type) -> bool { return type["name"] == "array"; }

auto is_bounded(const nlohmann::json& type) -> bool {
  return type.contains("maximum_size") || type.contains("size");
}

auto is_nestedtype(const nlohmann::json& type) -> bool {
  return is_sequence(type) || is_array(type);
}

auto is_string(const nlohmann::json& type) -> bool {
  return type["name"] == "string" || type["name"] == "wstring";
}

auto is_primitive(const nlohmann::json& type) -> bool {
  const std::set<std::string_view> PRIMITIVE_TYPES = {
      "boolean", "octet", "char", "wchar", "float", "double",
      "long double", "uint8", "int8", "uint16", "int16", "uint32",
      "int32", "uint64", "int64" /*, "string", "wstring"*/};

  return PRIMITIVE_TYPES.find(type["name"].get<std::string>()) !=
         PRIMITIVE_TYPES.end();
}

auto is_float(const nlohmann::json& type) -> bool {
  return type["name"] == "float" || type["name"] == "double" ||
         type["name"] == "long double";
}

auto is_namespaced(const nlohmann::json& type) -> bool {
  return type.contains("namespaces");
}

auto is_character(const nlohmann::json& type) -> bool {
  return type["name"] == "char" || type["name"] == "wchar";
}

auto is_integer(const nlohmann::json& type) -> bool {
  const std::set<std::string_view> INTEGER_TYPES = {"uint8", "uint16", "uint32", "uint64",
                                                    "int8", "int16", "int32", "int64"};
  return INTEGER_TYPES.find(type["name"].get<std::string>()) !=
         INTEGER_TYPES.end();
}

auto is_unsigned_integer(const nlohmann::json& type) -> bool {
  const std::set<std::string_view> UNSIGNED_TYPES = {"uint8", "uint16",
                                                     "uint32", "uint64"};
  return UNSIGNED_TYPES.find(type["name"].get<std::string>()) !=
         UNSIGNED_TYPES.end();
}

auto is_signed_integer(const nlohmann::json& type) -> bool {
  const std::set<std::string_view> SIGNED_TYPES = {"int8", "int16", "int32",
                                                   "int64"};
  return SIGNED_TYPES.find(type["name"].get<std::string>()) !=
         SIGNED_TYPES.end();
}

auto is_action_type(const nlohmann::json& type) -> bool {
  return type["name"].get<std::string>().ends_with(ACTION_GOAL_SUFFIX) ||
         type["name"].get<std::string>().ends_with(ACTION_RESULT_SUFFIX) ||
         type["name"].get<std::string>().ends_with(ACTION_FEEDBACK_SUFFIX);
}
auto is_service_type(const nlohmann::json& type) -> bool {
  return type["name"].get<std::string>().ends_with(SERVICE_REQUEST_MESSAGE_SUFFIX) ||
         type["name"].get<std::string>().ends_with(SERVICE_RESPONSE_MESSAGE_SUFFIX) ||
         type["name"].get<std::string>().ends_with(SERVICE_EVENT_MESSAGE_SUFFIX);
}

}  // namespace rosidlcpp_core