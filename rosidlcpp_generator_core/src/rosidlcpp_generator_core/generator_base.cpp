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

#include <inja/inja.hpp>

#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

namespace rosidlcpp_core {

std::string escape_string(const std::string& str) {
  std::string escaped_str;
  for (const auto& c : str) {
    if (c == '"') {
      escaped_str += "\\\"";
    } else {
      escaped_str += c;
    }
  }
  return escaped_str;
}

GeneratorBase::GeneratorBase() : m_env{} {
  m_env.set_trim_blocks(true);
  m_env.set_lstrip_blocks(true);

  /*
   * Register callbacks
   */

  // Debug
  m_env.add_void_callback("debug", 1, [](inja::Arguments& args) -> void {
    std::cout << args.at(0)->dump(4) << std::endl;
  });

  // Formatting
  m_env.add_callback(
      "convert_camel_case_to_lower_case_underscore", 1,
      [](inja::Arguments& args) {
        return rosidlcpp_core::camel_to_snake(args.at(0)->get<std::string>());
      });
  m_env.add_callback("format", 2, [](inja::Arguments& args) {
    auto format = args.at(0)->get<std::string>();
    const auto arg1 = *args.at(1);
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
  });
  m_env.add_callback("format", 3, [](inja::Arguments& args) {
    auto format = args.at(0)->get<std::string>();
    const auto arg1 = *args.at(1);
    const auto arg2 = *args.at(2);

    auto value1 = arg1.dump();
    auto value2 = arg2.dump();

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
  });
  m_env.add_callback("replace", 3, [](inja::Arguments& args) {
    auto str = args.at(0)->get<std::string>();
    const auto substr = args.at(1)->get<std::string>();
    const auto replacement = args.at(2)->get<std::string>();
    std::size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
      str.replace(pos, substr.length(), replacement);
      pos += replacement.length();
    }
    return str;
  });

  // Constants
  m_env.add_callback(
      "EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME", 0, [](inja::Arguments&) {
        return EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME;
      });
  m_env.add_callback("SERVICE_EVENT_MESSAGE_SUFFIX", 0, [](inja::Arguments&) {
    return SERVICE_EVENT_MESSAGE_SUFFIX;
  });
  m_env.add_callback("SERVICE_REQUEST_MESSAGE_SUFFIX", 0, [](inja::Arguments&) {
    return SERVICE_REQUEST_MESSAGE_SUFFIX;
  });
  m_env.add_callback("SERVICE_RESPONSE_MESSAGE_SUFFIX", 0, [](inja::Arguments&) {
    return SERVICE_RESPONSE_MESSAGE_SUFFIX;
  });
  m_env.add_callback("ACTION_GOAL_SUFFIX", 0, [](inja::Arguments&) {
    return ACTION_GOAL_SUFFIX;
  });
  m_env.add_callback("ACTION_RESULT_SUFFIX", 0, [](inja::Arguments&) {
    return ACTION_RESULT_SUFFIX;
  });
  m_env.add_callback("ACTION_FEEDBACK_SUFFIX", 0, [](inja::Arguments&) {
    return ACTION_FEEDBACK_SUFFIX;
  });
  m_env.add_callback("ACTION_GOAL_SERVICE_SUFFIX", 0, [](inja::Arguments&) {
    return ACTION_GOAL_SERVICE_SUFFIX;
  });
  m_env.add_callback("ACTION_RESULT_SERVICE_SUFFIX", 0, [](inja::Arguments&) {
    return ACTION_RESULT_SERVICE_SUFFIX;
  });
  m_env.add_callback("ACTION_FEEDBACK_MESSAGE_SUFFIX", 0, [](inja::Arguments&) {
    return ACTION_FEEDBACK_MESSAGE_SUFFIX;
  });

  // Utility
  m_env.add_callback("span", 3, [](inja::Arguments& args) {
    auto list = *args.at(0);
    auto start = args.at(1)->get<int>();
    auto end = args.at(2)->get<int>();

    return nlohmann::json(list.begin() + start, list.begin() + end);
  });
  m_env.add_callback("push_back", 2, [](inja::Arguments& args) {
    auto list = *args.at(0);
    auto value = *args.at(1);
    if (value.is_array()) {
      for (const auto& v : value) {
        list.push_back(v);
      }
    } else {
      list.push_back(value);
    }
    return list;
  });
  m_env.add_callback("insert", 3, [](inja::Arguments& args) {
    auto list = *args.at(0);
    const auto index = args.at(1)->get<int>();
    list.insert(list.begin() + index, *args.at(2));
    return list;
  });
  m_env.add_callback("string_contains", 2, [](inja::Arguments& args) {
    const auto str = args.at(0)->get<std::string>();
    const auto substr = args.at(1)->get<std::string>();
    return str.find(substr) != std::string::npos;
  });
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
  m_env.add_callback("unique", 1, [](inja::Arguments& args) {  // also sorts
    auto list = *args.at(0);
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    return list;
  });
  m_env.add_callback("split_string", 2, [](inja::Arguments& args) {
    const auto value = args.at(0)->get<std::string>();
    const auto sep = args.at(1)->get<std::string>();

    return rosidlcpp_parser::split_string(value, sep);
  });
  m_env.add_callback("custom_range", 3, [](inja::Arguments& args) {
    const int start = args.at(0)->get<int>();
    const int end = args.at(1)->get<int>();
    const int step = args.at(2)->get<int>();
    std::vector<int> result;
    int v = start;
    while (step > 0 ? v < end : v > end) {
      result.push_back(v);
      v += step;
    }
    return result;
  });

  // Types
  m_env.add_callback("is_primitive", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_primitive(*args.at(0));
                     });
  m_env.add_callback("is_string", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_string(*args.at(0));
                     });
  m_env.add_callback("is_character", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_character(*args.at(0));
                     });
  m_env.add_callback("is_float", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_float(*args.at(0));
                     });
  m_env.add_callback("is_nestedtype", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_nestedtype(*args.at(0));
                     });
  m_env.add_callback("is_integer", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_signed_integer(*args.at(0)) || is_unsigned_integer(*args.at(0));
                     });
  m_env.add_callback("is_signed_integer", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_signed_integer(*args.at(0));
                     });
  m_env.add_callback("is_unsigned_integer", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_unsigned_integer(*args.at(0));
                     });
  m_env.add_callback("is_namespaced", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       return is_namespaced(*args.at(0));
                     });
  m_env.add_callback(
      "is_action_type", 1, [](inja::Arguments& args) -> nlohmann::json {
        const auto type = *args.at(0);
        return type["name"].get<std::string>().ends_with(ACTION_GOAL_SUFFIX) ||
               type["name"].get<std::string>().ends_with(
                   ACTION_RESULT_SUFFIX) ||
               type["name"].get<std::string>().ends_with(
                   ACTION_FEEDBACK_SUFFIX);
      });
  m_env.add_callback("is_service_type", 1,
                     [](inja::Arguments& args) -> nlohmann::json {
                       const auto type = *args.at(0);
                       return type["name"].get<std::string>().ends_with(
                                  SERVICE_REQUEST_MESSAGE_SUFFIX) ||
                              type["name"].get<std::string>().ends_with(
                                  SERVICE_RESPONSE_MESSAGE_SUFFIX);
                     });
}

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
      "int32", "uint64", "int64" /*, "string", "wstring"*/};

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