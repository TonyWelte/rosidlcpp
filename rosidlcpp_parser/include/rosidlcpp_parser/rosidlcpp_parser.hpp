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

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace rosidlcpp_parser {

using TypedefMap = std::unordered_map<std::string, std::string>;

auto split_string_view(std::string_view value, std::string_view sep) -> std::vector<std::string>;
auto split_string(const std::string& value, const std::string& sep) -> std::vector<std::string>;

auto consume_white_space(std::string_view& content_view) -> void;
auto consume_comment(std::string_view& content_view) -> void;
auto consume_white_space_and_comment(std::string_view& content_view) -> void;

auto parse_include(std::string_view& content_view) -> std::string;
auto parse_string(std::string_view& content_view) -> std::string;
auto parse_string_part(std::string_view& content_view) -> std::string;
auto parse_string_python(std::string_view& content_view) -> std::string;
auto interpret_type(std::string_view content_view, TypedefMap typedefs = {}) -> nlohmann::json;
auto parse_type(std::string_view& content_view) -> std::string;
auto parse_name(std::string_view& content_view) -> std::string_view;
auto parse_value(std::string_view& content_view) -> nlohmann::json;
auto parse_constant(std::string_view& content_view, TypedefMap typedefs = {}) -> nlohmann::json;
auto parse_member(std::string_view& content_view, TypedefMap typedefs = {}) -> nlohmann::json;
auto parse_attribute(std::string_view& content_view) -> nlohmann::json;
auto parse_typedef(std::string_view& content_view) -> std::pair<std::string, std::string>;
auto parse_structure(std::string_view& content_view, TypedefMap typedefs = {}) -> nlohmann::json;
auto parse_module(std::string_view& content_view, TypedefMap tyepdefs = {}) -> nlohmann::json;

auto parse_default_list(std::string_view default_value) -> nlohmann::json;

auto has_non_ascii(const std::string& str) -> bool;

auto parse_idl_file(const std::string& filename) -> nlohmann::json;
auto parse_ros_idl_file(const std::string& filename) -> nlohmann::json;

auto convert_idljson_to_rosjson(const nlohmann::json& idl_json, std::string_view file_path) -> nlohmann::json;

}  // namespace rosidlcpp_parser