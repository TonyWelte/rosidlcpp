#include <rosidlcpp_generator_py/rosidlcpp_generator_py.hpp>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

#include <rosidlcpp_generator_core/generator_base.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <argparse/argparse.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <unordered_set>

using rosidlcpp_core::CallbackArgs;

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

const static std::unordered_set<std::string> PYTHON_BUILTINS = {
    "ArithmeticError", "AssertionError", "AttributeError", "BaseException",
    "BaseExceptionGroup", "BlockingIOError", "BrokenPipeError", "BufferError",
    "BytesWarning", "ChildProcessError", "ConnectionAbortedError", "ConnectionError",
    "ConnectionRefusedError", "ConnectionResetError", "DeprecationWarning", "EOFError",
    "Ellipsis", "EncodingWarning", "EnvironmentError", "Exception", "ExceptionGroup",
    "False", "FileExistsError", "FileNotFoundError", "FloatingPointError",
    "FutureWarning", "GeneratorExit", "IOError", "ImportError", "ImportWarning",
    "IndentationError", "IndexError", "InterruptedError", "IsADirectoryError",
    "KeyError", "KeyboardInterrupt", "LookupError", "MemoryError",
    "ModuleNotFoundError", "NameError", "None", "NotADirectoryError", "NotImplemented",
    "NotImplementedError", "OSError", "OverflowError", "PendingDeprecationWarning",
    "PermissionError", "ProcessLookupError", "RecursionError", "ReferenceError",
    "ResourceWarning", "RuntimeError", "RuntimeWarning", "StopAsyncIteration",
    "StopIteration", "SyntaxError", "SyntaxWarning", "SystemError", "SystemExit",
    "TabError", "TimeoutError", "True", "TypeError", "UnboundLocalError",
    "UnicodeDecodeError", "UnicodeEncodeError", "UnicodeError",
    "UnicodeTranslateError", "UnicodeWarning", "UserWarning", "ValueError", "Warning",
    "ZeroDivisionError", "_", "__build_class__", "__debug__", "__doc__", "__import__",
    "__loader__", "__name__", "__package__", "__spec__", "abs", "aiter", "all",
    "anext", "any", "ascii", "bin", "bool", "breakpoint", "bytearray", "bytes",
    "callable", "chr", "classmethod", "compile", "complex", "copyright", "credits",
    "delattr", "dict", "dir", "divmod", "enumerate", "eval", "exec", "exit",
    "filter", "float", "format", "frozenset", "getattr", "globals", "hasattr",
    "hash", "help", "hex", "id", "input", "int", "isinstance", "issubclass", "iter",
    "len", "license", "list", "locals", "map", "max", "memoryview", "min", "next",
    "object", "oct", "open", "ord", "pow", "print", "property", "quit", "range",
    "repr", "reversed", "round", "set", "setattr", "slice", "sorted",
    "staticmethod", "str", "sum", "super", "tuple", "type", "vars", "zip"};

constexpr std::string_view SERVICE_EVENT_MESSAGE_SUFFIX = "_Event";
constexpr std::string_view SERVICE_REQUEST_MESSAGE_SUFFIX = "_Request";
constexpr std::string_view SERVICE_RESPONSE_MESSAGE_SUFFIX = "_Response";
constexpr std::string_view ACTION_GOAL_SUFFIX = "_Goal";
constexpr std::string_view ACTION_RESULT_SUFFIX = "_Result";
constexpr std::string_view ACTION_FEEDBACK_SUFFIX = "_Feedback";

