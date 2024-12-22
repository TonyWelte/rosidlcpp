#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include <argparse/argparse.hpp>
#include <inja/inja.hpp>

#include <rosidlcpp_generator_cpp/rosidlcpp_parser.hpp>

#include <rosidlcpp_parser.hpp>

using json = nlohmann::json;

nlohmann::json value_array_from_string(std::string_view str) {
    assert(str[0] == '(' && "Array of value should start with '('");

    str.remove_prefix(1); // Remove (
    rosidlcpp_parser::remove_white_space_and_comment(str);

    json result = json::array();
    while (str[0] != ')') {
        result.push_back(rosidlcpp_parser::parse_value(str));
        rosidlcpp_parser::remove_white_space(str);

        if (str[0] == ',') {
            str.remove_prefix(1);
            rosidlcpp_parser::remove_white_space(str);
        }
    }

    return result;
}

nlohmann::json post_process(nlohmann::json input) {
    nlohmann::json output = input;

    // Message
    for (auto &message : output["messages"]) {
        for (auto &member : message["message"]["structure"]["members"]) {
            member["type"] = parse_idl_type(member["type"], message["message"]["typedefs"]);
        }
        for (auto &constant : message["message"]["constants"]) {
            constant["type"] = parse_idl_type(constant["type"], message["message"]["typedefs"]);
            if (constant["value"].is_array()) {
                for (auto &v : constant["value"]) {
                    v = v.dump();
                }
            } else {
                // constant["value"] = constant["value"].dump();
            }
        }
    }

    // Service
    for (auto &service : output["services"]) {
        for (auto &member : service["service"]["request_message"]["structure"]["members"]) {
            member["type"] = parse_idl_type(member["type"], service["service"]["request_message"]["typedefs"]);
        }
        for (auto &member : service["service"]["response_message"]["structure"]["members"]) {
            member["type"] = parse_idl_type(member["type"], service["service"]["response_message"]["typedefs"]);
        }
        for (auto &constant : service["service"]["request_message"]["constants"]) {
            constant["type"] = parse_idl_type(constant["type"], service["service"]["request_message"]["typedefs"]);
            if (constant["value"].is_array()) {
                for (auto &v : constant["value"]) {
                    v = v.dump();
                }
            } else {
                constant["value"] = constant["value"].dump();
            }
        }
        for (auto &constant : service["response_message"]["constants"]) {
            constant["type"] = parse_idl_type(constant["type"], service["response_message"]["typedefs"]);
            if (constant["value"].is_array()) {
                for (auto &v : constant["value"]) {
                    v = v.dump();
                }
            } else {
                constant["value"] = constant["value"].dump();
            }
        }
    }

    return output;
}

struct Arguments {
    std::string package_name;
    std::string output_dir;
    std::string template_dir;
    std::vector<std::pair<std::string, std::string>> idl_tuples;
    std::vector<std::string> ros_interface_dependencies;
    std::vector<std::pair<std::string, std::string>> target_dependencies;
    std::vector<std::pair<std::string, std::string>> type_description_tuples;
};

std::vector<std::pair<std::string, std::string>>
parse_pairs(const std::vector<std::string> list) {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &v : list) {
        std::istringstream ss(v);

        std::string first, second;
        std::getline(ss, first, ':');
        std::getline(ss, second, ':');
        result.push_back({first, second});
    }
    return result;
}

Arguments parse_arguments(const std::string &filepath) {
    std::ifstream f(filepath);
    json data = json::parse(f);

    Arguments result;

    result.package_name = data["package_name"];
    result.output_dir = data["output_dir"];
    result.template_dir = data["template_dir"];

    result.idl_tuples = parse_pairs(data["idl_tuples"]);
    result.ros_interface_dependencies = data["ros_interface_dependencies"];
    result.target_dependencies = parse_pairs(data["target_dependencies"]);
    result.type_description_tuples = parse_pairs(data["type_description_tuples"]);

    return result;
}

