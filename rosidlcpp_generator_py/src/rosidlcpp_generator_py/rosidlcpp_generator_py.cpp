#include <cassert>
#include <cctype>
#include <cstdint>
#include <fmt/core.h>
#include <limits>
#include <nlohmann/json_fwd.hpp>
#include <rosidlcpp_generator_py/rosidlcpp_generator_py.hpp>

#include <generator_base.hpp>

#include <rosidlcpp_parser.hpp>

#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <fmt/format.h>

#include <argparse/argparse.hpp>
#include <inja/inja.hpp>

struct SpecialNestedType {
    std::string dtype;
    std::string type_code;
};
const std::unordered_map<std::string, SpecialNestedType> SPECIAL_NESTED_BASIC_TYPES = {
    {"float", {"numpy.float32", "f"}},
    {"double", {"numpy.float64", "d"}},
    {"int8", {"numpy.int8", "b"}},
    {"uint8", {"numpy.uint8", "B"}},
    {"int16", {"numpy.int16", "h"}},
    {"uint16", {"numpy.uint16", "H"}},
    {"int32", {"numpy.int32", "i"}},
    {"uint32", {"numpy.uint32", "I"}},
    {"int64", {"numpy.int64", "q"}},
    {"uint64", {"numpy.uint64", "Q"}}};

constexpr std::string_view SERVICE_EVENT_MESSAGE_SUFFIX = "_Event";
constexpr std::string_view SERVICE_REQUEST_MESSAGE_SUFFIX = "_Request";
constexpr std::string_view SERVICE_RESPONSE_MESSAGE_SUFFIX = "_Response";
constexpr std::string_view ACTION_GOAL_SUFFIX = "_Goal";
constexpr std::string_view ACTION_RESULT_SUFFIX = "_Result";
constexpr std::string_view ACTION_FEEDBACK_SUFFIX = "_Feedback";

nlohmann::json get_imports(inja::Arguments &args) {
    const auto members = *args.at(0);

    nlohmann::json result = nlohmann::json::object();
    if (!members.empty()) {
        result["import rosidl_parser.definition"] = nlohmann::json::array();
    }

    for (const auto &member : members) {
        nlohmann::json type = member["type"];
        if (rosidlcpp_core::is_nestedtype(type)) {
            type = member["type"]["value_type"];
        }
        if (member["name"] != rosidlcpp_core::EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME) {
            if (!result.contains("import builtins"))
                result["import builtins"] = nlohmann::json::array();
        }
        if (rosidlcpp_core::is_float(type)) {
            if (!result.contains("import math"))
                result["import math"] = nlohmann::json::array();
        }
        if (rosidlcpp_core::is_nestedtype(member["type"]) && SPECIAL_NESTED_BASIC_TYPES.contains(member["type"]["value_type"]["name"])) {
            if (rosidlcpp_core::is_array(member["type"])) {
                result["import numpy"].push_back(member["name"]);
            } else if (rosidlcpp_core::is_sequence(member["type"])) {
                result["import array"].push_back(member["name"]);
            } else {
                assert(false && "Unexpected nested type");
            }
        }
    }

    return result;
}

std::string primitive_value_to_py(const nlohmann::json &type, const nlohmann::json &value) {
    assert(!value.is_null());

    if (rosidlcpp_core::is_string(type)) {
        const auto string_value = value.get<std::string>();
        if (!string_value.contains('\'')) {
            return std::format("'{}'", string_value);
        } else if (!string_value.contains('"')) {
            return std::format("\"{}\"", string_value);
        } else {
            return std::format("'''{}'''", string_value);
        }
    }

    if (type["name"] == "boolean") {
        return value.get<bool>() ? "True" : "False";
    }

    if (type["name"] == "octet") {
        if (std::isprint(value.get<int>())) {
            return std::format("b'{}'", static_cast<char>(value.get<int>()));
        }
        return std::format("b'\\x{:02x}'", value.get<int>());
    }

    if (type["name"] == "char") {
        return std::format("'{}'", static_cast<char>(value.get<int>()));
    }

    if (rosidlcpp_core::is_signed_integer(type)) {
        return std::to_string(value.get<int64_t>());
    }

    if (rosidlcpp_core::is_unsigned_integer(type)) {
        return std::to_string(value.get<uint64_t>());
    }

    if (rosidlcpp_core::is_float(type)) {
        return value.dump(); // TODO: Do not rely on nlohmann::json to format floating point numbers
    }

    return value.get<std::string>();
}

