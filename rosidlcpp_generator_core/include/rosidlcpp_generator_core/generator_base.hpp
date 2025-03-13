#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cstddef>

#include <nlohmann/json_fwd.hpp>

#include <inja/inja.hpp>

namespace rosidlcpp_core {

using Template = inja::Template;
using FunctionType = inja::CallbackFunction;
using VoidFunctionType = inja::VoidCallbackFunction;
using CallbackArgs = inja::Arguments;

struct GeneratorArguments {
  std::string package_name;
  std::string output_dir;
  std::string template_dir;
  std::vector<std::pair<std::string, std::string>> idl_tuples;
  std::vector<std::string> ros_interface_files;
  std::vector<std::string> ros_interface_dependencies;
  std::vector<std::pair<std::string, std::string>> target_dependencies;
  std::vector<std::pair<std::string, std::string>> type_description_tuples;
  std::vector<std::pair<std::string, std::string>> include_paths;
};

GeneratorArguments parse_arguments(const std::string& filepath);

class GeneratorEnvironment : public inja::Environment {
 public:
  void set_input_path(std::string_view path) { input_path = path; }
  void set_output_path(std::string_view path) {
    m_output_directory = path;
    output_path = path;
  }

  void write_template(const Template& template_object, const nlohmann::json& data, std::string_view output_file);

 private:
  std::filesystem::path m_output_directory;
};

#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_1 *args.at(0)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_2 *args.at(0), *args.at(1)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_3 *args.at(0), *args.at(1), *args.at(2)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_4 *args.at(0), *args.at(1), *args.at(2), *args.at(3)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_5 *args.at(0), *args.at(1), *args.at(2), *args.at(3), *args.at(4)

// Dispatcher Macro
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS(count) GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_##count

#define GENERATOR_BASE_REGISTER_VOID_FUNCTION(name, arg_count, function_name)         \
  register_void_callback(name, arg_count,                                             \
                         [](inja::Arguments& args) {                                  \
                           function_name(                                             \
                               GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS(arg_count)); \
                         })

#define GENERATOR_BASE_REGISTER_FUNCTION(name, arg_count, function_name)         \
  register_callback(name, arg_count,                                             \
                    [](inja::Arguments& args) {                                  \
                      return function_name(                                      \
                          GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS(arg_count)); \
                    })

#define GENERATOR_BASE_REGISTER_CONSTANT(name, value) \
  register_callback(name, 0,                          \
                    [](inja::Arguments&) {            \
                      return value;                   \
                    })

class GeneratorBase {
 public:
  GeneratorBase();
  virtual ~GeneratorBase() = default;

  void set_input_path(std::string_view path) { m_env.set_input_path(path); }
  void set_output_path(std::string_view path) { m_env.set_output_path(path); }

  void register_callback(std::string_view name, int arg_count, const FunctionType& function);
  void register_void_callback(std::string_view name, int arg_count, const VoidFunctionType& function);

  void write_template(const Template& template_object, const nlohmann::json& data, std::string_view output_file);

  Template parse_template(std::string_view template_path);

 private:
  GeneratorEnvironment m_env;

  nlohmann::json m_global_storage;
};

}  // namespace rosidlcpp_core