void print(const Arguments &a) {
    std::cout << "package_name: " << a.package_name << "\n";
    std::cout << "output_dir: " << a.output_dir << "\n";
    std::cout << "template_dir: " << a.template_dir << "\n";
    std::cout << "idl_tuple: " << a.idl_tuples.size() << "\n";
    std::cout << "ros_interface_dependencie: "
              << a.ros_interface_dependencies.size() << "\n";
    std::cout << "target_dependencie: " << a.target_dependencies.size() << "\n";
    std::cout << "type_description_tuple: " << a.type_description_tuples.size()
              << "\n";
}

// Function to join namespaces
std::string joinNamespaces(const std::vector<std::string> &namespaces,
                           const std::string &delimiter = "::") {
    std::ostringstream joined;
    for (size_t i = 0; i < namespaces.size(); ++i) {
        joined << namespaces[i];
        if (i != namespaces.size() - 1) {
            joined << delimiter;
        }
    }
    return joined.str();
}

std::string camel_to_snake(const std::string &input) {
    std::string result;
    bool wasPrevUppercase = false; // Track if previous character was uppercase

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        bool isCurrUppercase =
            isupper(c); // Check if the current character is uppercase

        if (isCurrUppercase) {
            // Add an underscore if the previous character was not uppercase OR
            // if the next character is lowercase (to handle transitions like
            // RGBAColor → rgba_color)
            if (i > 0 && (!wasPrevUppercase ||
                          (i + 1 < input.length() && islower(input[i + 1])))) {
                result += '_';
            }
            result += tolower(c); // Convert current uppercase letter to lowercase
        } else {
            result += c; // Append lowercase letter as-is
        }

        wasPrevUppercase = isCurrUppercase; // Update the tracking flag
    }

    return result;
}

// Member class definition
class Member {
  public:
    std::string name;
    nlohmann::json default_value;
    nlohmann::json zero_value;
    bool zero_need_array_override;
    nlohmann::json type;
    int num_prealloc;

    Member(const std::string &name)
        : name(name), zero_need_array_override(false), num_prealloc(0) {}

    // Compare if the default and zero values are the same for two members
    bool same_default_and_zero_value(const Member &other) const {
        return default_value == other.default_value &&
               zero_value == other.zero_value;
    }

    // Convert Member to JSON
    nlohmann::json to_json() const {
        return {{"name", name},
                {"default_value", default_value},
                {"zero_value", zero_value},
                {"zero_need_array_override", zero_need_array_override},
                {"type", type},
                {"num_prealloc", num_prealloc}};
    }
};

// CommonMemberSet class definition
class CommonMemberSet {
  public:
    std::vector<Member> members;

    bool add_member(const Member &member) {
        if (members.empty() || members.back().same_default_and_zero_value(member)) {
            members.push_back(member);
            return true;
        }
        return false;
    }

    // Convert CommonMemberSet to JSON
    nlohmann::json to_json() const {
        nlohmann::json members_json = nlohmann::json::array();
        for (const auto &member : members) {
            members_json.push_back(member.to_json());
        }
        return members_json;
    }
};

// Floating point types for checking
const std::vector<std::string> FLOATING_POINT_TYPES = {"float", "double",
                                                       "long double"};

nlohmann::json default_value_from_type(const std::string &type) {
    if (type == "string" || type == "wstring") {
        return ""; // Empty string for generic string types
    } else if (std::find(FLOATING_POINT_TYPES.begin(), FLOATING_POINT_TYPES.end(),
                         type) != FLOATING_POINT_TYPES.end()) {
        return 0.0; // Default for floating point types
    } else if (type == "boolean") {
        return false; // Default for boolean types
    }
    return 0; // Default for other types (integers)
}

