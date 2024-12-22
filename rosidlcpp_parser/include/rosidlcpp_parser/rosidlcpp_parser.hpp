#pragma once

#include <string_view>

#include <nlohmann/json.hpp>

namespace rosidlcpp_parser {

auto remove_white_space(std::string_view& content_view) -> void;
auto remove_comment(std::string_view& content_view) -> void;
auto remove_white_space_and_comment(std::string_view& content_view) -> void;


auto parse_include(std::string_view& content_view) -> std::string_view;
auto parse_string(std::string_view& content_view, char delimiter) -> std::string;
auto parse_string_part(std::string_view& content_view, char delimiter) -> std::string_view;
auto parse_name(std::string_view& content_view) -> std::string_view;
auto parse_value(std::string_view& content_view) -> nlohmann::json;
auto parse_constant(std::string_view& content_view) -> nlohmann::json;
auto parse_member(std::string_view& content_view) -> nlohmann::json;
auto parse_attribute(std::string_view& content_view) -> nlohmann::json;
auto parse_typedef(std::string_view& content_view) -> nlohmann::json;
auto parse_structure(std::string_view& content_view) -> nlohmann::json;
auto parse_module(std::string_view& content_view) -> nlohmann::json;

auto parse_idl_file(const std::string& filename) -> nlohmann::json;
auto parse_ros_idl_file(const std::string& filename) -> nlohmann::json;

}