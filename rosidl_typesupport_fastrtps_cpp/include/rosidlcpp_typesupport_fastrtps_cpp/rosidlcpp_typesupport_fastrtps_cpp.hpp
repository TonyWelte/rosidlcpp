#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <string>
#include <vector>

class GeneratorTypesupportFastrtpsCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportFastrtpsCpp(int argc, char** argv);
  virtual ~GeneratorTypesupportFastrtpsCpp() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};