#include <rosidlcpp_typesupport_introspection_c/rosidlcpp_typesupport_introspection_c.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/format.h>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <ostream>
#include <string>
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

GeneratorTypesupportIntrospectionC::GeneratorTypesupportIntrospectionC(const rosidlcpp_core::GeneratorArguments& generator_arguments) : GeneratorBase(), m_arguments(generator_arguments) {
  set_input_path(m_arguments.template_dir + "/");
  set_output_path(m_arguments.output_dir + "/");

  GENERATOR_BASE_REGISTER_FUNCTION("get_includes", 1, get_includes);
}

void GeneratorTypesupportIntrospectionC::run() {
  // Load templates
  auto template_idl = parse_template("./idl__type_support.c.template");
  auto template_idl_rosidl = parse_template("./idl__rosidl_typesupport_introspection_c.h.template");

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
    std::filesystem::create_directories(m_arguments.output_dir + "/" + msg_directory + "/detail");
    write_template(template_idl, ros_json, std::format("{}/detail/{}__type_support.c", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
    write_template(template_idl_rosidl, ros_json, std::format("{}/detail/{}__rosidl_typesupport_introspection_c.h", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

int main(int argc, char** argv) {
  /**
   * CLI Arguments
   */
  argparse::ArgumentParser argument_parser("rosidlcpp_typesupport_introspection_c");
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
  GeneratorTypesupportIntrospectionC generator(generator_arguments);
  generator.run();
  return 0;
}
