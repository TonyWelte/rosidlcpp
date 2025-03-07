#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <inja/inja.hpp>
#include <iostream>
#include <limits>
#include <nlohmann/json_fwd.hpp>
#include <ostream>
#include <rosidlcpp_generator_cpp/rosidlcpp_generator_cpp.hpp>

#include <cassert>
#include <cctype>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

using rosidlcpp_core::CallbackArgs;

// Member class definition
class Member {
 public:
  std::string name;
  nlohmann::json default_value;
  nlohmann::json zero_value;
  bool zero_need_array_override;
  nlohmann::json type;
  int num_prealloc;

  Member(const std::string &name)
      : name(name), zero_need_array_override(false), num_prealloc(0) {}

  // Compare if the default and zero values are the same for two members
  bool same_default_and_zero_value(const Member &other) const {
    return default_value == other.default_value &&
           zero_value == other.zero_value;
  }

  // Convert Member to JSON
  nlohmann::json to_json() const {
    return {{"name", name},
            {"default_value", default_value},
            {"zero_value", zero_value},
            {"zero_need_array_override", zero_need_array_override},
            {"type", type},
            {"num_prealloc", num_prealloc}};
  }
};

// CommonMemberSet class definition
class CommonMemberSet {
 public:
  std::vector<Member> members;

  bool add_member(const Member &member) {
    if (members.empty() || members.back().same_default_and_zero_value(member)) {
      members.push_back(member);
      return true;
    }
    return false;
  }

  // Convert CommonMemberSet to JSON
  nlohmann::json to_json() const {
    nlohmann::json members_json = nlohmann::json::array();
    for (const auto &member : members) {
      members_json.push_back(member.to_json());
    }
    return members_json;
  }
};

// Floating point types for checking
const std::vector<std::string> FLOATING_POINT_TYPES = {"float", "double",
                                                       "long double"};

nlohmann::json default_value_from_type(const nlohmann::json &type) {
  if (rosidlcpp_core::is_string(type)) {
    return "";  // Empty string for generic string types
  } else if (rosidlcpp_core::is_float(type)) {
    return 0.0;  // Default for floating point types
  } else if (type["name"] == "boolean") {
    return false;  // Default for boolean types
  }
  return 0;  // Default for other types (integers)
}

std::string primitive_value_to_cpp(const nlohmann::json &type,
                                   const nlohmann::json &value) {
  auto type_name = type["name"].get<std::string>();
  if (type_name == "string") {
    return "\"" + rosidlcpp_core::escape_string(value.get<std::string>()) + "\"";
  } else if (type_name == "wstring") {
    return "u\"" + rosidlcpp_core::escape_string(value.get<std::string>()) + "\"";
  } else if (type_name == "boolean") {
    return value.get<bool>() ? "true" : "false";
  } else if (type_name == "short" || type_name == "unsigned short" || type_name == "char" ||
             type_name == "wchar" || type_name == "octet" || type_name == "int8" ||
             type_name == "uint8" || type_name == "int16" || type_name == "uint16") {
    return value.is_string() ? value.get<std::string>()
                             : std::to_string(value.get<int>());
  } else if (type_name == "double" || type_name == "long double") {
    return value.is_string() ? value.get<std::string>() : value.dump();
  } else if (type_name == "int32") {
    const int32_t int_value = value.get<int32_t>();
    return int_value > std::numeric_limits<int32_t>::min() ? std::to_string(int_value) + "l" : std::format("({}l - 1)", int_value + 1);
  } else if (type_name == "uint32") {
    return std::to_string(value.get<uint32_t>()) + "ul";
  } else if (type_name == "int64") {
    const int64_t int_value = value.get<int64_t>();
    return int_value > std::numeric_limits<int64_t>::min() ? std::to_string(int_value) + "ll" : std::format("({}ll - 1)", int_value + 1);
  } else if (type_name == "uint64") {
    return std::to_string(value.get<uint64_t>()) + "ull";
  } else if (type_name == "float") {
    return value.dump() + "f";
  }

  throw std::invalid_argument("unknown primitive type: " + type_name);
}