std::string primitive_value_to_cpp(const std::string &type,
                                   const nlohmann::json &value) {
    if (type == "string") {
        return "\"" + value.get<std::string>() + "\"";
    } else if (type == "wstring") {
        return "u\"" + value.get<std::string>() + "\"";
    } else if (type == "boolean") {
        return value.get<bool>() ? "true" : "false";
    } else if (type == "short" || type == "unsigned short" || type == "char" ||
               type == "wchar" || type == "octet" || type == "int8" ||
               type == "uint8" || type == "int16" || type == "uint16") {
        return value.is_string() ? value.get<std::string>()
                                 : std::to_string(value.get<int>());
    } else if (type == "double" || type == "long double") {
        return value.is_string() ? value.get<std::string>() : value.dump();
    } else if (type == "int32") {
        // if (value.get<int>() == std::numeric_limits<int>::min()) {
        //   return "(" + std::to_string(value.get<int>() + 1) + "l - 1)";
        // }
        return (value.is_string() ? value.get<std::string>()
                                  : std::to_string(value.get<int32_t>())) +
               "l";
    } else if (type == "uint32") {
        return (value.is_string() ? value.get<std::string>()
                                  : std::to_string(value.get<int32_t>())) +
               "ul";
    } else if (type == "int64") {
        // if (value.get<int64_t>() == std::numeric_limits<long int>::min()) {
        //   return "(" + std::to_string(value.get<int64_t>() + 1) + "ll - 1)";
        // }
        return (value.is_string() ? value.get<std::string>()
                                  : std::to_string(value.get<int32_t>())) +
               "ll";
    } else if (type == "uint64") {
        return (value.is_string() ? value.get<std::string>()
                                  : std::to_string(value.get<int32_t>())) +
               "ull";
    } else if (type == "float") {
        return value.dump() + "f";
    }

    throw std::invalid_argument("unknown primitive type: " + type);
}

std::string value_to_cpp(const std::string &type, const nlohmann::json &value) {
    return "";
    // Assume that we are working with arrays
    std::vector<std::string> cpp_values;

    // For simplicity, we'll consider strings as arrays of characters
    bool is_string_array = (type == "string");

    for (const auto &single_value : value) {
        std::string cpp_value = primitive_value_to_cpp(type, single_value);
        if (is_string_array) {
            cpp_values.push_back("{" + cpp_value + "}");
        } else {
            cpp_values.push_back(cpp_value);
        }
    }

    std::string cpp_value =
        "{" + fmt::format("{}", fmt::join(cpp_values, ", ")) + "}";

    // Wrap in an extra set of braces if needed to avoid scalar initializer;
    // warnings
    if (cpp_values.size() > 1 && !is_string_array) {
        cpp_value = "{" + cpp_value + "}";
    }

    return cpp_value;
}

