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

#include <nlohmann/json.hpp>

#include <rosidlcpp_generator_core/generator_utils.hpp>

using nlohmann::json;

// Tests for format_as(const json&)

TEST(FormatAsJsonTest, StringValueIsNotQuoted) {
  json j = "hello";
  EXPECT_EQ(nlohmann::format_as(j), "hello");
}

TEST(FormatAsJsonTest, StringValueWithSpecialCharacters) {
  json j = "msg/UUID";
  EXPECT_EQ(nlohmann::format_as(j), "msg/UUID");
}

TEST(FormatAsJsonTest, IntegerValueIsDumped) {
  json j = 42;
  EXPECT_EQ(nlohmann::format_as(j), "42");
}

TEST(FormatAsJsonTest, BoolValueIsDumped) {
  json j = true;
  EXPECT_EQ(nlohmann::format_as(j), "true");
}

TEST(FormatAsJsonTest, NullValueIsDumped) {
  json j = nullptr;
  EXPECT_EQ(nlohmann::format_as(j), "null");
}

TEST(FormatAsJsonTest, ArrayValueIsDumped) {
  json j = json::array({1, 2, 3});
  EXPECT_EQ(nlohmann::format_as(j), "[1,2,3]");
}

TEST(FormatAsJsonTest, ObjectValueIsDumped) {
  json j = {{"key", "value"}};
  EXPECT_EQ(nlohmann::format_as(j), "{\"key\":\"value\"}");
}

// Tests for format_as(const detail::iter_impl<const json>&)

TEST(FormatAsIteratorTest, StringValueIsNotQuoted) {
  json obj = {{"name", "msg"}};
  auto it = obj.cbegin();
  EXPECT_EQ(nlohmann::format_as(it), "msg");
}

TEST(FormatAsIteratorTest, IntegerValueIsDumped) {
  json obj = {{"count", 7}};
  auto it = obj.cbegin();
  EXPECT_EQ(nlohmann::format_as(it), "7");
}

TEST(FormatAsIteratorTest, ArrayElementStringNotQuoted) {
  json arr = json::array({"foo", "bar"});
  auto it = arr.cbegin();
  EXPECT_EQ(nlohmann::format_as(it), "foo");
  ++it;
  EXPECT_EQ(nlohmann::format_as(it), "bar");
}
