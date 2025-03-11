#include <fmt/format.h>
#include <rosidlcpp_typesupport_c/rosidlcpp_typesupport_c.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <ostream>
#include <string>

// TODO: Move this to a common place
std::string idl_structure_type_to_c_typename(const nlohmann::json& type) {
  return fmt::format("{}__{}", fmt::join(type["namespaces"], "__"), type["name"].get<std::string>());
}

GeneratorTypesupportC::GeneratorTypesupportC(int argc, char** argv) : GeneratorBase() {
  // Arguments
  argparse::ArgumentParser argument_parser("rosidl_typesupport_cpp");
  argument_parser.add_argument("--typesupports")
      .required()
      .help("The list of typesupport implementations to generate");
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
  auto typesupport_implementations =
      argument_parser.get<std::string>("--typesupports");
  m_typesupport_implementations =
      rosidlcpp_parser::split_string(typesupport_implementations, ";");

  m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  m_env.set_input_path(m_arguments.template_dir + "/");
  m_env.set_output_path(m_arguments.output_dir + "/");

  m_env.add_callback("GET_DESCRIPTION_FUNC", 0, [](rosidlcpp_core::CallbackArgs& args) {
    return "get_type_description";
  });
  m_env.add_callback("GET_HASH_FUNC", 0, [](rosidlcpp_core::CallbackArgs& args) {
    return "get_type_hash";
  });
  m_env.add_callback("GET_INDIVIDUAL_SOURCE_FUNC", 0, [](rosidlcpp_core::CallbackArgs& args) {
    return "get_individual_type_description_source";
  });
  m_env.add_callback("GET_SOURCES_FUNC", 0, [](rosidlcpp_core::CallbackArgs& args) {
    return "get_type_description_sources";
  });
  m_env.add_callback("idl_structure_type_to_c_typename", 1, [](rosidlcpp_core::CallbackArgs& args) {
    return idl_structure_type_to_c_typename(*args.at(0));
  });
}

void GeneratorTypesupportC::run() {
  // Load templates
  inja::Template template_idl = m_env.parse_template("./idl__type_support.cpp.template");

  // Generate message specific files
  for (const auto& [path, file_path] : m_arguments.idl_tuples) {
    // std::cout << "Processing " << file_path << std::endl;

    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    ros_json["package_name"] = m_arguments.package_name;
    ros_json["type_supports"] = m_typesupport_implementations;

    const auto msg_directory = ros_json["interface_path"]["filedir"].get<std::string>();
    const auto msg_type = ros_json["interface_path"]["filename"].get<std::string>();

    std::filesystem::create_directories(m_arguments.output_dir + "/" + msg_directory);
    m_env.write(template_idl, ros_json,
                std::format("{}/{}__type_support.cpp", msg_directory,
                            rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

int main(int argc, char** argv) {
  GeneratorTypesupportC generator(argc, argv);
  generator.run();
  return 0;
}