std::string constant_value_to_py(const nlohmann::json &type, const nlohmann::json &value) {
    assert(!value.is_null());

    if (rosidlcpp_core::is_primitive(type)) {
        if (type["name"] == "boolean") {
            return value.get<bool>() ? "True" : "False";
        }

        if (rosidlcpp_core::is_signed_integer(type) || rosidlcpp_core::is_unsigned_integer(type)) {
            return std::to_string(value.get<int>());
        }

        if (type["name"] == "char") {
            return std::format("'{}'", static_cast<char>(value.get<int>()));
        }

        if (type["name"] == "octet") {
            if (std::isprint(value.get<int>())) {
                return std::format("b'{}'", static_cast<char>(value.get<int>()));
            }
            return std::format("b'\\x{:02x}'", value.get<int>());
        }

        if (rosidlcpp_core::is_float(type)) {
            return value.dump(); // TODO: Do not rely on nlohmann::json to format floating point numbers
        }
    }

    if (rosidlcpp_core::is_string(type)) {
        if (!value.get<std::string>().contains('\'')) {
            return std::format("'{}'", value.get<std::string>());
        } else if (!value.get<std::string>().contains('"')) {
            return std::format("\"{}\"", value.get<std::string>());
        } else {
            return std::format("'''{}'''", value.get<std::string>());
        }
    }

    assert(false && "unknown constant type");
    return "";
}