// Main function to create initialization lists
nlohmann::json
create_init_alloc_and_member_lists(const nlohmann::json &message) {
    std::vector<std::string> init_list;
    std::vector<std::string> alloc_list;
    std::vector<CommonMemberSet> member_list;

    for (const auto &field : message["structure"]["members"]) {
        Member member(field["name"]);
        member.type = field["type"];

        if (is_array(field["type"]["typename"])) {
            alloc_list.push_back(field["name"].get<std::string>() + "(_alloc)");

            if (is_primitive(field["type"]["value_type"]) ||
                is_string(field["type"]["value_type"])) {
                auto default_value = default_value_from_type(
                    field["type"]["value_type"].get<std::string>());
                std::string single_value =
                    primitive_value_to_cpp(field["type"]["value_type"], default_value);
                member.zero_value = {};
                for (int i = 0; i < field["type"]["size"].get<int>(); ++i) {
                    member.zero_value.push_back(single_value);
                }

                if (field.contains("annotations") &&
                    field["annotations"].contains("default")) {
                    auto default_val = value_array_from_string(field["annotations"]["default"][0]["value"].get<std::string>());
                    for (const auto &v : default_val) {
                        member.default_value.push_back(primitive_value_to_cpp(field["type"]["value_type"], v));
                    }
                }
            } else {
                member.zero_value = nlohmann::json::array(); // Empty initializer for
                                                             // non-primitive types
                member.zero_need_array_override = true;
            }
        } else if (is_sequence(field["type"]["typename"])) {
            if (field.contains("annotations") &&
                field["annotations"].contains("default")) {
                auto default_val = value_array_from_string(field["annotations"]["default"][0]["value"].get<std::string>());
                member.default_value =
                    value_to_cpp(field["type"]["typename"], default_val);
                member.num_prealloc = default_val.size();
            }
        } else {
            if (is_primitive(field["type"]["typename"])) {
                if (is_string(field["type"]["typename"])) {
                    alloc_list.push_back(field["name"].get<std::string>() + "(_alloc)");
                }

                auto default_value = default_value_from_type(
                    field["type"]["typename"].get<std::string>());
                member.zero_value =
                    primitive_value_to_cpp(field["type"]["typename"], default_value);

                std::cout << default_value.dump() << std::endl;
                std::cout << member.zero_value.dump() << std::endl;

                if (field.contains("annotations") &&
                    field["annotations"].contains("default")) {
                    member.default_value =
                        primitive_value_to_cpp(field["type"]["typename"],
                                               field["annotations"]["default"][0]["value"]);
                }
            } else {
                init_list.push_back(field["name"].get<std::string>() + "(_init)");
                alloc_list.push_back(field["name"].get<std::string>() +
                                     "(_alloc, _init)");
            }
        }

        if ((field.contains("annotations") &&
             field["annotations"].contains("default")) ||
            (!member.zero_value.is_null())) {
            if (member_list.empty() || !member_list.back().add_member(member)) {
                CommonMemberSet commonset;
                commonset.add_member(member);
                member_list.push_back(commonset);
            }
        }
    }

    // Convert the results to JSON format
    nlohmann::json output_json;
    output_json["init_list"] = init_list;
    output_json["alloc_list"] = alloc_list;
    output_json["default_value_members"] = false;
    output_json["zero_value_members"] = false;
    output_json["non_defaulted_zero_initialized_members"] = false;

    nlohmann::json member_list_json = nlohmann::json::array();
    for (const auto &commonset : member_list) {
        member_list_json.push_back(commonset.to_json());

        if (!commonset.members.front().default_value.empty()) {
            output_json["default_value_members"] = true;
        }
        if (!commonset.members.front().zero_value.is_null()) {
            output_json["zero_value_members"] = true;
        }
        if ((!commonset.members.front().zero_value.is_null() ||
             commonset.members.front().zero_need_array_override) &&
            commonset.members.front().default_value.empty()) {
            output_json["non_defaulted_zero_initialized_members"] = true;
        }
    }
    output_json["member_list"] = member_list_json;

    //   std::cout << output_json.dump();

    return output_json;
}

const std::unordered_map<std::string, std::string> MSG_TYPE_TO_CPP = {
    {"boolean", "bool"},
    {"octet", "unsigned char"}, // TODO change to std::byte with C++17
    {"char", "unsigned char"},
    {"wchar", "char16_t"},
    {"float", "float"},
    {"double", "double"},
    {"long double", "long double"},
    {"uint8", "uint8_t"},
    {"int8", "int8_t"},
    {"uint16", "uint16_t"},
    {"int16", "int16_t"},
    {"uint32", "uint32_t"},
    {"int32", "int32_t"},
    {"uint64", "uint64_t"},
    {"int64", "int64_t"},
    {"string", "std::basic_string<char, std::char_traits<char>, typename "
               "std::allocator_traits<ContainerAllocator>::template "
               "rebind_alloc<char>>"},
    {"wstring",
     "std::basic_string<char16_t, std::char_traits<char16_t>, typename "
     "std::allocator_traits<ContainerAllocator>::template "
     "rebind_alloc<char16_t>>"},
};

std::string msg_type_only_to_cpp(const nlohmann::json &type) {
    std::string main_type;
    std::string cpp_type;

    // Handle nested types
    if (type.contains("value_type")) {
        main_type = type["value_type"];
    } else {
        main_type = type["typename"];
    }

    // Check the basic type
    if (is_primitive(main_type)) {
        cpp_type = MSG_TYPE_TO_CPP.at(main_type);
    } else if (is_namespaced_type(main_type)) {
        cpp_type = main_type + "_<ContainerAllocator>";
    } else {
        throw std::invalid_argument("Unknown type encountered: " +
                                    type["typename"].get<std::string>());
    }

    return cpp_type;
}

