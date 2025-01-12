#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <argparse/argparse.hpp>
#include <generator_base.hpp>
#include <nlohmann/json_fwd.hpp>
#include <rosidlcpp_parser.hpp>

#include <fmt/ranges.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

class GeneratorTypeDescription : public rosidlcpp_core::GeneratorBase {
  public:
    GeneratorTypeDescription(int argc, char **argv);

    int run();

  private:
    rosidlcpp_core::GeneratorArguments m_arguments;
};

GeneratorTypeDescription::GeneratorTypeDescription(int argc, char **argv)
    : GeneratorBase() {
    // Arguments
    argparse::ArgumentParser argument_parser("rosidl_generator_type_description");
    argument_parser.add_argument("--generator-arguments-file")
        .required()
        .help("The location of the file containing the generator arguments");

    try {
        argument_parser.parse_args(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        std::cerr << argument_parser;
        std::exit(1); // TODO: Don't use exit in constructor
    }

    auto generator_arguments_file = argument_parser.get<std::string>("--generator-arguments-file");
    m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);
}

std::string to_type_name(const nlohmann::json &type) {
    assert(type.contains("namespaces") && "Type is missing namespaces");

    return fmt::format("{}/{}", fmt::join(type["namespaces"], "/"), type["name"].get<std::string>());
}

void add_message(const nlohmann::json &msg, nlohmann::json &type_description_json) {
    type_description_json[to_type_name(msg["type"])] = msg;
}

void add_service(const nlohmann::json &srv, nlohmann::json &type_description_json) {
    type_description_json[to_type_name(srv["type"])] = {
        {"type", srv["type"]},
        {"members",
         {
             {{"type", srv["request_message"]["type"]}, {"name", "request_message"}},
             {{"type", srv["response_message"]["type"]}, {"name", "response_message"}},
             {{"type", srv["event_message"]["type"]}, {"name", "event_message"}},
         }}};

    add_message(srv["request_message"], type_description_json);
    add_message(srv["response_message"], type_description_json);
    add_message(srv["event_message"], type_description_json);
}

void add_action(const nlohmann::json &action, nlohmann::json &type_description_json) {
    type_description_json[to_type_name(action["type"])] = {{"type", action["type"]},
                                                           {"members",
                                                            {
                                                                {{"type", action["goal"]["type"]}, {"name", "goal"}},
                                                                {{"type", action["result"]["type"]}, {"name", "result"}},
                                                                {{"type", action["feedback"]["type"]}, {"name", "feedback"}},
                                                                {{"type", action["send_goal_service"]["type"]}, {"name", "send_goal_service"}},
                                                                {{"type", action["get_result_service"]["type"]}, {"name", "get_result_service"}},
                                                                {{"type", action["feedback_message"]["type"]}, {"name", "feedback_message"}},
                                                            }}};

    add_message(action["goal"], type_description_json);
    add_message(action["result"], type_description_json);
    add_message(action["feedback"], type_description_json);
    add_service(action["send_goal_service"], type_description_json);
    add_service(action["get_result_service"], type_description_json);
    add_message(action["feedback_message"], type_description_json);
}

std::string field_type_type_name(const nlohmann::json &ftype) {
    static const std::unordered_map<std::string, std::string> FIELD_VALUE_TYPE_MAP = {
        {"nested_type", "FIELD_TYPE_NESTED_TYPE"},
        {"int8", "FIELD_TYPE_INT8"},
        {"uint8", "FIELD_TYPE_UINT8"},
        {"int16", "FIELD_TYPE_INT16"},
        {"uint16", "FIELD_TYPE_UINT16"},
        {"int32", "FIELD_TYPE_INT32"},
        {"uint32", "FIELD_TYPE_UINT32"},
        {"int64", "FIELD_TYPE_INT64"},
        {"uint64", "FIELD_TYPE_UINT64"},
        {"float", "FIELD_TYPE_FLOAT"},
        {"double", "FIELD_TYPE_DOUBLE"},
        {"long", "LONG_DOUBLE"},
        {"char", "FIELD_TYPE_CHAR"},
        {"wchar", "FIELD_TYPE_WCHAR"},
        {"boolean", "FIELD_TYPE_BOOLEAN"},
        {"octet", "FIELD_TYPE_BYTE"}};

    nlohmann::json value_type = ftype;
    std::string name_suffix;

    if (ftype.contains("value_type")) {
        value_type = ftype["value_type"];
        if (ftype["name"].get<std::string>() == "array") {
            name_suffix = "_ARRAY";
        } else if (ftype.contains("maximum_size")) {
            name_suffix = "_BOUNDED_SEQUENCE";
        } else {
            name_suffix = "_UNBOUNDED_SEQUENCE";
        }
    }

    std::string value_type_name;
    if (value_type["name"].get<std::string>() == "string") {
        value_type_name = value_type.contains("maximum_size") ? "FIELD_TYPE_BOUNDED_STRING" : "FIELD_TYPE_STRING";
    } else if (value_type["name"].get<std::string>() == "wstring") {
        value_type_name = value_type.contains("maximum_size") ? "FIELD_TYPE_BOUNDED_WSTRING" : "FIELD_TYPE_WSTRING";
    } else if (value_type.contains("namespaces")) {
        value_type_name = "FIELD_TYPE_NESTED_TYPE";
    } else if (rosidlcpp_core::is_primitive(value_type)) {
        value_type_name = FIELD_VALUE_TYPE_MAP.at(value_type["name"].get<std::string>());
    }

    return value_type_name + name_suffix;
}