std::string value_to_cpp(const nlohmann::json &type, const nlohmann::json &value) {
  // Assume that we are working with arrays
  std::vector<std::string> cpp_values;

  // For simplicity, we'll consider strings as arrays of characters
  bool is_string_array = (type["name"].get<std::string>() == "string");

  for (const auto &single_value : value) {
    std::string cpp_value = primitive_value_to_cpp(type, single_value);
    if (is_string_array) {
      cpp_values.push_back("{" + cpp_value + "}");
    } else {
      cpp_values.push_back(cpp_value);
    }
  }

  std::string cpp_value =
      "{" + fmt::format("{}", fmt::join(cpp_values, ", ")) + "}";

  // Wrap in an extra set of braces if needed to avoid scalar initializer;
  // warnings
  if (cpp_values.size() > 1 && !is_string_array) {
    cpp_value = "{" + cpp_value + "}";
  }

  return cpp_value;
}

nlohmann::json
create_init_alloc_and_member_lists(const nlohmann::json &message) {
  std::vector<std::string> init_list;
  std::vector<std::string> alloc_list;
  std::vector<CommonMemberSet> member_list;

  for (const auto &field : message["members"]) {
    Member member(field["name"]);
    member.type = field["type"];

    if (rosidlcpp_core::is_array(field["type"])) {
      alloc_list.push_back(field["name"].get<std::string>() + "(_alloc)");

      if (rosidlcpp_core::is_primitive(field["type"]["value_type"]) ||
          rosidlcpp_core::is_string(field["type"]["value_type"])) {
        auto default_value = default_value_from_type(field["type"]["value_type"]);
        std::string single_value = primitive_value_to_cpp(field["type"]["value_type"], default_value);
        member.zero_value = {};
        for (int i = 0; i < field["type"]["size"].get<int>(); ++i) {
          member.zero_value.push_back(single_value);
        }

        if (field.contains("default")) {
          auto default_val = field["default"];
          for (const auto &v : default_val) {
            member.default_value.push_back(primitive_value_to_cpp(field["type"]["value_type"], v));
          }
        }
      } else {
        member.zero_value = nlohmann::json::array();  // Empty initializer for
                                                      // non-primitive types
        member.zero_need_array_override = true;
      }
    } else if (rosidlcpp_core::is_sequence(field["type"])) {
      if (field.contains("default")) {
        auto default_val = field["default"];
        member.default_value = value_to_cpp(field["type"]["value_type"], default_val);
        member.num_prealloc = default_val.size();
      }
    } else {
      if (rosidlcpp_core::is_primitive(field["type"]) || rosidlcpp_core::is_string(field["type"])) {
        if (rosidlcpp_core::is_string(field["type"])) {
          alloc_list.push_back(field["name"].get<std::string>() + "(_alloc)");
        }

        auto default_value = default_value_from_type(field["type"]);
        member.zero_value = primitive_value_to_cpp(field["type"], default_value);

        if (field.contains("default")) {
          member.default_value = primitive_value_to_cpp(field["type"], field["default"]);
        }
      } else {
        init_list.push_back(field["name"].get<std::string>() + "(_init)");
        alloc_list.push_back(field["name"].get<std::string>() + "(_alloc, _init)");
      }
    }

    if ((field.contains("default")) ||
        (!member.zero_value.is_null())) {
      if (member_list.empty() || !member_list.back().add_member(member)) {
        CommonMemberSet commonset;
        commonset.add_member(member);
        member_list.push_back(commonset);
      }
    }
  }

  // Convert the results to JSON format
  nlohmann::json output_json;
  output_json["init_list"] = init_list;
  output_json["alloc_list"] = alloc_list;
  output_json["default_value_members"] = false;
  output_json["zero_value_members"] = false;
  output_json["non_defaulted_zero_initialized_members"] = false;

  nlohmann::json member_list_json = nlohmann::json::array();
  for (const auto &commonset : member_list) {
    member_list_json.push_back(commonset.to_json());

    if (!commonset.members.front().default_value.empty()) {
      output_json["default_value_members"] = true;
    }
    if (!commonset.members.front().zero_value.is_null()) {
      output_json["zero_value_members"] = true;
    }
    if ((!commonset.members.front().zero_value.is_null() ||
         commonset.members.front().zero_need_array_override) &&
        commonset.members.front().default_value.empty()) {
      output_json["non_defaulted_zero_initialized_members"] = true;
    }
  }
  output_json["member_list"] = member_list_json;

  return output_json;
}

std::string strip_end_until_char(const std::string &value, char limit) {
  return value.substr(0, value.find_last_of(limit));
  auto it = value.rbegin();
  while (it != value.rend()) {
    if (*it == limit) {
      break;
    }

    ++it;
  }

  std::string tmp(it, value.rend());  // TODO: Find less hacky way to do that

  return std::string(tmp.rbegin(), tmp.rend());
}

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

