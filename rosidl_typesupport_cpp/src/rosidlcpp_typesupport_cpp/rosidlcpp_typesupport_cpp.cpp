#include <fmt/format.h>
#include <rosidlcpp_typesupport_cpp/rosidlcpp_typesupport_cpp.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <ostream>
#include <string>

GeneratorTypesupportCpp::GeneratorTypesupportCpp(int argc, char** argv) : GeneratorBase() {
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
      rosidlcpp_parser::split_string_view(typesupport_implementations, ";");

  m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  set_input_path(m_arguments.template_dir + "/");
  set_output_path(m_arguments.output_dir + "/");
}

void GeneratorTypesupportCpp::run() {
  // Load templates
  auto template_idl = parse_template("./idl__type_support.cpp.template");

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
    write_template(template_idl, ros_json,
                   std::format("{}/{}__type_support.cpp", msg_directory,
                               rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

int main(int argc, char** argv) {
  GeneratorTypesupportCpp generator(argc, argv);
  generator.run();
  return 0;
}
