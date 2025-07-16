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

#include <rosidlcpp_typesupport_cpp/rosidlcpp_typesupport_cpp.hpp>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_generator_core/generator_utils.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/format.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

GeneratorTypesupportCpp::GeneratorTypesupportCpp(rosidlcpp_core::GeneratorArguments generator_arguments, std::vector<std::string> typesupport_implementations_list) : GeneratorBase(), m_arguments(std::move(generator_arguments)), m_typesupport_implementations(std::move(typesupport_implementations_list)) {
  set_input_path(m_arguments.template_dir + "/");
  set_output_path(m_arguments.output_dir + "/");
}

void GeneratorTypesupportCpp::run() {
  // Load templates
  auto template_idl = parse_template("./idl__type_support.cpp.template");

  // Generate message specific files
  for (const auto& [path, file_path] : m_arguments.idl_tuples) {
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
    write_template(template_idl, ros_json, fmt::format("{}/{}__type_support.cpp", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
  }
}

auto main(int argc, char** argv) -> int {
  /**
   * CLI Arguments
   */
  argparse::ArgumentParser argument_parser("rosidlcpp_typesupport_cpp");
  argument_parser.add_argument("--generator-arguments-file").required().help("The location of the file containing the generator arguments");
  argument_parser.add_argument("--typesupport-impls").required().help("The list of typesupport implementations to generate");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    std::cerr << argument_parser;
    return 1;
  }

  auto generator_arguments_file = argument_parser.get<std::string>("--generator-arguments-file");
  auto generator_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  auto typesupport_implementations = argument_parser.get<std::string>("--typesupport-impls");
  auto typesupport_implementations_list = rosidlcpp_parser::split_string_view(typesupport_implementations, ";");

  /**
   * Generation
   */
  GeneratorTypesupportCpp generator(generator_arguments, typesupport_implementations_list);
  generator.run();
  return 0;
}