int field_type_type_id(const nlohmann::json &ftype) {
    static const std::unordered_map<std::string, int> FIELD_TYPE_NAME_TO_ID = {
        {"FIELD_TYPE_NOT_SET", 0},
        {"FIELD_TYPE_NESTED_TYPE", 1},
        {"FIELD_TYPE_INT8", 2},
        {"FIELD_TYPE_UINT8", 3},
        {"FIELD_TYPE_INT16", 4},
        {"FIELD_TYPE_UINT16", 5},
        {"FIELD_TYPE_INT32", 6},
        {"FIELD_TYPE_UINT32", 7},
        {"FIELD_TYPE_INT64", 8},
        {"FIELD_TYPE_UINT64", 9},
        {"FIELD_TYPE_FLOAT", 10},
        {"FIELD_TYPE_DOUBLE", 11},
        {"FIELD_TYPE_LONG_DOUBLE", 12},
        {"FIELD_TYPE_CHAR", 13},
        {"FIELD_TYPE_WCHAR", 14},
        {"FIELD_TYPE_BOOLEAN", 15},
        {"FIELD_TYPE_BYTE", 16},
        {"FIELD_TYPE_STRING", 17},
        {"FIELD_TYPE_WSTRING", 18},
        {"FIELD_TYPE_FIXED_STRING", 19},
        {"FIELD_TYPE_FIXED_WSTRING", 20},
        {"FIELD_TYPE_BOUNDED_STRING", 21},
        {"FIELD_TYPE_BOUNDED_WSTRING", 22},
        {"FIELD_TYPE_NESTED_TYPE_ARRAY", 49},
        {"FIELD_TYPE_INT8_ARRAY", 50},
        {"FIELD_TYPE_UINT8_ARRAY", 51},
        {"FIELD_TYPE_INT16_ARRAY", 52},
        {"FIELD_TYPE_UINT16_ARRAY", 53},
        {"FIELD_TYPE_INT32_ARRAY", 54},
        {"FIELD_TYPE_UINT32_ARRAY", 55},
        {"FIELD_TYPE_INT64_ARRAY", 56},
        {"FIELD_TYPE_UINT64_ARRAY", 57},
        {"FIELD_TYPE_FLOAT_ARRAY", 58},
        {"FIELD_TYPE_DOUBLE_ARRAY", 59},
        {"FIELD_TYPE_LONG_DOUBLE_ARRAY", 60},
        {"FIELD_TYPE_CHAR_ARRAY", 61},
        {"FIELD_TYPE_WCHAR_ARRAY", 62},
        {"FIELD_TYPE_BOOLEAN_ARRAY", 63},
        {"FIELD_TYPE_BYTE_ARRAY", 64},
        {"FIELD_TYPE_STRING_ARRAY", 65},
        {"FIELD_TYPE_WSTRING_ARRAY", 66},
        {"FIELD_TYPE_FIXED_STRING_ARRAY", 67},
        {"FIELD_TYPE_FIXED_WSTRING_ARRAY", 68},
        {"FIELD_TYPE_BOUNDED_STRING_ARRAY", 69},
        {"FIELD_TYPE_BOUNDED_WSTRING_ARRAY", 70},
        {"FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE", 97},
        {"FIELD_TYPE_INT8_BOUNDED_SEQUENCE", 98},
        {"FIELD_TYPE_UINT8_BOUNDED_SEQUENCE", 99},
        {"FIELD_TYPE_INT16_BOUNDED_SEQUENCE", 100},
        {"FIELD_TYPE_UINT16_BOUNDED_SEQUENCE", 101},
        {"FIELD_TYPE_INT32_BOUNDED_SEQUENCE", 102},
        {"FIELD_TYPE_UINT32_BOUNDED_SEQUENCE", 103},
        {"FIELD_TYPE_INT64_BOUNDED_SEQUENCE", 104},
        {"FIELD_TYPE_UINT64_BOUNDED_SEQUENCE", 105},
        {"FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE", 106},
        {"FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE", 107},
        {"FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE", 108},
        {"FIELD_TYPE_CHAR_BOUNDED_SEQUENCE", 109},
        {"FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE", 110},
        {"FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE", 111},
        {"FIELD_TYPE_BYTE_BOUNDED_SEQUENCE", 112},
        {"FIELD_TYPE_STRING_BOUNDED_SEQUENCE", 113},
        {"FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE", 114},
        {"FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE", 115},
        {"FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE", 116},
        {"FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE", 117},
        {"FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE", 118},
        {"FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE", 145},
        {"FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE", 146},
        {"FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE", 147},
        {"FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE", 148},
        {"FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE", 149},
        {"FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE", 150},
        {"FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE", 151},
        {"FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE", 152},
        {"FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE", 153},
        {"FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE", 154},
        {"FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE", 155},
        {"FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE", 156},
        {"FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE", 157},
        {"FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE", 158},
        {"FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE", 159},
        {"FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE", 160},
        {"FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE", 161},
        {"FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE", 162},
        {"FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE", 163},
        {"FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE", 164},
        {"FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE", 165},
        {"FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE", 166},
    };

    return FIELD_TYPE_NAME_TO_ID.at(field_type_type_name(ftype));
}

