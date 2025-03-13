#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <string>
#include <vector>

class GeneratorTypesupportFastrtpsC : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorTypesupportFastrtpsC(const rosidlcpp_core::GeneratorArguments& generator_arguments);
  virtual ~GeneratorTypesupportFastrtpsC() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
};