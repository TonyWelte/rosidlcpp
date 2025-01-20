#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorC : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorC(int argc, char** argv);
  virtual ~GeneratorC() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  bool m_disable_description_codegen;
};