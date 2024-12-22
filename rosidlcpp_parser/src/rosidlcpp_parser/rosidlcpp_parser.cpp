#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <iostream>
#include <system_error>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

constexpr std::string_view STRING_MODULE = "module";
constexpr std::string_view STRING_STRUCT = "struct";
constexpr std::string_view STRING_TYPEDEF = "typedef";
constexpr std::string_view STRING_CONST = "const";
constexpr std::string_view STRING_INCLUDE = "#include";

constexpr std::string_view VALID_NUMERIC = "1234567890-";
constexpr std::string_view VALID_TYPE_CHAR = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_:";
constexpr std::string_view VALID_NAME_CHAR = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_";
constexpr std::string_view WHITE_CHAR = " \t\n";

using json = nlohmann::json;

namespace rosidlcpp_parser {

auto remove_white_space(std::string_view &content_view) -> void {
    auto new_start = content_view.find_first_not_of(" \t\n");
    if (new_start != std::string_view::npos) {
        content_view.remove_prefix(new_start);
    } else {
        content_view.remove_prefix(content_view.size());
    }
}

auto remove_comment(std::string_view &content_view) -> void {
    if (content_view.substr(0, 2) == "//") {
        auto end_of_line = content_view.find_first_of('\n');
        if (end_of_line == std::string_view::npos) {
            content_view.remove_prefix(content_view.size());
        } else {
            content_view.remove_prefix(end_of_line + 1);
        }
    }
}

auto remove_white_space_and_comment(std::string_view &content_view) -> void {
    size_t old_size;
    do {
        old_size = content_view.size();
        remove_white_space(content_view);
        remove_comment(content_view);
    } while (old_size != content_view.size());
}

auto parse_name(std::string_view &content_view) -> std::string_view {
    auto end_of_name = content_view.find_first_not_of(VALID_NAME_CHAR);
    auto name = content_view.substr(0, end_of_name);

    content_view.remove_prefix(end_of_name);

    return name;
}

auto parse_type(std::string_view &content_view) -> std::string_view {
    auto end_of_type = content_view.find_first_not_of(VALID_TYPE_CHAR);
    if (end_of_type == std::string_view::npos) {
        throw std::runtime_error("Malformed type");
    }

    if (content_view.at(end_of_type) == '<') {
        end_of_type = content_view.find_first_of('>') + 1;
    }

    auto result = content_view.substr(0, end_of_type);
    content_view.remove_prefix(end_of_type);

    return result;
}

auto parse_value_list(std::string_view &content_view) -> json {
    assert(content_view[0] == '[' && "Value list should start with '['");

    content_view.remove_prefix(1); // Remove [
    remove_white_space_and_comment(content_view);

    json result = json::array();
    while (content_view[0] != ']') {
        result.push_back(parse_value(content_view));
        remove_white_space(content_view);

        if (content_view[0] == ',') {
            content_view.remove_prefix(1);
            remove_white_space(content_view);
        }
    }

    content_view.remove_prefix(1); // Remove ]

    return result;
}

auto parse_string_part(std::string_view &content_view, char delimiter) -> std::string_view {
    assert(content_view[0] == delimiter && "Value string should start with '\"' or '''");

    content_view.remove_prefix(1);
    auto string_limit = content_view.find_first_of(delimiter);
    if (string_limit == 0) {
        content_view.remove_prefix(1);
        return "";
    }

    while (content_view[string_limit - 1] == '\\') { // '"' is escaped
        string_limit += content_view.substr(string_limit + 1).find_first_of(delimiter) + 1;
    }

    auto result = content_view.substr(0, string_limit);
    content_view.remove_prefix(string_limit + 1);

    remove_white_space_and_comment(content_view);

    return result;
}

auto parse_string(std::string_view &content_view, char delimiter) -> std::string /* not string_view */ {
    std::string result;
    size_t old_size = content_view.size();
    while (content_view[0] == delimiter) {

        result += parse_string_part(content_view, delimiter);
        remove_white_space_and_comment(content_view);

        if (old_size == content_view.size()) {
            throw std::runtime_error("Failed to parse string");
        }
        old_size = content_view.size();
    }

    return result;
}

auto parse_numeric(std::string_view &content_view) -> json {
    // bool is_negative = content_view[0] == '-';

    // if (is_negative)
    //     content_view.remove_prefix(1);

    auto end_of_numeric = content_view.find_first_not_of(VALID_NUMERIC);

    if (content_view[end_of_numeric] == 'e' || content_view[end_of_numeric] == '.') { // is float
        double result;
        auto [ptr, ec] = std::from_chars(content_view.data(), content_view.data() + content_view.size(), result);
        if (ec != std::errc()) {
            throw std::runtime_error("Failed to parse floating point value");
        }
        content_view.remove_prefix(std::distance(content_view.data(), ptr));
        remove_white_space_and_comment(content_view);
        return result;
    } else { // assume is integer
        // long long result;
        // auto [ptr, ec] = std::from_chars(content_view.data(), content_view.data() + content_view.size(), result);
        // if (ec != std::errc()) {
        //     throw std::runtime_error("Failed to parse integer value");
        // }
        // content_view.remove_prefix(std::distance(content_view.data(), ptr));
        json result = content_view.substr(0, end_of_numeric);
        content_view.remove_prefix(end_of_numeric);
        return result;
    }
}

auto parse_value(std::string_view &content_view) -> json {
    if (content_view[0] == '[') {
        return parse_value_list(content_view);
    } else if (content_view[0] == '"' || content_view[0] == '\'') {
        return parse_string(content_view, content_view[0]);
    } else if (content_view.substr(0, 4) == "TRUE" || content_view.substr(0, 4) == "True") {  // Real value is TRUE but array string value is True
        content_view.remove_prefix(4);
        return true;
    } else if (content_view.substr(0, 5) == "FALSE" || content_view.substr(0, 5) == "False") {
        content_view.remove_prefix(5);
        return false;
    } else if (std::isdigit(content_view[0]) || content_view[0] == '-') {
        return parse_numeric(content_view);
    } else {
        throw std::runtime_error("Malformed value");
    }
}

auto parse_constant(std::string_view &content_view) -> json {
    assert(content_view.substr(0, STRING_CONST.size()) == STRING_CONST && "Bad constant parsing call");

    content_view.remove_prefix(STRING_CONST.size());
    remove_white_space_and_comment(content_view);

    json result;

    result["type"] = parse_type(content_view);
    remove_white_space_and_comment(content_view);

    result["name"] = parse_name(content_view);
    remove_white_space_and_comment(content_view);

    auto equal_sign_pos = content_view.find_first_of('=');
    content_view.remove_prefix(equal_sign_pos + 1);
    remove_white_space_and_comment(content_view);

    result["value"] = parse_value(content_view);
    remove_white_space_and_comment(content_view);

    if (content_view[0] != ';') {
        throw std::runtime_error("Failed to parse contant");
    }

    content_view.remove_prefix(1);
    remove_white_space_and_comment(content_view);

    return result;
}

auto parse_member(std::string_view &content_view) -> json {
    json result;

    result["type"] = parse_type(content_view);

    remove_white_space_and_comment(content_view);

    result["name"] = parse_name(content_view);

    remove_white_space_and_comment(content_view);

    // if (content_view[0] == '=') {
    //     content_view.remove_prefix(1);
    //     remove_white_space_and_comment(content_view);
    //     result["default"] = parse_value(content_view);
    // }

    remove_white_space_and_comment(content_view);

    assert(content_view[0] == ';' && "Malformed member definition!?");

    content_view.remove_prefix(1);

    return result;
}

auto parse_attribute(std::string_view &content_view) -> json {
    assert(content_view[0] == '@' && "Not an attribute!?");

    json result;

    content_view.remove_prefix(1);
    result["name"] = parse_name(content_view);

    remove_white_space_and_comment(content_view);

    assert(content_view[0] == '(' && "Not an attribute!?");
    content_view.remove_prefix(1);
    remove_white_space_and_comment(content_view);

    size_t old_size = content_view.size();
    while (content_view[0] != ')') {
        auto name = parse_name(content_view);
        remove_white_space_and_comment(content_view);
        content_view.remove_prefix(1); // Skip "=" sign
        remove_white_space_and_comment(content_view);
        auto value = parse_value(content_view);
        result["content"][name] = value;

        if (content_view[0] == ',') {
            content_view.remove_prefix(1);
            remove_white_space_and_comment(content_view);
        }

        if (old_size == content_view.size()) {
            throw std::runtime_error("Failed to parse attribute");
        }
        old_size = content_view.size();
    }

    content_view.remove_prefix(1); // Remove ")"

    remove_white_space_and_comment(content_view);

    return result;
}

auto parse_typedef(std::string_view &content_view) -> json {
    assert(content_view.substr(0, STRING_TYPEDEF.size()) == STRING_TYPEDEF && "Not a typedef!?");
    json result;

    content_view.remove_prefix(STRING_TYPEDEF.size() + 1);

    remove_white_space_and_comment(content_view);
    std::string type{parse_type(content_view)};

    remove_white_space_and_comment(content_view);
    auto name = parse_name(content_view);

    if (content_view[0] == '[') {
        auto end_of_array_definition = content_view.find_first_of(']') + 1;
        type += content_view.substr(0, end_of_array_definition);
        content_view.remove_prefix(end_of_array_definition);
    }

    result[name] = type;

    remove_white_space_and_comment(content_view);
    if (content_view.empty() || content_view.front() != ';') {
        throw std::runtime_error("Malformed typedef");
    }

    content_view.remove_prefix(1);
    remove_white_space_and_comment(content_view);

    return result;
}

auto parse_structure(std::string_view &content_view) -> json {
    assert(content_view.substr(0, STRING_STRUCT.size()) == STRING_STRUCT && "Not a struct!?");

    content_view.remove_prefix(STRING_STRUCT.size() + 1);

    remove_white_space_and_comment(content_view);
    auto name = parse_name(content_view);

    auto module_bloc_start = content_view.find_first_of('{');

    // Move to the first module element
    content_view.remove_prefix(module_bloc_start + 1);
    remove_white_space_and_comment(content_view);

    json module_json = json::object();
    module_json["name"] = name;

    nlohmann::json annotations;

    size_t old_size = content_view.size();
    while (content_view.front() != '}') {
        // Parse module content
        if (content_view.substr(0, STRING_TYPEDEF.size()) == STRING_TYPEDEF) {
            module_json["typedefs"].update(parse_typedef(content_view));
            annotations.clear();
        } else if (content_view[0] == '@') {
            auto annotation = parse_attribute(content_view);
            annotations[annotation["name"]].push_back(annotation["content"]);
        } else if (content_view.substr(0, STRING_CONST.size()) == STRING_CONST) {
            module_json["constants"].push_back(parse_constant(content_view));
            annotations.clear();
        } else {
            module_json["members"].push_back(parse_member(content_view));
            module_json["members"].back()["annotations"] = annotations;
            annotations.clear();
        }
        remove_white_space_and_comment(content_view);

        if (old_size == content_view.size()) {
            throw std::runtime_error("Malformed struct");
        }
        old_size = content_view.size();
    }

    content_view.remove_prefix(1); // Remove '}'
    remove_white_space_and_comment(content_view);
    content_view.remove_prefix(1); // Remove ';'
    remove_white_space_and_comment(content_view);

    return module_json;
}

auto parse_module(std::string_view &content_view) -> json {
    assert(content_view.substr(0, STRING_MODULE.size()) == STRING_MODULE && "Not a module!?");

    content_view.remove_prefix(STRING_MODULE.size() + 1);

    remove_white_space_and_comment(content_view);
    auto name = parse_name(content_view);

    auto module_bloc_start = content_view.find_first_of('{');

    // Move to the first module element
    content_view.remove_prefix(module_bloc_start + 1);
    remove_white_space_and_comment(content_view);

    json module_json = json::object();
    module_json["name"] = name;

    size_t old_size = content_view.size();
    while (content_view.front() != '}') {
        // Parse module content
        if (content_view.substr(0, STRING_MODULE.size()) == STRING_MODULE) {
            module_json["modules"].push_back(parse_module(content_view));
        } else if (content_view.substr(0, STRING_STRUCT.size()) == STRING_STRUCT) {
            module_json["structures"].push_back(parse_structure(content_view));
        } else if (content_view[0] == '@') {
            module_json["attributes"].push_back(parse_attribute(content_view));
        } else if (content_view.substr(0, STRING_CONST.size()) == STRING_CONST) {
            module_json["constants"].push_back(parse_constant(content_view));
        } else if (content_view.substr(0, STRING_TYPEDEF.size()) == STRING_TYPEDEF) {
            module_json["typedefs"].update(parse_typedef(content_view));
        }

        remove_white_space_and_comment(content_view);

        if (old_size == content_view.size()) {
            throw std::runtime_error("Failed to parse module");
        }
        old_size = content_view.size();
    }

    content_view.remove_prefix(1); // Remove '}'
    remove_white_space_and_comment(content_view);
    content_view.remove_prefix(1); // Remove ';'
    remove_white_space_and_comment(content_view);

    return module_json;
}

auto parse_include(std::string_view &content_view) -> std::string_view {
    assert(content_view.substr(0, STRING_INCLUDE.size()) == STRING_INCLUDE && "Not an include!?");

    content_view.remove_prefix(STRING_INCLUDE.size());
    remove_white_space(content_view);

    return parse_string_part(content_view, '"');
}

auto parse_idl_file(const std::string &filename) -> json {
    json result;

    std::ifstream file(filename);

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::string_view content_view(content);

    remove_white_space_and_comment(content_view);

    try {

        size_t old_size = content_view.size();
        while (!content_view.empty()) {
            if (content_view.substr(0, STRING_INCLUDE.size()) == STRING_INCLUDE) {
                result["includes"].push_back(parse_include(content_view));
            } else if (content_view.substr(0, STRING_MODULE.size()) == STRING_MODULE) {
                result["modules"].push_back(parse_module(content_view));
            }

            if (old_size == content_view.size()) {
                throw std::runtime_error("Failed to parse file");
            }
            old_size = content_view.size();
        }

    } catch (const std::runtime_error &error) {
        std::cerr << error.what() << std::endl;
        std::cerr << "Remaining unparsed content: \n"
                  << content_view << std::endl;
    }

    return result;
}

template <typename Container>
auto join(const Container &container, std::string_view sep) -> std::string {
    std::string result = container[0];
    for (auto it = std::next(container.begin()); it != container.end(); ++it) {
        result += sep;
        result += *it;
    }
    return result;
}

auto parse_ros_idl_file(const std::string &filename) -> json {
    auto idl_json_representation = parse_idl_file(filename);

    json result;
    result["messages"] = json::array();
    result["services"] = json::array();

    std::vector<std::string> namespaces;

    auto &current_node = idl_json_representation;
    // Process modules
    namespaces.push_back(current_node["modules"][0]["name"]);
    namespaces.push_back(current_node["modules"][0]["modules"][0]["name"]);
    current_node = current_node["modules"][0]["modules"][0];

    // Process structures
    if (current_node["structures"].size() == 1) { // Message
        result["messages"][0]["message"]["typedefs"] = current_node["typedefs"];
        result["messages"][0]["message"]["structure"] = current_node["structures"][0];
        if (current_node["modules"][0]["constants"].is_null()) {
            result["messages"][0]["message"]["constants"] = json::array();
        } else {
            result["messages"][0]["message"]["constants"] = current_node["modules"][0]["constants"];
        }
        result["messages"][0]["message"]["structure"]["namespaced_type"] = {{"name", current_node["structures"][0]["name"]},
                                                                            {"namespaces", namespaces},
                                                                            {"typename", join(namespaces, "::") + "::" + current_node["structures"][0]["name"].get<std::string>()}};
        result["messages"][0]["message_typename"] = result["messages"][0]["message"]["structure"]["namespaced_type"]["typename"];
    } else if (current_node["structures"].size() == 2) { // Service
        int request_structure_index = current_node["structures"][0]["name"].get<std::string>().ends_with("Request") ? 0 : 1;
        result["services"][0]["service"]["request_message"]["structure"] = current_node["structures"][request_structure_index];
        result["services"][0]["service"]["response_message"]["structure"] = current_node["structures"][1 - request_structure_index];
        result["services"][0]["service"]["request_message"]["constants"] = json::array();  // TODO
        result["services"][0]["service"]["response_message"]["constants"] = json::array(); // TODO
        result["services"][0]["service"]["request_message"]["structure"]["namespaced_type"] = {{"name", current_node["structures"][request_structure_index]["name"]},
                                                                                               {"namespaces", namespaces},
                                                                                               {"typename", join(namespaces, "::") + "::" + current_node["structures"][request_structure_index]["name"].get<std::string>()}};
        result["services"][0]["service"]["response_message"]["structure"]["namespaced_type"] = {{"name", current_node["structures"][1 - request_structure_index]["name"]},
                                                                                                {"namespaces", namespaces},
                                                                                                {"typename", join(namespaces, "::") + "::" + current_node["structures"][1 - request_structure_index]["name"].get<std::string>()}};
        auto service_typename = result["services"][0]["service"]["request_message"]["structure"]["namespaced_type"]["typename"].get<std::string>();
        service_typename = service_typename.substr(0, service_typename.size() - std::string_view("_Request").size());
        result["services"][0]["service_typename"] = service_typename;
        result["services"][0]["service"]["namespaced_type"]["namespaces"] = namespaces;
        auto service_name = current_node["structures"][request_structure_index]["name"].get<std::string>();
        service_name = service_name.substr(0, service_name.size() - std::string_view("_Request").size());
        result["services"][0]["service"]["namespaced_type"]["name"] = service_name;
        result["services"][0]["service"]["namespaced_type"]["namespaced_name"] = join(namespaces, "::") + "::" + service_name;

        // Add Event
        nlohmann::json event = nlohmann::json::object();
        event["constants"] = json::array();
        event["structure"] = json::object();
        event["structure"]["members"] = json::array();
        event["structure"]["members"].push_back(
            {{"annotations", json::object()},
             {"name", "info"},
             {"type", {{"namespaces", {"service_msgs", "msg"}}, {"typename", "service_msgs::msg::ServiceEventInfo"}}}});
        event["structure"]["members"].push_back(
            {{"annotations", json::object()}, {"name", "request"}, {"type", {{"namespaces", result["services"][0]["service"]["request_message"]["structure"]["namespaced_type"]["namespaces"]}, {"value_type", result["services"][0]["service_typename"].get<std::string>() + "_Request"}, {"maximum_size", "1"}, {"typename", "sequence<" + result["services"][0]["service_typename"].get<std::string>() + "_Request" + ", 1>"}}}});
        event["structure"]["members"].push_back(
            {{"annotations", json::object()}, {"name", "response"}, {"type", {{"namespaces", result["services"][0]["service"]["request_message"]["structure"]["namespaced_type"]["namespaces"]}, {"value_type", result["services"][0]["service_typename"].get<std::string>() + "_Response"}, {"maximum_size", "1"}, {"typename", "sequence<" + result["services"][0]["service_typename"].get<std::string>() + "_Response" + ", 1>"}}}});
        event["structure"]["namespaced_type"]["name"] = result["services"][0]["service"]["namespaced_type"]["name"].get<std::string>() + "_Event";
        event["structure"]["namespaced_type"]["namespaced_name"] = std::string("Test2") + "_Event";
        event["structure"]["namespaced_type"]["namespaces"] = result["services"][0]["service"]["request_message"]["structure"]["namespaced_type"]["namespaces"];
        event["structure"]["namespaced_type"]["typename"] = result["services"][0]["service_typename"].get<std::string>() + "_Event";

        result["services"][0]["service"]["event_message"] = event;

    } else if (current_node["structures"].size() == 3) { // Action
        // TODO
    }

    return result;
}

} // namespace rosidlcpp_parser