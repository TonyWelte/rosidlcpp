#pragma once

#include <vector>

#include <cstddef>

#include <nlohmann/json.hpp>

namespace rosidlcpp_core {

constexpr std::string_view EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME =
    "structure_needs_at_least_one_member";

struct GeneratorArguments {
  std::string package_name;
  std::string output_dir;
  std::string template_dir;
  std::vector<std::pair<std::string, std::string>> idl_tuples;
  std::vector<std::string> ros_interface_dependencies;
  std::vector<std::pair<std::string, std::string>> target_dependencies;
  std::vector<std::pair<std::string, std::string>> type_description_tuples;
  std::vector<std::pair<std::string, std::string>> include_paths;
};

GeneratorArguments parse_arguments(const std::string& filepath);

std::string camel_to_snake(const std::string& input);

class GeneratorBase {
 public:
  virtual ~GeneratorBase() = default;

 protected:
  void register_callback(
      const std::string& name, std::size_t arg_count,
      std::function<nlohmann::json(std::vector<const nlohmann::json*>&)>
          callback);
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