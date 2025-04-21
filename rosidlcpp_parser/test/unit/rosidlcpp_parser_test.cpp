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

#include <gtest/gtest.h>

#include <nlohmann/json_fwd.hpp>
#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace rosidlcpp_parser;

TEST(RosIdlParserTest, RemoveWhiteSpace) {
  std::string_view test_string_1 = "   a";
  std::string_view test_string_2 = "\ta";
  std::string_view test_string_3 = "  \ta";
  std::string_view test_string_4 = "b  \ta";

  consume_white_space(test_string_1);
  EXPECT_EQ(test_string_1, "a");

  consume_white_space(test_string_2);
  EXPECT_EQ(test_string_2, "a");

  consume_white_space(test_string_3);
  EXPECT_EQ(test_string_3, "a");

  consume_white_space(test_string_4);
  EXPECT_EQ(test_string_4, "b  \ta");
}

TEST(RosIdlParserTest, RemoveComment) {
  std::string_view test_string_1 = "// This is a test comment";
  std::string_view test_string_2 = "// This is a test comment\n// This is the next line";
  std::string_view test_string_3 = "This is not a comment";

  consume_comment(test_string_1);
  EXPECT_EQ(test_string_1, "");

  consume_comment(test_string_2);
  EXPECT_EQ(test_string_2, "// This is the next line");

  consume_comment(test_string_3);
  EXPECT_EQ(test_string_3, "This is not a comment");
}

TEST(RosIdlParserTest, RemoveWhiteSpaceAndComment) {
  std::string_view test_string_1 = "  \t// This is a one line comment";
  std::string_view test_string_2 = "  \t// This is another one line comment\nThis is not a comment";
  std::string_view test_string_3 = "  // This is a multiline comment\n// This is the next line\nThis is not a comment";

  consume_white_space_and_comment(test_string_1);
  EXPECT_EQ(test_string_1, "");

  consume_white_space_and_comment(test_string_2);
  EXPECT_EQ(test_string_2, "This is not a comment");

  consume_white_space_and_comment(test_string_3);
  EXPECT_EQ(test_string_3, "This is not a comment");
}

TEST(RosIdlParserTest, ParseInclude) {
  std::string_view test_string_1 = "#include \"test_include.h\"";
  std::string_view test_string_2 = "#include \"test_include.h\"  // This is a comment";
  std::string_view test_string_3 = "#include\"test_include.h\"";
  std::string_view test_string_4 = "#include";

  EXPECT_EQ(parse_include(test_string_1), "test_include.h");
  EXPECT_EQ(test_string_1, "");

  EXPECT_EQ(parse_include(test_string_2), "test_include.h");
  EXPECT_EQ(test_string_2, "");

  EXPECT_THROW(parse_include(test_string_3), std::runtime_error);
  EXPECT_THROW(parse_include(test_string_4), std::runtime_error);
}

TEST(RosIdlParserTest, ParseName) {
  std::string_view test_string_1 = "";
  std::string_view test_string_2 = "abcABC123_";
  std::string_view test_string_3 = "abcABC123_   ";
  std::string_view test_string_4 = "abcAB-C123_";

  EXPECT_EQ(parse_name(test_string_1), "");
  EXPECT_EQ(parse_name(test_string_2), "abcABC123_");
  EXPECT_EQ(parse_name(test_string_3), "abcABC123_");
  EXPECT_EQ(parse_name(test_string_4), "abcAB");
  EXPECT_EQ(test_string_4, "-C123_");
}

TEST(RosIdlParserTest, ParseStructure) {
  std::string_view test_string_1 = "struct EmptyStruct {};// After struct";
  std::string_view test_string_2 = "struct EmptyStructWithSpace   \n\t  {\n\n\t   };// After struct";

  auto struct_result_1 = parse_structure(test_string_1);
  ASSERT_TRUE(struct_result_1.contains("name"));
  EXPECT_EQ(struct_result_1["name"], "EmptyStruct");
  EXPECT_EQ(test_string_1, "");

  auto struct_result_2 = parse_structure(test_string_2);
  ASSERT_TRUE(struct_result_2.contains("name"));
  EXPECT_EQ(struct_result_2["name"], "EmptyStructWithSpace");
  EXPECT_EQ(test_string_2, "");
}

TEST(RosIdlParserTest, ParseValueString) {
  std::string_view test_string_1 = "\"This is a test string\"Unparsed data";
  std::string_view test_string_2 = "\"This is a test string\\\"with escaped elements\"Unparsed data";
  std::string_view test_string_3 = "\"This is a test string\\\"with multiple\\\" escaped elements\"Unparsed data";

  EXPECT_EQ(parse_string(test_string_1), "This is a test string");
  EXPECT_EQ(parse_string(test_string_2), "This is a test string\"with escaped elements");
  EXPECT_EQ(parse_string(test_string_3), "This is a test string\"with multiple\" escaped elements");
}

TEST(RosIdlParserTest, ParseTypedef) {
  std::string_view test_string_1 = "typedef uint8 other_name;";
  std::string_view test_string_2 = "typedef uint8 other_name;\n";

  std::pair<std::string, std::string> result_1 = {"other_name", "uint8"};
  std::pair<std::string, std::string> result_2 = {"other_name", "uint8"};

  EXPECT_EQ(parse_typedef(test_string_1), result_1);
  EXPECT_EQ(parse_typedef(test_string_2), result_2);
}

TEST(RosIdlParserTest, ParseAttribute) {
  std::string_view test_string_1 = "@key long key;";
  std::string_view test_string_2 = "@default (value=1.23)\n";
  std::string_view test_string_3 = "@verbatim (language=\"comment\", text=\"My comment\")\n";

  nlohmann::json result_1 = {{"name", "key"}};
  nlohmann::json result_2 = {{"name", "default"}, {"content", {{"value", 1.23}}}};
  nlohmann::json result_3 = {{"name", "verbatim"}, {"content", {{"language", "comment"}, {"text", "My comment"}}}};

  EXPECT_EQ(parse_attribute(test_string_1), result_1);
  EXPECT_EQ(parse_attribute(test_string_2), result_2);
  EXPECT_EQ(parse_attribute(test_string_3), result_3);
}

TEST(RosIdlParserTest, ParseModule) {
  std::string_view test_string_1 = "module my_module { }";
  std::string_view test_string_2 = "module my_module { typedef uint8 other_name; }";
  std::string_view test_string_3 = "modulemy_module { typedef uint8 other_name; }";
  std::string_view test_string_4 = "module {  }";

  nlohmann::json result_1 = {{"name", "my_module"}};
  nlohmann::json result_2 = {{"name", "my_module"}};

  EXPECT_EQ(parse_module(test_string_1), result_1);
  EXPECT_EQ(parse_module(test_string_2), result_2);

  EXPECT_THROW(parse_module(test_string_3), std::runtime_error);
  EXPECT_THROW(parse_module(test_string_4), std::runtime_error);
}
