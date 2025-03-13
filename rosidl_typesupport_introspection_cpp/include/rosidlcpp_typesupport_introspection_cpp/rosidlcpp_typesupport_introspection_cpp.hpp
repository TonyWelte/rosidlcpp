#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorTypesupportIntrospectionCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportIntrospectionCpp(rosidlcpp_core::GeneratorArguments generator_arguments);

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};
