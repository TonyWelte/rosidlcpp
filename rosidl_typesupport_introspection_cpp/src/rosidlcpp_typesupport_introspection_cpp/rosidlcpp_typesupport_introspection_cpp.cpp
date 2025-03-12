#include <fmt/format.h>
#include <rosidlcpp_typesupport_introspection_cpp/rosidlcpp_typesupport_introspection_cpp.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/format.h>

#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <ostream>
#include <string>

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

// TODO: Move this to a common place with rosidl_generator_c
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
std::string idl_type_to_c(const nlohmann::json& type) {
  std::string c_type;
  if (rosidlcpp_core::is_array(type)) {
    std::runtime_error("The array size is part of the variable");
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

std::string idl_structure_type_to_c_include_prefix(const nlohmann::json& type, const std::string& subdirectory = "") {
  std::vector<std::string> parts;
  for (const auto& part : type["namespaces"]) {
    parts.push_back(rosidlcpp_core::camel_to_snake(part.get<std::string>()));
  }
  std::string include_prefix = fmt::format("{}/{}", fmt::join(parts, "/"), (subdirectory.empty() ? "" : subdirectory + "/") + rosidlcpp_core::camel_to_snake(type["name"]));

  // Strip service or action suffixes
  if (include_prefix.ends_with("__request")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 9);
  } else if (include_prefix.ends_with("__response")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 10);
  } else if (include_prefix.ends_with("__goal")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 6);
  } else if (include_prefix.ends_with("__result")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 8);
  } else if (include_prefix.ends_with("__feedback")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 10);
  } else if (include_prefix.ends_with("__send_goal")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 11);
  } else if (include_prefix.ends_with("__get_result")) {
    include_prefix = include_prefix.substr(0, include_prefix.size() - 12);
  }
  return include_prefix;
}

nlohmann::json get_includes(rosidlcpp_core::CallbackArgs& args) {
  const auto& message = *args.at(0);
  nlohmann::json includes_json = nlohmann::json::array();

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
      auto include_prefix_no_detail = idl_structure_type_to_c_include_prefix(type);
      append_header_to_members(header_to_members, include_prefix_no_detail + ".h", member["name"]);
      auto include_prefix = idl_structure_type_to_c_include_prefix(type, "detail");
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

GeneratorTypesupportIntrospectionCpp::GeneratorTypesupportIntrospectionCpp(int argc, char** argv) : GeneratorBase() {
  // Arguments
  argparse::ArgumentParser argument_parser("rosidl_typesupport_cpp");
  argument_parser.add_argument("--generator-arguments-file")
      .required()
      .help("The location of the file containing the generator arguments");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << error.what() << std::endl;
    std::cerr << argument_parser;
    std::exit(1);  // TODO: Don't use exit in constructor
  }

  auto generator_arguments_file =
      argument_parser.get<std::string>("--generator-arguments-file");

  m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  m_env.set_input_path(m_arguments.template_dir + "/");
  m_env.set_output_path(m_arguments.output_dir + "/");

  m_env.add_callback("get_includes", 1, [](rosidlcpp_core::CallbackArgs& args) {
    return get_includes(args);
  });
  m_env.add_callback("idl_type_to_c", 1, [](rosidlcpp_core::CallbackArgs& args) {
    return idl_type_to_c(*args.at(0));
  });
  m_env.add_callback("basetype_to_c", 1, [](rosidlcpp_core::CallbackArgs& args) {
    return basetype_to_c(*args.at(0));
  });
  m_env.add_callback("is_vector_bool", 1, [](rosidlcpp_core::CallbackArgs& args) {
    auto type = *args.at(0);
    return type["name"] == "sequence" && type["value_type"]["name"] == "boolean";
  });
  m_env.add_callback("MSG_TYPE_TO_CPP", 1, [](inja::Arguments& args) {
    const auto type = args.at(0)->get<std::string>();
    const auto v = MSG_TYPE_TO_CPP.find(type);
    if (v != MSG_TYPE_TO_CPP.end()) {
      return v->second;
    } else {
      return type;
    }
  });
}

void GeneratorTypesupportIntrospectionCpp::run() {
  // Load templates
  inja::Template template_idl = m_env.parse_template("./idl__type_support.cpp.template");
  inja::Template template_idl_rosidl = m_env.parse_template("./idl__rosidl_typesupport_introspection_cpp.hpp.template");

  // Generate message specific files
  for (const auto& [path, file_path] : m_arguments.idl_tuples) {
    // std::cout << "Processing " << file_path << std::endl;

    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    ros_json["package_name"] = m_arguments.package_name;

    const auto msg_directory = ros_json["interface_path"]["filedir"].get<std::string>();
    const auto msg_type = ros_json["interface_path"]["filename"].get<std::string>();
    std::filesystem::create_directories(m_arguments.output_dir + "/" + msg_directory + "/detail");
    m_env.write(template_idl, ros_json,
                std::format("{}/detail/{}__type_support.cpp", msg_directory,
                            rosidlcpp_core::camel_to_snake(msg_type)));
    m_env.write(template_idl_rosidl, ros_json,
                std::format("{}/detail/{}__rosidl_typesupport_introspection_cpp.hpp", msg_directory,
                            rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

int main(int argc, char** argv) {
  GeneratorTypesupportIntrospectionCpp generator(argc, argv);
  generator.run();
  return 0;
}