std::string msg_type_only_to_cpp(const nlohmann::json &type) {
  nlohmann::json main_type;
  std::string cpp_type;

  // Handle nested types
  if (type.contains("value_type")) {
    main_type = type["value_type"];
  } else {
    main_type = type;
  }

  // Check the basic type
  if (rosidlcpp_core::is_primitive(main_type) || rosidlcpp_core::is_string(main_type)) {
    cpp_type = MSG_TYPE_TO_CPP.at(main_type["name"].get<std::string>());
  } else if (rosidlcpp_core::is_namespaced(main_type)) {
    cpp_type = fmt::format("{}::{}_<ContainerAllocator>", fmt::join(main_type["namespaces"], "::"), main_type["name"].get<std::string>());
  } else {
    throw std::invalid_argument("Unknown type encountered: " + type["name"].get<std::string>());
  }

  return cpp_type;
}

std::string msg_type_to_cpp(const nlohmann::json &type) {
  std::string cpp_type = msg_type_only_to_cpp(type);

  // Handle nested types
  if (rosidlcpp_core::is_nestedtype(type)) {
    if (rosidlcpp_core::is_sequence(type) &&
        !rosidlcpp_core::is_bounded(type)) {  // Unbounded sequence
      return "std::vector<" + cpp_type +
             ", typename std::allocator_traits<ContainerAllocator>::template "
             "rebind_alloc<" +
             cpp_type + ">>";
    } else if (rosidlcpp_core::is_sequence(type)) {  // Bounded sequence
      return std::format(
          "rosidl_runtime_cpp::BoundedVector<{}, {}, typename "
          "std::allocator_traits<ContainerAllocator>::template "
          "rebind_alloc<{}>>",
          cpp_type, type["maximum_size"].get<int>(), cpp_type);
    } else if (rosidlcpp_core::is_array(type)) {
      unsigned int array_size = type["size"];
      return "std::array<" + cpp_type + ", " + std::to_string(array_size) + ">";
    } else {
      throw std::invalid_argument("Unknown nested type encountered: " +
                                  type["nested_type"].get<std::string>());
    }
  }

  return cpp_type;  // Return simple types as-is
}

