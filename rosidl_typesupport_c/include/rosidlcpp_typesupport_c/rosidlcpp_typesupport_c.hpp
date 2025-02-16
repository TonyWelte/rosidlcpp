#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <string>
#include <vector>

class GeneratorTypesupportC : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportC(int argc, char** argv);
  virtual ~GeneratorTypesupportC() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  std::vector<std::string> m_typesupport_implementations;
};