GeneratorPython::GeneratorPython(int argc, char **argv)
    : GeneratorBase() {
    // Arguments
    argparse::ArgumentParser argument_parser("rosidl_generator_cpp");
    argument_parser.add_argument("--typesupport-impls")
        .required()
        .help("The list of typesupport implementations to generate");
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
    auto typesupport_implementations = argument_parser.get<std::string>("--typesupport-impls");
    m_typesupport_implementations = rosidlcpp_parser::split_string(typesupport_implementations, ";");

    m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

    p_env = std::make_unique<inja::Environment>(m_arguments.template_dir + "/", m_arguments.output_dir + "/");
    p_env->set_trim_blocks(true);
    p_env->set_lstrip_blocks(true);

    p_env->add_void_callback("debug", 1, [](inja::Arguments &args) -> void {
        std::cout << args.at(0)->dump(4) << std::endl;
    });
    p_env->add_callback("get_imports", 1, get_imports);
    p_env->add_callback("constant_value_to_py", 2, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        const auto value = *args.at(1);

        return constant_value_to_py(type, value);
    });
    p_env->add_callback("convert_camel_case_to_lower_case_underscore", 1,
                        [](inja::Arguments &args) {
                            return rosidlcpp_core::camel_to_snake(args.at(0)->get<std::string>());
                        });
    p_env->add_callback("span", 3, [](inja::Arguments &args) {
        auto list = *args.at(0);
        auto start = args.at(1)->get<int>();
        auto end = args.at(2)->get<int>();

        return nlohmann::json(list.begin() + start, list.begin() + end);
    });
    p_env->add_callback("get_importable_typesupports", 1, [](inja::Arguments &args) {
        const auto members = *args.at(0);

        nlohmann::json result = nlohmann::json::array();
        for (const auto &member : members) {
            auto type = member["type"];
            if (rosidlcpp_core::is_nestedtype(member["type"])) {
                type = member["type"]["value_type"];
            }
            if (rosidlcpp_core::is_namespaced(type)) {
                if (type["name"].get<std::string>().ends_with(SERVICE_REQUEST_MESSAGE_SUFFIX) || type["name"].get<std::string>().ends_with(SERVICE_RESPONSE_MESSAGE_SUFFIX)) {
                    continue;
                }
                auto typesupport = nlohmann::json::object();
                if (type["name"].get<std::string>().ends_with(ACTION_GOAL_SUFFIX) || type["name"].get<std::string>().ends_with(ACTION_RESULT_SUFFIX) || type["name"].get<std::string>().ends_with(ACTION_FEEDBACK_SUFFIX)) {
                    auto action_info = rosidlcpp_parser::split_string(type["name"].get<std::string>(), "_");
                    typesupport["namespaces"] = type["namespaces"];
                    typesupport["type"] = action_info[0];
                    typesupport["type2"] = fmt::format("{}", fmt::join(action_info, ".")); // TODO: Find better name
                } else {
                    typesupport["namespaces"] = type["namespaces"];
                    typesupport["type"] = type["name"];
                    typesupport["type2"] = type["name"]; // TODO: Find better name
                }
                if (std::find(result.begin(), result.end(), typesupport) == result.end()) {
                    result.push_back(typesupport);
                }
            }
        }

        return result;
    });
    p_env->add_callback("value_to_py", 2, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        const auto value = *args.at(1);

        if (!rosidlcpp_core::is_nestedtype(type)) {
            return primitive_value_to_py(type, value);
        }

        std::vector<nlohmann::json> py_values;
        for (const auto &single_value : value) {
            auto py_value = primitive_value_to_py(type["value_type"], single_value);
            py_values.push_back(py_value);
        }

        if (rosidlcpp_core::is_primitive(type["value_type"]) && SPECIAL_NESTED_BASIC_TYPES.contains(type["value_type"]["name"])) {
            if (rosidlcpp_core::is_array(type)) {
                return fmt::format("numpy.array(({}, ), dtype={})",
                                   fmt::join(py_values, ", "),
                                   SPECIAL_NESTED_BASIC_TYPES.at(type["value_type"]["name"]).dtype);
            }
            if (rosidlcpp_core::is_sequence(type)) {
                return fmt::format("array.array('{}', ({}, ))",
                                   SPECIAL_NESTED_BASIC_TYPES.at(type["value_type"]["name"]).type_code,
                                   fmt::join(py_values, ", "));
            }
        }

        return fmt::format("[{}]", fmt::join(py_values, ", "));
    });
    p_env->add_callback("is_primitive", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_primitive(type);
    });
    p_env->add_callback("is_string", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_string(type);
    });
    p_env->add_callback("get_rosidl_parser_type", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        if (type["name"] == "sequence") {
            if (type.contains("maximum_size")) {
                return "rosidl_parser.definition.BoundedSequence";
            } else {
                return "rosidl_parser.definition.UnboundedSequence";
            }
        }
        if (type["name"] == "array") {
            return "rosidl_parser.definition.Array";
        }
        if (type["name"] == "string") {
            if (type.contains("maximum_size")) {
                return "rosidl_parser.definition.BoundedString";
            } else {
                return "rosidl_parser.definition.UnboundedString";
            }
        }
        if (type["name"] == "wstring") {
            if (type.contains("maximum_size")) {
                return "rosidl_parser.definition.BoundedWString";
            } else {
                return "rosidl_parser.definition.UnboundedWString";
            }
        }
        if (type.contains("namespaces")) {
            return "rosidl_parser.definition.NamespacedType";
        }
        return "rosidl_parser.definition.BasicType";
    });
    p_env->add_callback("is_character", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_character(type);
    });
    p_env->add_callback("get_special_nested_basic_type", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        if (SPECIAL_NESTED_BASIC_TYPES.contains(type["name"].get<std::string>())) {
            const auto special_basic_type = SPECIAL_NESTED_BASIC_TYPES.at(type["name"]);
            return {
                {"dtype", special_basic_type.dtype},
                {"type_code", special_basic_type.type_code}};
        }
        return nlohmann::json::object();
    });
    p_env->add_callback("get_python_type", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        if (rosidlcpp_core::is_string(type)) {
            return "str";
        }
        if (rosidlcpp_core::is_primitive(type)) {
            if (rosidlcpp_core::is_float(type)) {
                return "float";
            }
            if (rosidlcpp_core::is_character(type)) {
                return "str";
            }
            if (type["name"] == "boolean") {
                return "bool";
            }
            if (type["name"] == "octet") {
                return "bytes";
            }
            return "int";
        }
        if (rosidlcpp_core::is_sequence(type) || rosidlcpp_core::is_array(type)) {
            return "list";
        }
        if (rosidlcpp_core::is_namespaced(type)) {
            return type["name"].get<std::string>();
        }
        return "object";
    });
    p_env->add_callback("is_action_type", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return type["name"].get<std::string>().ends_with(ACTION_GOAL_SUFFIX) || type["name"].get<std::string>().ends_with(ACTION_RESULT_SUFFIX) || type["name"].get<std::string>().ends_with(ACTION_FEEDBACK_SUFFIX);
    });
    p_env->add_callback("is_service_type", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return type["name"].get<std::string>().ends_with(SERVICE_REQUEST_MESSAGE_SUFFIX) || type["name"].get<std::string>().ends_with(SERVICE_RESPONSE_MESSAGE_SUFFIX);
    });
    p_env->add_callback("is_nestedtype", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_nestedtype(type);
    });
    p_env->add_callback("is_signed_integer", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_signed_integer(type);
    });
    p_env->add_callback("is_unsigned_integer", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_unsigned_integer(type);
    });
    p_env->add_callback("get_bound", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        if (type["name"] == "int8") {
            return {{"max", std::numeric_limits<std::int8_t>::max()}, {"max_plus_one", "128"}, {"max_string", std::to_string(std::numeric_limits<std::int8_t>::max())}};
        }
        if (type["name"] == "int16") {
            return {{"max", std::numeric_limits<std::int16_t>::max()}, {"max_plus_one", "32768"}, {"max_string", std::to_string(std::numeric_limits<std::int16_t>::max())}};
        }
        if (type["name"] == "int32") {
            return {{"max", std::numeric_limits<std::int32_t>::max()}, {"max_plus_one", "2147483648"}, {"max_string", std::to_string(std::numeric_limits<std::int32_t>::max())}};
        }
        if (type["name"] == "int64") {
            return {{"max", std::numeric_limits<std::int64_t>::max()}, {"max_plus_one", "9223372036854775808"}, {"max_string", std::to_string(std::numeric_limits<std::int64_t>::max())}};
        }
        if (type["name"] == "uint8") {
            return {{"max", std::numeric_limits<std::uint8_t>::max()}, {"max_plus_one", "256"}, {"max_string", std::to_string(std::numeric_limits<std::uint8_t>::max())}};
        }
        if (type["name"] == "uint16") {
            return {{"max", std::numeric_limits<std::uint16_t>::max()}, {"max_plus_one", "65536"}, {"max_string", std::to_string(std::numeric_limits<std::uint16_t>::max())}};
        }
        if (type["name"] == "uint32") {
            return {{"max", std::numeric_limits<std::uint32_t>::max()}, {"max_plus_one", "4294967296"}, {"max_string", std::to_string(std::numeric_limits<std::uint32_t>::max())}};
        }
        if (type["name"] == "uint64") {
            return {{"max", std::numeric_limits<std::uint64_t>::max()}, {"max_plus_one", "18446744073709551616"}, {"max_string", "18446744073709551615"}};
        }
        if (type["name"] == "float") {
            return {{"max", std::numeric_limits<float>::max()}, {"max_string", "3.402823466e+38"}};
        }
        if (type["name"] == "double") {
            return {{"max", std::numeric_limits<double>::max()}, {"max_string", "1.7976931348623157e+308"}};
        }
        if (type["name"] == "long double") {
            return {{"max", std::numeric_limits<long double>::max()}, {"max_string", std::to_string(std::numeric_limits<long double>::max())}};
        }
        return {{"max", 0ULL}, {"max_plus_one", "unknown"}, {"max_string", "unknown"}};
    });
    p_env->add_callback("is_float", 1, [](inja::Arguments &args) -> nlohmann::json {
        const auto type = *args.at(0);
        return rosidlcpp_core::is_float(type);
    });
    p_env->add_callback("push_back", 2, [](inja::Arguments &args) {
        auto list = *args.at(0);
        auto value = *args.at(1);
        if (value.is_array()) {
            for (const auto &v : value) {
                list.push_back(v);
            }
        } else {
            list.push_back(value);
        }
        return list;
    });
    p_env->add_callback("insert", 3, [](inja::Arguments &args) {
        auto list = *args.at(0);
        const auto index = args.at(1)->get<int>();
        list.insert(list.begin() + index, args.at(2)->get<std::string>());
        return list;
    });
    p_env->add_callback("format", 2, [](inja::Arguments &args) {
        auto format = args.at(0)->get<std::string>();
        const auto arg1 = *args.at(1);
        if (arg1.is_string()) {
            return fmt::format(fmt::runtime(format), arg1.get<std::string>());
        }
        if (arg1.is_number_integer()) {
            return fmt::format(fmt::runtime(format), arg1.get<int>());
        }
        if (arg1.is_number_float()) {
            return fmt::format(fmt::runtime(format), arg1.get<double>());
        }
        return std::string{"unknown"};
    });
    p_env->add_callback("format", 3, [](inja::Arguments &args) {
        auto format = args.at(0)->get<std::string>();
        const auto arg1 = *args.at(1);
        const auto arg2 = *args.at(2);

        auto value1 = arg1.dump();
        auto value2 = arg2.dump();

        return fmt::format(fmt::runtime(format), value1, value2);

        if (arg1.is_string()) {
            if (arg2.is_string()) {
                return fmt::format(fmt::runtime(format), arg1.get<std::string>(), arg2.get<std::string>());
            }
            if (arg2.is_number_integer()) {
                return fmt::format(fmt::runtime(format), arg1.get<std::string>(), arg2.get<long long>());
            }
            if (arg2.is_number_float()) {
                return fmt::format(fmt::runtime(format), arg1.get<std::string>(), arg2.get<double>());
            }
        }
        if (arg1.is_number_integer()) {
            if (arg2.is_string()) {
                return fmt::format(fmt::runtime(format), arg1.get<long long>(), arg2.get<std::string>());
            }
            if (arg2.is_number_integer()) {
                return fmt::format(fmt::runtime(format), arg1.get<long long>(), arg2.get<long long>());
            }
            if (arg2.is_number_float()) {
                return fmt::format(fmt::runtime(format), arg1.get<long long>(), arg2.get<double>());
            }
        }
        if (arg1.is_number_float()) {
            if (arg2.is_string()) {
                return fmt::format(fmt::runtime(format), arg1.get<double>(), arg2.get<std::string>());
            }
            if (arg2.is_number_integer()) {
                return fmt::format(fmt::runtime(format), arg1.get<double>(), arg2.get<long long>());
            }
            if (arg2.is_number_float()) {
                return fmt::format(fmt::runtime(format), arg1.get<double>(), arg2.get<double>());
            }
        }
        return std::string{"unknown"};
    });
    p_env->add_callback("string_contains", 2, [](inja::Arguments &args) {
        const auto str = args.at(0)->get<std::string>();
        const auto substr = args.at(1)->get<std::string>();
        return str.find(substr) != std::string::npos;
    });
    p_env->add_callback("replace", 3, [](inja::Arguments &args) {
        auto str = args.at(0)->get<std::string>();
        const auto substr = args.at(1)->get<std::string>();
        const auto replacement = args.at(2)->get<std::string>();
        std::size_t pos = 0;
        while ((pos = str.find(substr, pos)) != std::string::npos) {
            str.replace(pos, substr.length(), replacement);
            pos += replacement.length();
        }
        return str;
    });
    p_env->add_callback("primitive_msg_type_to_c", 1, [](inja::Arguments &args) {
        const auto type = *args.at(0);
        if (rosidlcpp_core::is_primitive(type)) {
            if (type["name"] == "boolean") {
                return "bool";
            }
            if (type["name"] == "byte") {
                return "int8_t";
            }
            if (type["name"] == "int8") {
                return "int8_t";
            }
            if (type["name"] == "int16") {
                return "int16_t";
            }
            if (type["name"] == "int32") {
                return "int32_t";
            }
            if (type["name"] == "int64") {
                return "int64_t";
            }
            if (type["name"] == "uint8") {
                return "uint8_t";
            }
            if (type["name"] == "uint16") {
                return "uint16_t";
            }
            if (type["name"] == "uint32") {
                return "uint32_t";
            }
            if (type["name"] == "uint64") {
                return "uint64_t";
            }
            if (type["name"] == "char") {
                return "char";
            }
            if (type["name"] == "octet") {
                return "uint8_t";
            }
            if (type["name"] == "string") {
                return "rosidl_runtime_c__String";
            }
            if (type["name"] == "wstring") {
                return "rosidl_runtime_c__U16String";
            }
            if (type["name"] == "float") {
                return "float";
            }
            if (type["name"] == "double") {
                return "double";
            }
            if (type["name"] == "long double") {
                return "long double";
            }
        }
        return "todo";
    });
    p_env->add_callback("EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME", 0, [](inja::Arguments &) {
        return rosidlcpp_core::EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME;
    });
    p_env->add_callback("set_global_variable", 2, [&](inja::Arguments &args) {
        auto name = args.at(0)->get<std::string>();
        auto value = *args.at(1);
        m_global_storage[name] = value;
        return m_global_storage[name];
    });
    p_env->add_callback("get_global_variable", 1, [&](inja::Arguments &args) {
        auto name = args.at(0)->get<std::string>();
        return m_global_storage[name];
    });
    p_env->add_callback("unique", 1, [](inja::Arguments &args) { // also sorts
        auto list = *args.at(0);
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
        return list;
    });
    p_env->add_callback("split_string", 2, [](inja::Arguments &args) {
        const auto value = args.at(0)->get<std::string>();
        const auto sep = args.at(1)->get<std::string>();
        return rosidlcpp_parser::split_string(value, sep);
    });
}