GeneratorCpp::GeneratorCpp(int argc, char **argv) : GeneratorBase() {
  // Arguments
  argparse::ArgumentParser argument_parser("rosidl_generator_cpp");
  argument_parser.add_argument("--generator-arguments-file")
      .required()
      .help("The location of the file containing the generator arguments");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    std::cerr << argument_parser;
    std::exit(1);  // TODO: Don't use exit in constructor
  }

  auto generator_arguments_file =
      argument_parser.get<std::string>("--generator-arguments-file");

  m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  m_env.set_input_path(m_arguments.template_dir + "/");
  _output_path = m_arguments.output_dir + "/";

  // m_env.add_callback("is_python_builtin", 1, is_python_builtin);
  m_env.add_callback("get_includes", 2, [](inja::Arguments &args) {
    std::vector<std::pair<std::string, std::vector<std::string>>> includes;

    const auto message = *args.at(0);
    const std::string suffix = *args.at(1);
    for (const auto &member : message["members"]) {
      auto type = member["type"];
      if (rosidlcpp_core::is_nestedtype(type)) {
        type = member["type"]["value_type"];
      }
      if (rosidlcpp_core::is_namespaced(type)) {
        std::string type_name;
        if ((message["type"]["namespaces"].back() == "action" ||
             message["type"]["namespaces"].back() == "srv") &&
            (type["name"].get<std::string>().ends_with("_Request") || type["name"].get<std::string>().ends_with("_Response"))) {
          type_name = fmt::format("{}::{}", fmt::join(type["namespaces"], "::"), strip_end_until_char(type["name"].get<std::string>(), '_'));
          std::string current_struct_type = "";
          for (const auto &ns :
               message["type"]["namespaces"]) {
            current_struct_type += (ns.get<std::string>() + "::");
          }
          current_struct_type += strip_end_until_char(
              message["type"]["name"].get<std::string>(), '_');
          if (type_name == current_struct_type) {
            continue;
          }
        }
        if (type["name"].get<std::string>().ends_with("_Goal") || type["name"].get<std::string>().ends_with("_Result") ||
            type["name"].get<std::string>().ends_with("_Feedback")) {
          type_name = strip_end_until_char(type["name"].get<std::string>(), '_');
        } else {
          type_name = type["name"].get<std::string>();
        }
        std::string tmp;
        for (const auto &ns : type["namespaces"]) {
          tmp += ns.get<std::string>() + "/";
        }
        tmp += "detail/";
        tmp += rosidlcpp_core::camel_to_snake(type_name);
        tmp += suffix;
        // Add include member keeping the order
        auto it = std::find_if(includes.begin(), includes.end(),
                               [tmp](const auto &v) { return v.first == tmp; });
        if (it == includes.end()) {
          it = includes.insert(it, {tmp, {}});
        }
        it->second.push_back(member["name"].get<std::string>());
      }
    }
    // Convert to json list
    nlohmann::json result = nlohmann::json::array();
    for (const auto &[key, value] : includes) {
      result.push_back({{"member_names", value}, {"header_file", key}});
    }

    return result;
  });
  m_env.add_callback("MSG_TYPE_TO_CPP", 1, [](inja::Arguments &args) {
    const auto type = args.at(0)->get<std::string>();
    const auto v = MSG_TYPE_TO_CPP.find(type);
    if (v != MSG_TYPE_TO_CPP.end()) {
      return v->second;
    } else {
      return type;
    }
  });

  m_env.add_callback("msg_type_to_cpp", 1, [](inja::Arguments &args) {
    return msg_type_to_cpp(*args.at(0));
  });

  m_env.add_callback("create_init_alloc_and_member_lists", 1,
                     [](inja::Arguments &args) {
                       return create_init_alloc_and_member_lists(*args.at(0));
                     });
  m_env.add_callback("generate_zero_string", 2, [](inja::Arguments &args) {
    const auto membset = (*args.at(0));
    const std::string fill_args = *args.at(1);

    nlohmann::json result = nlohmann::json::array();
    for (const auto &member : membset) {
      if (member["name"].get<std::string>() == "basic_types_values" ||
          member["name"].get<std::string>() == "constants_values" ||
          member["name"].get<std::string>() == "defaults_values") {
      }
      if (member["zero_value"].is_null()) {
        // TODO
        continue;
      }
      if (member["zero_value"].is_array()) {
        if (member["num_prealloc"] > 0) {
          result.push_back(
              "this->" + member["name"].get<std::string>() + ".resize(" +
              std::to_string(member["num_prealloc"].get<int>()) + ");");
        }
        if (member["zero_need_array_override"]) {
          result.push_back("this->" + member["name"].get<std::string>() +
                           ".fill(" + msg_type_only_to_cpp(member["type"]) +
                           "{" + fill_args + "});");

        } else {
          result.push_back(
              "std::fill<typename " + msg_type_to_cpp(member["type"]) +
              "::iterator, " + msg_type_only_to_cpp(member["type"]) +
              ">(this->" + member["name"].get<std::string>() +
              ".begin(), this->" + member["name"].get<std::string>() +
              ".end(), " + member["zero_value"][0].get<std::string>() + ");");
        }
      } else {
        result.push_back("this->" + member["name"].get<std::string>() + " = " +
                         member["zero_value"].get<std::string>() + ";");
      }
    }
    return result;
  });
  m_env.add_callback("generate_default_string", 2, [](inja::Arguments &args) {
    const auto membset = (*args.at(0));
    const std::string fill_args = *args.at(1);

    nlohmann::json result = nlohmann::json::array();
    for (const auto &member : membset) {
      if (!member.contains("default_value") || member["default_value"].is_null()) {
        continue;
      }
      if (member["num_prealloc"] > 0) {
        result.push_back(
            "this->" + member["name"].get<std::string>() + ".resize(" +
            std::to_string(member["num_prealloc"].get<int>()) + ");");
      }
      if (member["default_value"].is_array()) {
        if (std::all_of(member["default_value"].begin(),
                        member["default_value"].end(),
                        [first = member["default_value"]](const auto &value) {
                          return value == first;
                        })) {
          result.push_back(
              "this->" + member["name"].get<std::string>() + ".fill(" +
              msg_type_only_to_cpp(member["type"]["name"]) + "{" +
              member["default_value"][0].get<std::string>() + "});");
        } else {
          for (size_t i = 0; i < member["default_value"].size(); ++i) {
            result.push_back("this->" + member["name"].get<std::string>() +
                             "[" + std::to_string(i) + "] = " +
                             member["default_value"][i].get<std::string>() + ";");
          }
        }
      } else {
        result.push_back("this->" + member["name"].get<std::string>() + " = " +
                         member["default_value"].get<std::string>() + ";");
      }
    }
    return result;
  });
  m_env.add_callback("get_fixed_template_strings", 1,
                     [](const inja::Arguments &args) -> nlohmann::json {
                       const auto members = *args.at(0);

                       std::set<std::string> fixed_template_strings;
                       for (const auto &member : members) {
                         auto type = member["type"];
                         if (rosidlcpp_core::is_sequence(type)) {
                           return {"false"};
                         }
                         if (rosidlcpp_core::is_array(type)) {
                           type = member["type"]["value_type"];
                         }
                         if (rosidlcpp_core::is_string(type)) {
                           return {"false"};
                         }
                         if (rosidlcpp_core::is_namespaced(type)) {
                           fixed_template_strings.insert(fmt::format(
                               "has_fixed_size<{}::{}>::value", fmt::join(type["namespaces"], "::"),
                               type["name"].get<std::string>()));
                         }
                       }
                       if (fixed_template_strings.empty()) {
                         return {"true"};
                       } else {
                         return fixed_template_strings;
                       }
                     });
  m_env.add_callback(
      "get_bounded_template_strings", 1,
      [](const inja::Arguments &args) -> nlohmann::json {
        const auto members = *args.at(0);

        std::set<std::string> bounded_template_strings;
        for (const auto &member : members) {
          auto type = member["type"];
          if (rosidlcpp_core::is_sequence(type) &&
              !member["type"].contains("maximum_size")) {
            return {"false"};
          }
          if (rosidlcpp_core::is_nestedtype(type) /* bounded sequence or array */) {
            type = member["type"]["value_type"];
          }
          if (rosidlcpp_core::is_string(type)) {
            return {"false"};
          }
          if (rosidlcpp_core::is_namespaced(type)) {
            bounded_template_strings.insert(fmt::format(
                "has_bounded_size<{}::{}>::value", fmt::join(type["namespaces"], "::"),
                type["name"].get<std::string>()));
          }
        }
        if (bounded_template_strings.empty()) {
          return {"true"};
        } else {
          return bounded_template_strings;
        }
      });
}