int field_type_capacity(const nlohmann::json &ftype) {
    if (rosidlcpp_core::is_nestedtype(ftype) && ftype.contains("maximum_size")) {
        return ftype["maximum_size"].get<int>();
    }
    if (ftype.contains("size")) {
        return ftype["size"].get<int>();
    }

    return 0;
}

int field_type_string_capacity(const nlohmann::json &ftype) {
    nlohmann::json value_type = ftype.contains("value_type") ? ftype["value_type"] : ftype;

    if (rosidlcpp_core::is_string(value_type)) {
        if (value_type.contains("maximum_size")) {
            return value_type["maximum_size"].get<int>();
        }
    }

    return 0;
}

std::string field_type_nested_type_name(const nlohmann::json &ftype) {
    nlohmann::json value_type = ftype.contains("value_type") ? ftype["value_type"] : ftype;

    if (value_type.contains("namespaces")) {
        return fmt::format("{}/{}", fmt::join(value_type["namespaces"], "/"), value_type["name"].get<std::string>());
    }

    if (false /* Named type ? */) { // TODO: Figure out what named types are
    }

    return "";
}

nlohmann::json serialize_field_type(const nlohmann::json &type) {
    return {
        {"type_id", field_type_type_id(type)},
        {"capacity", field_type_capacity(type)},
        {"string_capacity", field_type_string_capacity(type)},
        {"nested_type_name", field_type_nested_type_name(type)},
    };
}

nlohmann::json serialize_field(const nlohmann::json &member) {
    return {
        {"name", member["name"].get<std::string>()},
        {"type", serialize_field_type(member["type"])},
        {"default_value", member.contains("default") ? member["default"].dump() : ""},
    };
}

nlohmann::json serialize_individual_type_description(const nlohmann::json &type, const nlohmann::json &members) {
    nlohmann::json result = {
        {"type_name", to_type_name(type)},
        {"fields", nlohmann::json::array()},
    };

    for (const auto &member : members) {
        result["fields"].push_back(serialize_field(member));
    }

    return result;
}

nlohmann::json extract_full_type_description(const std::string &output_type_name, const std::map<std::string, nlohmann::json> &type_map) {
    // Traverse reference graph to narrow down the references for the output type
    const auto &output_type = type_map.at(output_type_name);
    std::unordered_set<std::string> output_references;

    std::vector<std::string> process_queue;
    for (const auto &field : output_type["fields"]) {
        if (!field["type"]["nested_type_name"].get<std::string>().empty()) {
            process_queue.push_back(field["type"]["nested_type_name"].get<std::string>());
        }
    }

    while (!process_queue.empty()) {
        auto process_type = process_queue.back();
        process_queue.pop_back();
        if (output_references.find(process_type) == output_references.end()) {
            output_references.insert(process_type);
            for (const auto &field : type_map.at(process_type)["fields"]) {
                if (!field["type"]["nested_type_name"].get<std::string>().empty()) {
                    process_queue.push_back(field["type"]["nested_type_name"].get<std::string>());
                }
            }
        }
    }

    nlohmann::json referenced_type_descriptions = nlohmann::json::array();
    for (const auto &type_name : output_references) {
        referenced_type_descriptions.push_back(type_map.at(type_name));
    }

    return {
        {"type_description", output_type},
        {"referenced_type_descriptions", referenced_type_descriptions}};
}