void GeneratorPython::run() {
    // Load templates
    inja::Template template_idl_py = p_env->parse_template("./_idl.py.template");
    inja::Template template_idl_support_c = p_env->parse_template("./_idl_support.c.template");
    inja::Template template_idl_pkg_typesupport = p_env->parse_template("./_idl_pkg_typesupport_entry_point.c.template");
    inja::Template template_init = p_env->parse("{% for import in imports %}{{ import }}\n{% endfor %}");

    // Combined ros_json
    nlohmann::json pkg_json;
    pkg_json["package_name"] = m_arguments.package_name;
    pkg_json["messages"] = nlohmann::json::array();
    pkg_json["services"] = nlohmann::json::array();
    pkg_json["actions"] = nlohmann::json::array();

    // __init__.py file
    std::unordered_map<std::string, std::vector<std::string>> init_py;

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

        p_env->write(template_idl_py, ros_json, std::format("{}/_{}.py", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));
        p_env->write(template_idl_support_c, ros_json, std::format("{}/_{}_s.c", msg_directory, rosidlcpp_core::camel_to_snake(msg_type)));

        // Add to the combined ros_json
        if (ros_json.contains("messages")) {
            for (auto msg : ros_json["messages"]) {
                msg["interface_path"]["filepath"] = file_path;
                msg["interface_path"]["filename"] = msg_type;
                msg["interface_path"]["filedir"] = msg_directory;
                pkg_json["messages"].push_back(msg);
            }
        }
        if (ros_json.contains("services")) {
            for (auto srv : ros_json["services"]) {
                srv["interface_path"]["filepath"] = file_path;
                srv["interface_path"]["filename"] = msg_type;
                srv["interface_path"]["filedir"] = msg_directory;
                pkg_json["services"].push_back(srv);
            }
        }
        if(ros_json.contains("actions")) {
            for (auto action : ros_json["actions"]) {
                action["interface_path"]["filepath"] = file_path;
                action["interface_path"]["filename"] = msg_type;
                action["interface_path"]["filedir"] = msg_directory;
                pkg_json["actions"].push_back(action);
            }
        }

        // Add to __init__.py
        std::vector<std::string> type_suffixes{{""}};
        if (msg_directory == "srv") {
            type_suffixes.emplace_back(SERVICE_EVENT_MESSAGE_SUFFIX);
            type_suffixes.emplace_back(SERVICE_REQUEST_MESSAGE_SUFFIX);
            type_suffixes.emplace_back(SERVICE_RESPONSE_MESSAGE_SUFFIX);
        }
        if (msg_directory == "action") {
            // TODO: Add action suffixes
        }

        for(const auto &type_suffix : type_suffixes) {
            init_py[msg_directory].push_back(std::format(
                "from {}.{}._{} import {}{}  # noqa: F401",
                m_arguments.package_name,
                msg_directory,
                rosidlcpp_core::camel_to_snake(msg_type),
                msg_type,
                type_suffix));
        }
    }

    // Generate package files
    for (const auto &typesupport : m_typesupport_implementations) {
        pkg_json["typesupport_impl"] = typesupport;
        p_env->write(template_idl_pkg_typesupport, pkg_json, std::format("_{}_s.ep.{}.c", m_arguments.package_name, typesupport));
    }

    // Generate __init__.py
    for (const auto &[msg_directory, imports] : init_py) {
        nlohmann::json init_py_json;
        init_py_json["imports"] = imports;
        p_env->write(template_init, init_py_json, std::format("{}/__init__.py", msg_directory));
    }
}

int main(int argc, char **argv) {
    GeneratorPython generator(argc, argv);
    generator.run();
    return 0;
}