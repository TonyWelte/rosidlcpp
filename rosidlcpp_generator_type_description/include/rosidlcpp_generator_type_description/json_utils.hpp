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

#include <ostream>
#include <string>

#include <nlohmann/json_fwd.hpp>

struct dump_parameters {
  const std::string object_start = "{";
  const std::string object_end = "}";
  const std::string object_key_sep = ": ";
  const std::string object_value_sep = ",";
  const std::string object_empty = "";
  const unsigned int object_newline_indent = 3;

  const std::string array_start = "[";
  const std::string array_end = "]";
  const std::string array_sep = ", ";
  const std::string array_empty = "";
  const unsigned int array_newline_indent = 3;

  unsigned int current_indent = 0;
};

const dump_parameters compact = {"{", "}", ":", ",", "", 0,
                                 "[", "]", ",", "", 0, 0};
const dump_parameters pretty = {"{", "}", ": ", ",", "", 3,
                                "[", "]", ", ", "", 3, 0};
const dump_parameters array_oneliner = {"{", "}", ": ", ",", "", 3,
                                        "[", "]", ", ", "", 0, 0};

void dump(const nlohmann::ordered_json &data, std::ostream &o, const dump_parameters &param);

std::string dump(const nlohmann::ordered_json &data, const dump_parameters &param);