int GeneratorTypeDescription::run() {
    std::unordered_map<std::string, std::string> include_map;

    for (const auto &[package_name, include_base_path] : m_arguments.include_paths) {
        include_map[package_name] = include_base_path;
    }

    nlohmann::json individual_types;
    // Generate message specific files
    for (const auto &[path, file_path] : m_arguments.idl_tuples) {
        std::cout << "Processing " << file_path << std::endl;

        const auto full_path = path + "/" + file_path;

        const auto msg_directory = std::filesystem::path(file_path).parent_path().string();
        const auto msg_type = std::filesystem::path(file_path).stem().string();

        const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
        // TODO: Save the result to an output file for debugging

        auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json);
        // TODO: Save the result to an output file for debugging

        ros_json["package_name"] = m_arguments.package_name;
        ros_json["interface_path"]["filepath"] = file_path;
        ros_json["interface_path"]["filename"] = msg_type;
        ros_json["interface_path"]["filedir"] = msg_directory;

        for (auto msg : ros_json.value("messages", nlohmann::json::array())) {
            add_message(msg["message"], individual_types);
        }
        for (auto srv : ros_json.value("services", nlohmann::json::array())) {
            add_service(srv, individual_types);
        }
        for (auto action : ros_json.value("actions", nlohmann::json::array())) {
            add_action(action, individual_types);
        }
    }

    std::unordered_set<std::string> pending_includes;
    for (const auto &[name, structure] : individual_types.items()) {
        for (const auto &member : structure["members"]) {
            nlohmann::json member_type;
            if (member["type"].contains("namespaces")) {
                member_type = member["type"];
            } else if (member["type"].contains("value_type") && member["type"]["value_type"].contains("namespaces")) {
                member_type = member["type"]["value_type"];
            } else {
                continue;
            }

            if (!individual_types.contains(to_type_name(member_type))) {
                pending_includes.insert(to_type_name(member_type)); // TODO: Fix
            }
        }
    }

    std::map<std::string, nlohmann::json> serialized_type_lookup;
    for (const auto &[key, value] : individual_types.items()) {
        serialized_type_lookup[key] = serialize_individual_type_description(value["type"], value["members"]);
    }

    // Hash lookup
    std::unordered_map<std::string, std::string> hash_lookup;
    while (!pending_includes.empty()) {
        auto process_include = *pending_includes.begin();
        pending_includes.erase(pending_includes.begin());

        std::filesystem::path p_path = process_include + ".json";
        std::string pkg = p_path.begin()->string();
        std::string pkg_dir = include_map[pkg];
        std::filesystem::path include_path = std::filesystem::path(pkg_dir) / std::filesystem::relative(p_path, pkg);

        std::ifstream include_file(include_path);
        if (!include_file.is_open()) {
            throw std::runtime_error("Could not open include file: " + include_path.string());
        }

        nlohmann::json include_json;
        include_file >> include_json;

        auto type_description_msg = include_json["type_description_msg"];
        try {
            for (const auto &val : include_json["type_hashes"]) {
                hash_lookup[val["type_name"].get<std::string>()] = val["hash_string"].get<std::string>();
            }
        } catch (const std::exception &e) {
            throw std::runtime_error("Key 'type_hashes' not found in " + include_path.string());
        }

        serialized_type_lookup[type_description_msg["type_description"]["type_name"].get<std::string>()] = type_description_msg["type_description"];
        for (const auto &referenced_type : type_description_msg["referenced_type_descriptions"]) {
            serialized_type_lookup[referenced_type["type_name"].get<std::string>()] = referenced_type;
        }
    }

    // Create fully-unrolled type description
    std::vector<nlohmann::json> type_description;
    for (const auto &[type_name, individual_type] : individual_types.items()) {
        nlohmann::json full_type_description = extract_full_type_description(type_name, serialized_type_lookup);

        nlohmann::json json_content = {
            {"type_description_msg", full_type_description},
            {"type_hashes", nlohmann::json::array()},
        };

        auto top_type_name = full_type_description["type_description"]["type_name"].get<std::string>();
        auto tmp = top_type_name.find_first_of('/');
        auto type_name2 = top_type_name.substr(tmp + 1);

        std::ofstream json_file{m_arguments.output_dir + "/" + type_name2 + ".json"};
        json_file << json_content.dump(2);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    GeneratorTypeDescription generator(argc, argv);
    return generator.run();
}