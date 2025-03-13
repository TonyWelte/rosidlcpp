#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorTypesupportFastrtpsCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportFastrtpsCpp(const rosidlcpp_core::GeneratorArguments& generator_arguments);

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};