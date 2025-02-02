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
