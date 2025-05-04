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

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorC : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorC(rosidlcpp_core::GeneratorArguments generator_arguments, bool disable_description_codegen);

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  bool m_disable_description_codegen;
};