#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <string>
#include <vector>

class GeneratorPython : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorPython(rosidlcpp_core::GeneratorArguments generator_arguments, std::vector<std::string> typesupport_implementations_list);
  virtual ~GeneratorPython() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  std::vector<std::string> m_typesupport_implementations;
};