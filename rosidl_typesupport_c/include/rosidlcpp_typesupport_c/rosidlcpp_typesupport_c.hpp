#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <string>
#include <vector>

class GeneratorTypesupportC : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportC(rosidlcpp_core::GeneratorArguments generator_arguments, std::vector<std::string> typesupport_implementations_list);

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  std::vector<std::string> m_typesupport_implementations;
};
