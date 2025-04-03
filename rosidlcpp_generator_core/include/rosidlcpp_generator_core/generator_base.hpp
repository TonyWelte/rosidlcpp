#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <inja/inja.hpp>

#include <thread_pool/thread_pool.h>

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

auto parse_arguments(const std::string& filepath) -> GeneratorArguments;

class GeneratorEnvironment : public inja::Environment {
 public:
  void set_input_path(std::string_view path) { input_path = path; }
  void set_output_path(std::string_view path) {
    m_output_directory = path;
    output_path = path;
  }

  void write_template(const Template& template_object, const nlohmann::json& data, std::string_view output_file, bool add_bom_if_needed);

 private:
  std::filesystem::path m_output_directory;
};

class GeneratorBase {
 public:
  GeneratorBase();

  void set_input_path(std::string_view path) { m_env.set_input_path(path); }
  void set_output_path(std::string_view path) { m_env.set_output_path(path); }

  void register_callback(std::string_view name, int arg_count, const FunctionType& function);
  void register_void_callback(std::string_view name, int arg_count, const VoidFunctionType& function);

  void write_template(const Template& template_object, const nlohmann::json& data, std::string_view output_file, bool add_bom_if_needed = true);

  auto parse_template(std::string_view template_path) -> Template;

  auto queue_task(auto task, auto... args) -> void {
    m_pool.enqueue_detach(std::move(task), args...);
  }

  auto queue_task_with_result(auto task, auto... args) -> std::future<decltype(task(args...))> {
    return m_pool.enqueue(std::move(task), args...);
  }

  auto wait_for_tasks() -> void;

 private:
  GeneratorEnvironment m_env;

  dp::thread_pool<> m_pool;

  std::map<std::thread::id, nlohmann::json> m_global_storage;
  std::mutex m_global_storage_mutex;
};

}  // namespace rosidlcpp_core