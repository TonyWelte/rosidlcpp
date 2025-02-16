#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorTypesupportIntrospectionC : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportIntrospectionC(int argc, char** argv);
  virtual ~GeneratorTypesupportIntrospectionC() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};
