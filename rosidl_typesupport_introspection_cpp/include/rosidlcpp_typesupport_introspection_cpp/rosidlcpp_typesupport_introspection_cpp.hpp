#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorTypesupportIntrospectionCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportIntrospectionCpp(const rosidlcpp_core::GeneratorArguments& generator_arguments);
  virtual ~GeneratorTypesupportIntrospectionCpp() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};