void GeneratorCpp::run() {
  // Load templates
  inja::Template template_idl_builder = m_env.parse_template("./idl__builder.hpp.template");
  inja::Template template_idl_struct = m_env.parse_template("./idl__struct.hpp.template");
  inja::Template template_idl_traits = m_env.parse_template("./idl__traits.hpp.template");
  inja::Template template_idl_type_support = m_env.parse_template("./idl__type_support.hpp.template");
  inja::Template template_idl = m_env.parse_template("./idl.hpp.template");

  // Combined ros_json
  nlohmann::json pkg_json;
  pkg_json["package_name"] = m_arguments.package_name;
  pkg_json["messages"] = nlohmann::json::array();
  pkg_json["services"] = nlohmann::json::array();
  pkg_json["actions"] = nlohmann::json::array();

  // Generate message specific files
  for (const auto &[path, file_path] : m_arguments.idl_tuples) {
    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    ros_json["package_name"] = m_arguments.package_name;

    const auto msg_directory = ros_json["interface_path"]["filedir"].get<std::string>();
    const auto msg_type = ros_json["interface_path"]["filename"].get<std::string>();

    auto write_template = [&](const inja::Template& template_object, const nlohmann::json& data, std::string_view output_file) {
      std::string result = m_env.render(template_object, data);

      if(rosidlcpp_parser::has_non_ascii(result)) {
        result = "\ufeff// NOLINT: This file starts with a BOM since it contain non-ASCII characters\n" + result;
      }

      std::ofstream file(_output_path + std::string{output_file});
      file << result;
      file.close();
    };

    write_template(template_idl_builder, ros_json, std::format("{}/detail/{}__builder.hpp", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_struct, ros_json, std::format("{}/detail/{}__struct.hpp", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_traits, ros_json, std::format("{}/detail/{}__traits.hpp", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_type_support, ros_json, std::format("{}/detail/{}__type_support.hpp", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl, ros_json, std::format("{}/{}.hpp", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

int main(int argc, char **argv) {
  GeneratorCpp generator(argc, argv);
  generator.run();
  return 0;
}