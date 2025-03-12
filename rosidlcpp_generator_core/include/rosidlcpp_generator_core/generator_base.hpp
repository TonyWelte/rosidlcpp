#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cstddef>

#include <nlohmann/json_fwd.hpp>

#include <inja/inja.hpp>

namespace rosidlcpp_core {

using nlohmann::json;

using CallbackArgs = std::vector<const nlohmann::json*>;

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
  void set_input_path(const std::string& directory) { input_path = directory; }
  void set_output_path(const std::string& directory) { output_path = directory; }
};

#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_1 *args.at(0)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_2 *args.at(0), *args.at(1)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_3 *args.at(0), *args.at(1), *args.at(2)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_4 *args.at(0), *args.at(1), *args.at(2), *args.at(3)
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_5 *args.at(0), *args.at(1), *args.at(2), *args.at(3), *args.at(4)

// Dispatcher Macro
#define GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS(count) GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS_##count

#define GENERATOR_BASE_REGISTER_VOID_FUNCTION(name, arg_count, function_name)          \
  m_env.add_void_callback(name, arg_count,                                             \
                          [](inja::Arguments& args) {                                  \
                            function_name(                                             \
                                GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS(arg_count)); \
                          })

#define GENERATOR_BASE_REGISTER_FUNCTION(name, arg_count, function_name)          \
  m_env.add_callback(name, arg_count,                                             \
                     [](inja::Arguments& args) {                                  \
                       return function_name(                                      \
                           GENERATOR_BASE_REGISTER_FUNCTION_GET_ARGS(arg_count)); \
                     })

class GeneratorBase {
 public:
  GeneratorBase();
  virtual ~GeneratorBase() = default;

  void register_function(std::string_view name, int arg_count, auto callback);
  void register_variable(std::string_view name, auto value);

 protected:
  GeneratorEnvironment m_env;

  nlohmann::json m_global_storage;
};

void GeneratorBase::register_function(std::string_view name, int arg_count, auto callback) {
  m_env.add_callback(std::string{name}, arg_count, callback);
}

void GeneratorBase::register_variable(std::string_view name, auto value) {
  m_env.add_callback(std::string{name}, 0, [value](CallbackArgs&) { return value; });
}

}  // namespace rosidlcpp_core