#pragma once

#include <vector>

#include <cstddef>

#include <nlohmann/json.hpp>

#include <inja/inja.hpp>

namespace rosidlcpp_core {

using CallbackArgs = std::vector<const nlohmann::json*>;

constexpr std::string_view SERVICE_EVENT_MESSAGE_SUFFIX = "_Event";
constexpr std::string_view SERVICE_REQUEST_MESSAGE_SUFFIX = "_Request";
constexpr std::string_view SERVICE_RESPONSE_MESSAGE_SUFFIX = "_Response";
constexpr std::string_view ACTION_GOAL_SUFFIX = "_Goal";
constexpr std::string_view ACTION_RESULT_SUFFIX = "_Result";
constexpr std::string_view ACTION_FEEDBACK_SUFFIX = "_Feedback";
constexpr std::string_view ACTION_GOAL_SERVICE_SUFFIX = "_SendGoal";
constexpr std::string_view ACTION_RESULT_SERVICE_SUFFIX = "_GetResult";
constexpr std::string_view ACTION_FEEDBACK_MESSAGE_SUFFIX = "_FeedbackMessage";

constexpr std::string_view EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME =
    "structure_needs_at_least_one_member";

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

std::string escape_string(const std::string& str);

GeneratorArguments parse_arguments(const std::string& filepath);

std::string camel_to_snake(const std::string& input);

class GeneratorEnvironment : public inja::Environment {
 public:
  void set_input_path(const std::string& directory) { input_path = directory; }
  void set_output_path(const std::string& directory) { output_path = directory; }
};

class GeneratorBase {
 public:
  GeneratorBase();
  virtual ~GeneratorBase() = default;

 protected:
  void register_callback(
      const std::string& name, std::size_t arg_count,
      std::function<nlohmann::json(std::vector<const nlohmann::json*>&)>
          callback);

 protected:
  GeneratorEnvironment m_env;

  nlohmann::json m_global_storage;
};

bool is_sequence(const nlohmann::json& type);
bool is_array(const nlohmann::json& type);
bool is_bounded(const nlohmann::json& type);
bool is_nestedtype(const nlohmann::json& type);
bool is_string(const nlohmann::json& type);
bool is_primitive(const nlohmann::json& type);
bool is_namespaced(const nlohmann::json& type);
bool is_float(const nlohmann::json& type);
bool is_character(const nlohmann::json& type);

bool is_unsigned_integer(const nlohmann::json& type);
bool is_signed_integer(const nlohmann::json& type);

}  // namespace rosidlcpp_core