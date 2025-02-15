#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <string>
#include <vector>

class GeneratorTypesupportCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportCpp(int argc, char** argv);
  virtual ~GeneratorTypesupportCpp() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  std::vector<std::string> m_typesupport_implementations;
};