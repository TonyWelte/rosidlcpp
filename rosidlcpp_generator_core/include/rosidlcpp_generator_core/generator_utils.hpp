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

#pragma once

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace rosidlcpp_core {

/**
 * Constants
 */

constexpr std::string_view SERVICE_EVENT_MESSAGE_SUFFIX = "_Event";
constexpr std::string_view SERVICE_REQUEST_MESSAGE_SUFFIX = "_Request";
constexpr std::string_view SERVICE_RESPONSE_MESSAGE_SUFFIX = "_Response";
constexpr std::string_view ACTION_GOAL_SUFFIX = "_Goal";
constexpr std::string_view ACTION_RESULT_SUFFIX = "_Result";
constexpr std::string_view ACTION_FEEDBACK_SUFFIX = "_Feedback";
constexpr std::string_view ACTION_GOAL_SERVICE_SUFFIX = "_SendGoal";
constexpr std::string_view ACTION_RESULT_SERVICE_SUFFIX = "_GetResult";
constexpr std::string_view ACTION_FEEDBACK_MESSAGE_SUFFIX = "_FeedbackMessage";

constexpr std::string_view EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME = "structure_needs_at_least_one_member";

/**
 * Debug tools
 */

void print_indented_json(const nlohmann::json& value);

/**
 * String tools
 */
auto escape_string(const std::string& str) -> std::string;
auto string_contains(const std::string& str, const std::string& substr) -> bool;
auto format_string(const std::string& format, const nlohmann::json& arg1) -> std::string;
auto format_string(const std::string& format, const nlohmann::json& arg1, const nlohmann::json& arg2) -> std::string;
auto replace_string(std::string str, const std::string& substr, const std::string& replacement) -> std::string;
auto camel_to_snake(const std::string& str) -> std::string;

/**
 * List tools
 */

auto span(const nlohmann::json& list, int start, int end) -> nlohmann::json;
auto push_back(nlohmann::json list, const nlohmann::json& value) -> nlohmann::json;
auto insert(nlohmann::json list, int index, const nlohmann::json& value) -> nlohmann::json;
auto custom_range(int start, int end, int step) -> std::vector<int>;

// TODO: Restrain to containers with concept
auto get_unique(auto container) {
  std::sort(container.begin(), container.end());
  container.erase(std::unique(container.begin(), container.end()), container.end());
  return container;
}

/**
 * Type checks
 */

auto is_sequence(const nlohmann::json& type) -> bool;
auto is_array(const nlohmann::json& type) -> bool;
auto is_bounded(const nlohmann::json& type) -> bool;
auto is_nestedtype(const nlohmann::json& type) -> bool;
auto is_string(const nlohmann::json& type) -> bool;
auto is_primitive(const nlohmann::json& type) -> bool;
auto is_namespaced(const nlohmann::json& type) -> bool;
auto is_float(const nlohmann::json& type) -> bool;
auto is_character(const nlohmann::json& type) -> bool;

auto is_integer(const nlohmann::json& type) -> bool;
auto is_unsigned_integer(const nlohmann::json& type) -> bool;
auto is_signed_integer(const nlohmann::json& type) -> bool;

auto is_action_type(const nlohmann::json& type) -> bool;
auto is_service_type(const nlohmann::json& type) -> bool;

/**
 * C API
 */

auto type_to_c_typename(const nlohmann::json& type) -> std::string;
auto basetype_to_c(const nlohmann::json& type) -> std::string;
auto idl_type_to_c(const nlohmann::json& type) -> std::string;
auto idl_structure_type_to_c_include_prefix(const nlohmann::json& type, const std::string& subdirectory = "") -> std::string;

/**
 * C++ API
 */

auto cpp_typename(const std::string& idl_typename) -> std::string;

/**
 * Key
 */

auto has_any_key(const nlohmann::json& message) -> bool;

}  // namespace rosidlcpp_core