std::string msg_type_to_cpp(const nlohmann::json &type) {
    std::string cpp_type = msg_type_only_to_cpp(type);

    // Handle nested types
    if (is_nestedtype(type["typename"])) {
        if (is_sequence(type["typename"]) &&
            !type.contains("maximum_size")) { // Unbounded sequence
            return "std::vector<" + cpp_type +
                   ", typename std::allocator_traits<ContainerAllocator>::template "
                   "rebind_alloc<" +
                   cpp_type + ">>";
        } else if (is_sequence(type["typename"])) { // Bounded sequence
            return "rosidl_runtime_cpp::BoundedVector<" + cpp_type + ", " +
                   type["maximum_size"].get<std::string>() +
                   ", typename std::allocator_traits<ContainerAllocator>::template "
                   "rebind_alloc<" +
                   cpp_type + ">>";
        } else if (is_array(type["typename"])) {
            unsigned int array_size = type["size"];
            return "std::array<" + cpp_type + ", " + std::to_string(array_size) + ">";
        } else {
            throw std::invalid_argument("Unknown nested type encountered: " +
                                        type["nested_type"].get<std::string>());
        }
    }

    return cpp_type; // Return simple types as-is
}

bool ends_with(const std::string &value, const std::string &ending) {
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string strip_end_until_char(const std::string &value, char limit) {
    return value.substr(0, value.find_last_of(limit));
    auto it = value.rbegin();
    while (it != value.rend()) {
        if (*it == limit) {
            break;
        }

        ++it;
    }

    std::string tmp(it, value.rend()); // TODO: Find less hacky way to do that

    return std::string(tmp.rbegin(), tmp.rend());
}

int main(int argc, char *argv[]) {
    // Arguments
    argparse::ArgumentParser argument_parser("rosidl_generator_cpp");
    argument_parser.add_argument("--generator-arguments-file")
        .required()
        .help("The location of the file containing the generator arguments");

    try {
        argument_parser.parse_args(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        std::cerr << argument_parser;
        std::exit(1);
    }

    auto generator_arguments_file =
        argument_parser.get<std::string>("--generator-arguments-file");

    auto arguments_content = parse_arguments(generator_arguments_file);
    // Setup templates
    inja::Environment env{arguments_content.template_dir + "/"};

    env.add_callback("convert_camel_case_to_lower_case_underscore", 1,
                     [](inja::Arguments &args) {
                         return camel_to_snake(args.at(0)->get<std::string>());
                     });

    env.add_callback("custom_range", 3, [](inja::Arguments &args) {
        const int start = args.at(0)->get<int>();
        const int end = args.at(1)->get<int>();
        const int step = args.at(2)->get<int>();
        std::vector<int> result;
        int v = start;
        while (step > 0 ? v < end : v > end) {
            result.push_back(v);
            v += step;
        }
        return result;
    });
    env.add_callback("push_back", 2, [](inja::Arguments &args) {
        auto list = *args.at(0);
        list.push_back(args.at(1)->get<std::string>());
        // std::cout << list << std::endl;
        return list;
    });

    env.add_callback("MSG_TYPE_TO_CPP", 1, [](inja::Arguments &args) {
        const auto v = MSG_TYPE_TO_CPP.find(args.at(0)->get<std::string>());
        if (v != MSG_TYPE_TO_CPP.end()) {
            return v->second;
        } else {
            return args.at(0)->get<std::string>();
        }
    });

    env.add_callback("msg_type_to_cpp", 1, [](inja::Arguments &args) {
        return msg_type_to_cpp(*args.at(0));
    });

    env.add_callback("is_type_integer", 1, [](inja::Arguments &args) {
        const std::set<std::string> INTEGER_TYPES = {"int8", "int16", "int32",
                                                     "int64", "uint8", "uint16",
                                                     "uint32", "uint64"};
        return INTEGER_TYPES.find(args.at(0)->get<std::string>()) !=
               INTEGER_TYPES.end();
    });
    env.add_callback("is_primitive", 1, [](inja::Arguments &args) {
        return is_primitive((*args.at(0)));
    });
    env.add_callback("is_type_string", 1, [](inja::Arguments &args) {
        return is_string((*args.at(0)));
    });
    env.add_callback("is_type_namespaced", 1, [](inja::Arguments &args) {
        return is_namespaced_type((*args.at(0)));
    });
    env.add_callback("is_type_nested", 1, [](inja::Arguments &args) {
        return is_nestedtype((*args.at(0)));
    });
    env.add_callback("is_type_unsigned", 1, [](inja::Arguments &args) {
        const std::set<std::string> UNSIGNED_TYPES = {"uint8", "uint16", "uint32",
                                                      "uint64"};
        return UNSIGNED_TYPES.find(args.at(0)->get<std::string>()) !=
               UNSIGNED_TYPES.end();
    });
    env.add_callback("create_init_alloc_and_member_lists", 1,
                     [](inja::Arguments &args) {
                         return create_init_alloc_and_member_lists(*args.at(0));
                     });
    env.add_callback("generate_zero_string", 2, [](inja::Arguments &args) {
        const auto membset = (*args.at(0));
        const std::string fill_args = *args.at(1);

        nlohmann::json result = nlohmann::json::array();
        for (const auto &member : membset) {
            if (member["zero_value"].is_null()) {
                // TODO
                continue;
            }
            if (member["zero_value"].is_array()) {
                if (member["num_prealloc"] > 0) {
                    result.push_back(
                        "this->" + member["name"].get<std::string>() + ".resize(" +
                        std::to_string(member["num_prealloc"].get<int>()) + ");");
                }
                if (member["zero_need_array_override"]) {
                    result.push_back("this->" + member["name"].get<std::string>() +
                                     ".fill(" + msg_type_only_to_cpp(member["type"]) +
                                     "{" + fill_args + "});");

                } else {
                    result.push_back(
                        "std::fill<typename " + msg_type_to_cpp(member["type"]) +
                        "::iterator, " + msg_type_only_to_cpp(member["type"]) +
                        ">(this->" + member["name"].get<std::string>() +
                        ".begin(), this->" + member["name"].get<std::string>() +
                        ".end(), " + member["zero_value"][0].get<std::string>() + ");");
                }
            } else {
                result.push_back("this->" + member["name"].get<std::string>() + " = " +
                                 member["zero_value"].get<std::string>() + ";");
            }
        }
        return result;
    });
    env.add_callback("generate_default_string", 2, [](inja::Arguments &args) {
        const auto membset = (*args.at(0));
        const std::string fill_args = *args.at(1);

        nlohmann::json result = nlohmann::json::array();
        for (const auto &member : membset) {
            if (!member.contains("default_value") || member["default_value"].is_null()) {
                continue;
            }
            if (member["num_prealloc"] > 0) {
                result.push_back(
                    "this->" + member["name"].get<std::string>() + ".resize(" +
                    std::to_string(member["num_prealloc"].get<int>()) + ");");
            }
            if (member["default_value"].is_array()) {
                if (std::all_of(member["default_value"].begin(),
                                member["default_value"].end(),
                                [first = member["default_value"]](const auto &value) {
                                    return value == first;
                                })) {
                    result.push_back(
                        "this->" + member["name"].get<std::string>() + ".fill(" +
                        msg_type_only_to_cpp(member["type"]["typename"]) + "{" +
                        member["default_value"][0].get<std::string>() + "});");
                } else {
                    for (size_t i = 0; i < member["default_value"].size(); ++i) {
                        result.push_back("this->" + member["name"].get<std::string>() +
                                         "[" + std::to_string(i) + "] = " +
                                         member["default_value"][i].get<std::string>() + ";");
                    }
                }
            } else {
                result.push_back("this->" + member["name"].get<std::string>() + " = " +
                                 member["default_value"].get<std::string>() + ";");
            }
        }
        return result;
    });
    env.add_callback("get_fixed_template_strings", 1,
                     [](const inja::Arguments &args) -> nlohmann::json {
                         const auto members = *args.at(0);

                         std::set<std::string> fixed_template_strings;
                         for (const auto &member : members) {
                             std::string type_name = member["type"]["typename"];
                             if (is_sequence(type_name)) {
                                 return {"false"};
                             }
                             if (is_array(type_name)) {
                                 type_name = member["type"]["value_type"];
                             }
                             if (is_string(type_name)) {
                                 return {"false"};
                             }
                             if (is_namespaced_type(type_name)) {
                                 fixed_template_strings.insert("has_fixed_size<" +
                                                               type_name + ">::value");
                             }
                         }
                         if (fixed_template_strings.empty()) {
                             return {"true"};
                         } else {
                             return fixed_template_strings;
                         }
                     });
    env.add_callback(
        "get_bounded_template_strings", 1,
        [](const inja::Arguments &args) -> nlohmann::json {
            const auto members = *args.at(0);

            std::set<std::string> bounded_template_strings;
            for (const auto &member : members) {
                std::string type_name = member["type"]["typename"];
                if (is_sequence(type_name) &&
                    !member["type"].contains("maximum_size")) {
                    return {"false"};
                }
                if (is_nestedtype(type_name) /* bounded sequence or array */) {
                    type_name = member["type"]["value_type"];
                }
                if (is_string(type_name)) {
                    return {"false"};
                }
                if (is_namespaced_type(type_name)) {
                    bounded_template_strings.insert("has_bounded_size<" + type_name +
                                                    ">::value");
                }
            }
            if (bounded_template_strings.empty()) {
                return {"true"};
            } else {
                return bounded_template_strings;
            }
        });
    env.add_callback("get_includes", 2, [](inja::Arguments &args) {
        std::vector<std::pair<std::string, std::vector<std::string>>> includes;

        const auto message = *args.at(0);
        const std::string suffix = *args.at(1);
        for (const auto &member : message["structure"]["members"]) {
            auto type = member["type"]["typename"].get<std::string>();
            if (is_nestedtype(type)) {
                type = member["type"]["value_type"].get<std::string>();
            }
            if (is_namespaced_type(type)) {
                // std::cout << "namespaced " << type << std::endl;
                std::string type_name;
                if ((message["structure"]["namespaced_type"]["namespaces"].back() ==
                         "action" ||
                     message["structure"]["namespaced_type"]["namespaces"].back() ==
                         "srv") &&
                    (ends_with(type, "_Request") || ends_with(type, "_Response"))) {
                    type_name = strip_end_until_char(type, '_');
                    std::string current_struct_type = "";
                    for (const auto &ns :
                         message["structure"]["namespaced_type"]["namespaces"]) {
                        current_struct_type += (ns.get<std::string>() + "::");
                    }
                    current_struct_type += strip_end_until_char(
                        message["structure"]["namespaced_type"]["name"], '_');
                    //   std::cout << type_name << " " << current_struct_type <<
                    //   std::endl;
                    if (type_name == current_struct_type) {
                        continue;
                    }
                }
                if (ends_with(type, "_Goal") || ends_with(type, "_Result") ||
                    ends_with(type, "_Feedback")) {
                    type_name = strip_end_until_char(type, '_');
                } else {
                    type_name = type;
                }
                std::string tmp;
                for (const auto &ns : member["type"]["namespaces"]) {
                    tmp += ns.get<std::string>() + "/";
                }
                tmp += "detail/";
                int last_colon = 0;
                for (size_t i = 0; i < type_name.size(); ++i) {
                    if (type_name[i] == ':') {
                        last_colon = i;
                    }
                }
                tmp += camel_to_snake(type_name.substr(last_colon + 1));
                tmp += suffix;
                // Add include member keeping the order
                auto it = std::find_if(includes.begin(), includes.end(),
                                       [tmp](const auto &v) { return v.first == tmp; });
                if (it == includes.end()) {
                    it = includes.insert(it, {tmp, {}});
                }
                it->second.push_back(member["name"].get<std::string>());
            }
        }

        // Convert to json list
        nlohmann::json result = nlohmann::json::array();
        for (const auto &[key, value] : includes) {
            result.push_back({{"member_names", value}, {"header_file", key}});
        }

        return result;
    });
    env.add_callback("debug", 1, [](inja::Arguments &args) -> nlohmann::json {
        std::cout << args.at(0)->dump(4) << std::endl;
        std::cout << "is_array: " << args.at(0)->is_array() << std::endl;

        return {};
    });

    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);

    // Load templates
    auto template_msg_base = env.parse_template("./idl.hpp.template");
    auto template_msg_builder = env.parse_template("./idl__builder.hpp.template");
    auto template_msg_struct = env.parse_template("./idl__struct.hpp.template");
    auto template_msg_traits = env.parse_template("./idl__traits.hpp.template");
    auto template_msg_type_support = env.parse_template("./idl__type_support.hpp.template");

    for (const auto &idl_pair : arguments_content.idl_tuples) {
        auto parsed_idl = post_process(rosidlcpp_parser::parse_ros_idl_file(idl_pair.first + "/" + idl_pair.second));

        parsed_idl["package_name"] = arguments_content.package_name;
        parsed_idl["interface_path"]["parent"] = std::filesystem::path(idl_pair.second).parent_path();
        parsed_idl["interface_path"]["parents"]["parts"] = {};
        for (const auto &part : std::filesystem::path(idl_pair.second).parent_path()) {
            parsed_idl["interface_path"]["parents"]["parts"].push_back(part.string());
        }

        std::filesystem::path(idl_pair.second).parent_path();

        parsed_idl["interface_path"]["stem"] = std::filesystem::path(idl_pair.second).stem();

        std::string msg_type(idl_pair.second.begin() + 4,
                             idl_pair.second.end() - 4);

        std::cout << parsed_idl.dump() << std::endl;

        // Save the result to an output file
        std::string output_file_path = arguments_content.output_dir + "/" + parsed_idl["interface_path"]["parent"].get<std::string>();
        std::filesystem::create_directories(arguments_content.output_dir + "/" + parsed_idl["interface_path"]["parent"].get<std::string>() + "/detail");

        {
            std::ofstream output_json_2_file(output_file_path + "/" + camel_to_snake(msg_type) + "_2.json");
            output_json_2_file << parsed_idl.dump(4);
        }

        // Render the template with the provided data
        std::string result_base = env.render(template_msg_base, parsed_idl);
        std::string result_builder = env.render(template_msg_builder, parsed_idl);
        std::string result_struct = env.render(template_msg_struct, parsed_idl);
        std::string result_traits = env.render(template_msg_traits, parsed_idl);
        std::string result_type_support = env.render(template_msg_type_support, parsed_idl);

        std::ofstream output_file_base(output_file_path + "/" + camel_to_snake(msg_type) + ".hpp");
        if (output_file_base.is_open()) {
            output_file_base << result_base;
            output_file_base.close();
        } else {
            std::cerr << "Failed to open the output file: " << output_file_path
                      << std::endl;
        }
        std::ofstream output_file_builder(output_file_path + "/detail/" + camel_to_snake(msg_type) + "__builder.hpp");
        if (output_file_builder.is_open()) {
            output_file_builder << result_builder;
            output_file_builder.close();
        } else {
            std::cerr << "Failed to open the output file: " << output_file_path
                      << std::endl;
        }
        std::ofstream output_file_struct(output_file_path + "/detail/" + camel_to_snake(msg_type) + "__struct.hpp");
        if (output_file_struct.is_open()) {
            output_file_struct << result_struct;
            output_file_struct.close();
        } else {
            std::cerr << "Failed to open the output file." << std::endl;
        }
        std::ofstream output_file_traits(output_file_path + "/detail/" + camel_to_snake(msg_type) + "__traits.hpp");
        if (output_file_traits.is_open()) {
            output_file_traits << result_traits;
            output_file_traits.close();
        } else {
            std::cerr << "Failed to open the output file." << std::endl;
        }
        std::ofstream output_file_type_support(output_file_path + "/detail/" + camel_to_snake(msg_type) + "__type_support.hpp");
        if (output_file_type_support.is_open()) {
            output_file_type_support << result_type_support;
            output_file_type_support.close();
        } else {
            std::cerr << "Failed to open the output file." << std::endl;
        }
    }
}