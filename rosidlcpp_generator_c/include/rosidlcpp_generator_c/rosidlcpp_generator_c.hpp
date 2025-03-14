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