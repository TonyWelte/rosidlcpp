#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorCpp(rosidlcpp_core::GeneratorArguments generator_arguments);

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};