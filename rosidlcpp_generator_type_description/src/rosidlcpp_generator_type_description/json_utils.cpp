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

#include <rosidlcpp_generator_type_description/json_utils.hpp>

#include <cassert>
#include <cstdint>
#include <cwctype>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/detail/value_t.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

using string_t = nlohmann::ordered_json::string_t;

std::string escape_string(std::string_view str) {
  std::string escaped_str;
  for (const auto &c : str) {
    switch (c) {
      case '"':
        escaped_str += R"(\")";
        break;
      case '\\':
        escaped_str += "\\\\";
        break;
      default:
        escaped_str += c;
    }
  }
  return escaped_str;
}

// Largly based on
// https://github.com/nlohmann/json/issues/229#issuecomment-217742892

std::string dump(const nlohmann::ordered_json &data, const dump_parameters &param) {
  std::stringstream ss;
  dump(data, ss, param);
  return ss.str();
}

void dump(const nlohmann::ordered_json &data, std::ostream &o, const dump_parameters &param) {
  // variable to hold indentation for recursive calls
  auto new_indent = param.current_indent;

  switch (data.type()) {
    case nlohmann::ordered_json::value_t::object: {
      o << param.object_start;

      // increase indentation
      if (param.object_newline_indent > 0) {
        new_indent += param.object_newline_indent;
        o << "\n"
          << string_t(new_indent, ' ');
      }

      for (auto i = data.cbegin(); i != data.cend();
           ++i) {
        if (i != data.cbegin()) {
          o << param.object_value_sep;
          if (param.object_newline_indent > 0) {
            o << "\n"
              << string_t(new_indent, ' ');
          }
        }
        o << "\"" << escape_string(i.key()) << "\"" << param.object_key_sep;
        auto new_param = param;
        new_param.current_indent = new_indent;
        dump(i.value(), o, new_param);
      }

      if (data.empty()) {
        o << param.object_empty;
      }

      // decrease indentation
      if (param.object_newline_indent > 0) {
        new_indent -= param.object_newline_indent;
        o << "\n"
          << string_t(new_indent, ' ');
      }

      o << param.object_end;
      return;
    }

    case nlohmann::ordered_json::value_t::array: {
      o << param.array_start;

      // increase indentation
      if (!data.empty() && param.array_newline_indent > 0) {
        new_indent += param.array_newline_indent;
        o << "\n"
          << string_t(new_indent, ' ');
      }

      for (auto i = data.cbegin(); i != data.cend(); ++i) {
        if (i != data.cbegin()) {
          o << param.array_sep;
          if (param.array_newline_indent > 0) {
            o << "\n"
              << string_t(new_indent, ' ');
          }
        }
        auto new_param = param;
        new_param.current_indent = new_indent;
        dump(*i, o, new_param);
      }

      if (data.empty()) {
        o << param.array_empty;
      }

      // decrease indentation
      if (!data.empty() && param.array_newline_indent > 0) {
        new_indent -= param.array_newline_indent;
        o << "\n"
          << string_t(new_indent, ' ');
      }

      o << param.array_end;
      return;
    }

    default: {
      o << data.dump(-1, ' ', true);
      return;
    }
  }
}