nlohmann::json get_imports(CallbackArgs& args) {
  const auto members = *args.at(0);

  nlohmann::json result = nlohmann::json::object();
  if (!members.empty()) {
    result["import rosidl_parser.definition"] = nlohmann::json::array();
  }

  for (const auto& member : members) {
    nlohmann::json type = member["type"];
    if (rosidlcpp_core::is_nestedtype(type)) {
      type = member["type"]["value_type"];
    }
    if (member["name"] !=
        rosidlcpp_core::EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME) {
      if (!result.contains("import builtins"))
        result["import builtins"] = nlohmann::json::array();
    }
    if (rosidlcpp_core::is_float(type)) {
      if (!result.contains("import math"))
        result["import math"] = nlohmann::json::array();
    }
    if (rosidlcpp_core::is_nestedtype(member["type"]) &&
        SPECIAL_NESTED_BASIC_TYPES.contains(
            member["type"]["value_type"]["name"])) {
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

std::string primitive_value_to_py(nlohmann::json type, nlohmann::json value) {
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
    return value.dump();  // TODO: Do not rely on nlohmann::json to format
                          // floating point numbers
  }

  return value.get<std::string>();
}

std::string constant_value_to_py(CallbackArgs& args) {
  const auto type = *args.at(0);
  const auto value = *args.at(1);

  assert(!value.is_null());

  if (rosidlcpp_core::is_primitive(type)) {
    if (type["name"] == "boolean") {
      return value.get<bool>() ? "True" : "False";
    }

    if (rosidlcpp_core::is_signed_integer(type) ||
        rosidlcpp_core::is_unsigned_integer(type)) {
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
      return value.dump();  // TODO: Do not rely on nlohmann::json to format
                            // floating point numbers
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

auto get_importable_typesupports(CallbackArgs& args) -> nlohmann::json {
  const auto members = *args.at(0);

  nlohmann::json result = nlohmann::json::array();
  for (const auto& member : members) {
    auto type = member["type"];
    if (rosidlcpp_core::is_nestedtype(member["type"])) {
      type = member["type"]["value_type"];
    }
    if (rosidlcpp_core::is_namespaced(type)) {
      if (type["name"].get<std::string>().ends_with(
              SERVICE_REQUEST_MESSAGE_SUFFIX) ||
          type["name"].get<std::string>().ends_with(
              SERVICE_RESPONSE_MESSAGE_SUFFIX)) {
        continue;
      }
      auto typesupport = nlohmann::json::object();
      if (type["name"].get<std::string>().ends_with(ACTION_GOAL_SUFFIX) ||
          type["name"].get<std::string>().ends_with(
              ACTION_RESULT_SUFFIX) ||
          type["name"].get<std::string>().ends_with(
              ACTION_FEEDBACK_SUFFIX)) {
        auto action_info = rosidlcpp_parser::split_string(
            type["name"].get<std::string>(), "_");
        typesupport["namespaces"] = type["namespaces"];
        typesupport["type"] = action_info[0];
        typesupport["type2"] = fmt::format(
            "{}", fmt::join(action_info, "."));  // TODO: Find better name
      } else {
        typesupport["namespaces"] = type["namespaces"];
        typesupport["type"] = type["name"];
        typesupport["type2"] = type["name"];  // TODO: Find better name
      }
      if (std::find(result.begin(), result.end(), typesupport) ==
          result.end()) {
        result.push_back(typesupport);
      }
    }
  }

  return result;
}

auto value_to_py(CallbackArgs& args) -> nlohmann::json {
  const auto type = *args.at(0);
  const auto value = *args.at(1);

  if (!rosidlcpp_core::is_nestedtype(type)) {
    return primitive_value_to_py(type, value);
  }

  std::vector<nlohmann::json> py_values;
  for (const auto& single_value : value) {
    auto py_value =
        primitive_value_to_py(type["value_type"], single_value);
    py_values.push_back(py_value);
  }

  if (rosidlcpp_core::is_primitive(type["value_type"]) &&
      SPECIAL_NESTED_BASIC_TYPES.contains(type["value_type"]["name"])) {
    if (rosidlcpp_core::is_array(type)) {
      return fmt::format(
          "numpy.array(({}, ), dtype={})", fmt::join(py_values, ", "),
          SPECIAL_NESTED_BASIC_TYPES.at(type["value_type"]["name"])
              .dtype);
    }
    if (rosidlcpp_core::is_sequence(type)) {
      return fmt::format(
          "array.array('{}', ({}, ))",
          SPECIAL_NESTED_BASIC_TYPES.at(type["value_type"]["name"])
              .type_code,
          fmt::join(py_values, ", "));
    }
  }

  return fmt::format("[{}]", fmt::join(py_values, ", "));
}

auto get_rosidl_parser_type(CallbackArgs& args) -> nlohmann::json {
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
}

auto get_special_nested_basic_type(CallbackArgs& args) -> nlohmann::json {
  const auto type = *args.at(0);
  if (SPECIAL_NESTED_BASIC_TYPES.contains(
          type["name"].get<std::string>())) {
    const auto special_basic_type =
        SPECIAL_NESTED_BASIC_TYPES.at(type["name"]);
    return {{"dtype", special_basic_type.dtype},
            {"type_code", special_basic_type.type_code}};
  }
  return nlohmann::json::object();
}

auto get_python_type(CallbackArgs& args) -> nlohmann::json {
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
  if (rosidlcpp_core::is_sequence(type) ||
      rosidlcpp_core::is_array(type)) {
    return "list";
  }
  if (rosidlcpp_core::is_namespaced(type)) {
    return type["name"].get<std::string>();
  }
  return "object";
}

auto get_bound(CallbackArgs& args) -> nlohmann::json {
  const auto type = *args.at(0);
  if (type["name"] == "int8") {
    return {{"max", std::numeric_limits<std::int8_t>::max()},
            {"max_plus_one", "128"},
            {"max_string",
             std::to_string(std::numeric_limits<std::int8_t>::max())}};
  }
  if (type["name"] == "int16") {
    return {{"max", std::numeric_limits<std::int16_t>::max()},
            {"max_plus_one", "32768"},
            {"max_string",
             std::to_string(std::numeric_limits<std::int16_t>::max())}};
  }
  if (type["name"] == "int32") {
    return {{"max", std::numeric_limits<std::int32_t>::max()},
            {"max_plus_one", "2147483648"},
            {"max_string",
             std::to_string(std::numeric_limits<std::int32_t>::max())}};
  }
  if (type["name"] == "int64") {
    return {{"max", std::numeric_limits<std::int64_t>::max()},
            {"max_plus_one", "9223372036854775808"},
            {"max_string",
             std::to_string(std::numeric_limits<std::int64_t>::max())}};
  }
  if (type["name"] == "uint8") {
    return {{"max", std::numeric_limits<std::uint8_t>::max()},
            {"max_plus_one", "256"},
            {"max_string",
             std::to_string(std::numeric_limits<std::uint8_t>::max())}};
  }
  if (type["name"] == "uint16") {
    return {{"max", std::numeric_limits<std::uint16_t>::max()},
            {"max_plus_one", "65536"},
            {"max_string",
             std::to_string(std::numeric_limits<std::uint16_t>::max())}};
  }
  if (type["name"] == "uint32") {
    return {{"max", std::numeric_limits<std::uint32_t>::max()},
            {"max_plus_one", "4294967296"},
            {"max_string",
             std::to_string(std::numeric_limits<std::uint32_t>::max())}};
  }
  if (type["name"] == "uint64") {
    return {{"max", std::numeric_limits<std::uint64_t>::max()},
            {"max_plus_one", "18446744073709551616"},
            {"max_string", "18446744073709551615"}};
  }
  if (type["name"] == "float") {
    return {{"max", std::numeric_limits<float>::max()},
            {"max_string", "3.402823466e+38"}};
  }
  if (type["name"] == "double") {
    return {{"max", std::numeric_limits<double>::max()},
            {"max_string", "1.7976931348623157e+308"}};
  }
  if (type["name"] == "long double") {
    return {{"max", std::numeric_limits<long double>::max()},
            {"max_string",
             std::to_string(std::numeric_limits<long double>::max())}};
  }
  return {{"max", 0ULL},
          {"max_plus_one", "unknown"},
          {"max_string", "unknown"}};
}

auto primitive_msg_type_to_c(CallbackArgs& args) -> std::string {
  const auto type = *args.at(0);
  static const std::unordered_map<std::string, std::string> type_map = {
      {"boolean", "bool"},
      {"byte", "int8_t"},
      {"int8", "int8_t"},
      {"int16", "int16_t"},
      {"int32", "int32_t"},
      {"int64", "int64_t"},
      {"uint8", "uint8_t"},
      {"uint16", "uint16_t"},
      {"uint32", "uint32_t"},
      {"uint64", "uint64_t"},
      {"char", "char"},
      {"octet", "uint8_t"},
      {"string", "rosidl_runtime_c__String"},
      {"wstring", "rosidl_runtime_c__U16String"},
      {"float", "float"},
      {"double", "double"},
      {"long double", "long double"}};

  auto it = type_map.find(type["name"].get<std::string>());
  if (it != type_map.end()) {
    return it->second;
  }

  return {};
}

auto is_python_builtin(CallbackArgs& args) -> bool {
  const auto name = *args.at(0);
  return PYTHON_BUILTINS.contains(name.get<std::string>());
}

GeneratorPython::GeneratorPython(int argc, char** argv) : GeneratorBase() {
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
  } catch (const std::exception& error) {
    std::cerr << error.what() << std::endl;
    std::cerr << argument_parser;
    std::exit(1);  // TODO: Don't use exit in constructor
  }

  auto generator_arguments_file =
      argument_parser.get<std::string>("--generator-arguments-file");
  auto typesupport_implementations =
      argument_parser.get<std::string>("--typesupport-impls");
  m_typesupport_implementations =
      rosidlcpp_parser::split_string(typesupport_implementations, ";");

  m_arguments = rosidlcpp_core::parse_arguments(generator_arguments_file);

  m_env.set_input_path(m_arguments.template_dir + "/");
  m_env.set_output_path(m_arguments.output_dir + "/");

  m_env.add_callback("get_imports", 1, get_imports);
  m_env.add_callback("constant_value_to_py", 2, constant_value_to_py);
  m_env.add_callback("get_importable_typesupports", 1, get_importable_typesupports);
  m_env.add_callback("value_to_py", 2, value_to_py);
  m_env.add_callback("get_rosidl_parser_type", 1, get_rosidl_parser_type);
  m_env.add_callback("get_special_nested_basic_type", 1, get_special_nested_basic_type);
  m_env.add_callback("get_python_type", 1, get_python_type);
  m_env.add_callback("get_bound", 1, get_bound);
  m_env.add_callback("primitive_msg_type_to_c", 1, primitive_msg_type_to_c);
  m_env.add_callback("is_python_builtin", 1, is_python_builtin);
}

void GeneratorPython::run() {
  // Load templates
  inja::Template template_idl_py = m_env.parse_template("./_idl.py.template");
  inja::Template template_idl_support_c =
      m_env.parse_template("./_idl_support.c.template");
  inja::Template template_idl_pkg_typesupport =
      m_env.parse_template("./_idl_pkg_typesupport_entry_point.c.template");
  inja::Template template_init =
      m_env.parse("{% for import in imports %}{{ import }}\n{% endfor %}");

  // Combined ros_json
  nlohmann::json pkg_json;
  pkg_json["package_name"] = m_arguments.package_name;
  pkg_json["messages"] = nlohmann::json::array();
  pkg_json["services"] = nlohmann::json::array();
  pkg_json["actions"] = nlohmann::json::array();

  // __init__.py file
  std::unordered_map<std::string, std::vector<std::string>> init_py;

  // Generate message specific files
  for (const auto& [path, file_path] : m_arguments.idl_tuples) {
    // std::cout << "Processing " << file_path << std::endl;

    const auto full_path = path + "/" + file_path;

    const auto idl_json = rosidlcpp_parser::parse_idl_file(full_path);
    // TODO: Save the result to an output file for debugging

    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(idl_json, file_path);
    // TODO: Save the result to an output file for debugging

    ros_json["package_name"] = m_arguments.package_name;

    const auto msg_directory = ros_json["interface_path"]["filedir"].get<std::string>();
    const auto msg_type = ros_json["interface_path"]["filename"].get<std::string>();
    m_env.write(template_idl_py, ros_json,
                std::format("{}/_{}.py", msg_directory,
                            rosidlcpp_core::camel_to_snake(msg_type)));
    m_env.write(template_idl_support_c, ros_json,
                std::format("{}/_{}_s.c", msg_directory,
                            rosidlcpp_core::camel_to_snake(msg_type)));

    // Add to the combined ros_json
    for (auto msg : ros_json.value("messages", nlohmann::json::array())) {
      pkg_json["messages"].push_back(msg);
    }
    for (auto srv : ros_json.value("services", nlohmann::json::array())) {
      pkg_json["services"].push_back(srv);
    }
    for (auto action : ros_json.value("actions", nlohmann::json::array())) {
      pkg_json["actions"].push_back(action);
    }

    // Add to __init__.py
    std::vector<std::string> type_suffixes{{""}};
    if (msg_directory == "srv") {
      type_suffixes.emplace_back(SERVICE_EVENT_MESSAGE_SUFFIX);
      type_suffixes.emplace_back(SERVICE_REQUEST_MESSAGE_SUFFIX);
      type_suffixes.emplace_back(SERVICE_RESPONSE_MESSAGE_SUFFIX);
    }
    if (msg_directory == "action") {
      type_suffixes.emplace_back(std::string{"_GetResult"} + std::string{SERVICE_EVENT_MESSAGE_SUFFIX});
      type_suffixes.emplace_back(std::string{"_GetResult"} + std::string{SERVICE_REQUEST_MESSAGE_SUFFIX});
      type_suffixes.emplace_back(std::string{"_GetResult"} + std::string{SERVICE_RESPONSE_MESSAGE_SUFFIX});
      type_suffixes.emplace_back(std::string{"_SendGoal"} + std::string{SERVICE_EVENT_MESSAGE_SUFFIX});
      type_suffixes.emplace_back(std::string{"_SendGoal"} + std::string{SERVICE_REQUEST_MESSAGE_SUFFIX});
      type_suffixes.emplace_back(std::string{"_SendGoal"} + std::string{SERVICE_RESPONSE_MESSAGE_SUFFIX});
    }

    for (const auto& type_suffix : type_suffixes) {
      init_py[msg_directory].push_back(std::format(
          "from {}.{}._{} import {}{}  # noqa: F401", m_arguments.package_name,
          msg_directory, rosidlcpp_core::camel_to_snake(msg_type), msg_type,
          type_suffix));
    }
  }

  // Generate package files
  for (const auto& typesupport : m_typesupport_implementations) {
    pkg_json["typesupport_impl"] = typesupport;
    m_env.write(
        template_idl_pkg_typesupport, pkg_json,
        std::format("_{}_s.ep.{}.c", m_arguments.package_name, typesupport));
  }

  // Generate __init__.py
  for (const auto& [msg_directory, imports] : init_py) {
    nlohmann::json init_py_json;
    init_py_json["imports"] = imports;
    m_env.write(template_init, init_py_json,
                std::format("{}/__init__.py", msg_directory));
  }
}

int main(int argc, char** argv) {
  GeneratorPython generator(argc, argv);
  generator.run();
  return 0;
}