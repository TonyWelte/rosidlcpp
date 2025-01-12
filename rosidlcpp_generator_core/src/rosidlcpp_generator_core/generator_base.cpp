#include <rosidlcpp_generator_core/generator_base.hpp>

#include <cmath>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include <fmt/core.h>

#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

namespace rosidlcpp_core {

std::vector<std::pair<std::string, std::string>> parse_pairs(
    const std::vector<std::string> list) {
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

std::string camel_to_snake(const std::string& input) {
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

bool is_sequence(const nlohmann::json& type) {
  return type["name"] == "sequence";
}

bool is_array(const nlohmann::json& type) { return type["name"] == "array"; }

bool is_bounded(const nlohmann::json& type) {
  return type.contains("maximum_size") || type.contains("size");
}

bool is_nestedtype(const nlohmann::json& type) {
  return is_sequence(type) || is_array(type);
}

bool is_string(const nlohmann::json& type) {
  return type["name"] == "string" || type["name"] == "wstring";
}

bool is_primitive(const nlohmann::json& type) {
  const std::set<std::string_view> PRIMITIVE_TYPES = {
      "boolean", "octet", "char", "wchar", "float", "double",
      "long double", "uint8", "int8", "uint16", "int16", "uint32",
      "int32", "uint64", "int64", "string", "wstring"};

  return PRIMITIVE_TYPES.find(type["name"].get<std::string>()) !=
         PRIMITIVE_TYPES.end();
}

bool is_float(const nlohmann::json& type) {
  return type["name"] == "float" || type["name"] == "double" ||
         type["name"] == "long double";
}

bool is_namespaced(const nlohmann::json& type) {
  return type.contains("namespaces");
}

bool is_character(const nlohmann::json& type) {
  return type["name"] == "char" || type["name"] == "wchar";
}

bool is_unsigned_integer(const nlohmann::json& type) {
  const std::set<std::string_view> UNSIGNED_TYPES = {"uint8", "uint16",
                                                     "uint32", "uint64"};
  return UNSIGNED_TYPES.find(type["name"].get<std::string>()) !=
         UNSIGNED_TYPES.end();
}

bool is_signed_integer(const nlohmann::json& type) {
  const std::set<std::string_view> SIGNED_TYPES = {"int8", "int16", "int32",
                                                   "int64"};
  return SIGNED_TYPES.find(type["name"].get<std::string>()) !=
         SIGNED_TYPES.end();
}

}  // namespace rosidlcpp_core