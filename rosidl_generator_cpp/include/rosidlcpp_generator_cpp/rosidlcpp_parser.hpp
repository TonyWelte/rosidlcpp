#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json_fwd.hpp>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

bool is_sequence(const std::string &str) {
  return str.substr(0, 9) == "sequence<" && str.back() == '>';
}

bool is_array(const std::string &str) { return str.back() == ']'; }

bool is_bounded_string(std::string_view str) {
  if((str.substr(0, 7) == "string<" || str.substr(0, 8) == "wstring<") && str.back() == '>') {
    return true;
  }

  return false;
}

bool is_nestedtype(const std::string &str) {
  return is_sequence(str) || is_array(str); // TODO: Add bounded sequence
}

bool is_string(const std::string &str) {
  return str == "string" || str == "wstring" || is_bounded_string(str);
}

bool is_primitive(const std::string &str) {
  std::set<std::string_view> PRIMITIVE_TYPES = {
      "boolean",     "octet",  "char",  "wchar",  "float",  "double",
      "long double", "uint8",  "int8",  "uint16", "int16",  "uint32",
      "int32",       "uint64", "int64", "string", "wstring"};

  return PRIMITIVE_TYPES.find(str) != PRIMITIVE_TYPES.end();
}

bool is_namespaced_type(const std::string &str) {
  return !is_primitive(str) && !is_nestedtype(str) && !is_bounded_string(str);
}

std::vector<std::string> split_string(std::string_view value, std::string_view sep) {
  std::vector<std::string> result;

  auto cursor = value.find(sep);
  while(cursor != std::string::npos) {
    result.push_back(std::string{value.substr(0, cursor)});
    value.remove_prefix(cursor + sep.size());
    cursor = value.find(sep);
  }
  result.push_back(std::string{value});

  return result;
}

nlohmann::json
parse_idl_type(const std::string &type_string,
               const nlohmann::json &typedefs = nlohmann::json::object()) {
  nlohmann::json result;

  auto resolve_typdedef = [typedefs](const std::string &str) {
    std::string underlying_type = str;
    while (typedefs.contains(underlying_type)) {
      underlying_type = typedefs[underlying_type];
    }
    return underlying_type;
  };

  std::string tmp_type_string = resolve_typdedef(type_string);

  result["typename"] = tmp_type_string;
  if (is_sequence(tmp_type_string)) {
    tmp_type_string = tmp_type_string.substr(9, tmp_type_string.size() - 9 - 1);
    auto pos = tmp_type_string.find_first_of(',');
    if (pos == std::string::npos) { // Unbounded sequence
      result["value_type"] = tmp_type_string;
    } else { // Bounded sequence
      result["value_type"] = tmp_type_string.substr(0, pos);
      result["maximum_size"] = tmp_type_string.substr(pos + 2);
    }
  }
  if (is_array(tmp_type_string)) {
    result["value_type"] =
        resolve_typdedef(tmp_type_string.substr(0, tmp_type_string.find('[')));
    result["size"] = std::stoi(tmp_type_string.substr(
        tmp_type_string.find('[') + 1,
        tmp_type_string.find(']') - tmp_type_string.find('[') - 1));
  }
  if(is_bounded_string(tmp_type_string)) {
      auto size_index = tmp_type_string.find_first_of('<') + 1;
      result["maximum_size"] = tmp_type_string.substr(size_index, tmp_type_string.size() - 1 - size_index);
      result["typename"] = tmp_type_string.substr(0, size_index-1);
  }

  if (is_nestedtype(result["typename"])) {
    if (is_namespaced_type(result["value_type"])) {
      auto parts = split_string(result["value_type"], ':');
      parts.pop_back();
      result["namespaces"] = parts;
    }
  } else {
    if (is_namespaced_type(result["typename"])) {
      auto parts = split_string(result["typename"], ':');
      parts.pop_back();
      result["namespaces"] = parts;
    }
  }

  return result;
}