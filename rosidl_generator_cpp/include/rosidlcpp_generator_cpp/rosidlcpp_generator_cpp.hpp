#pragma once

#include <rosidlcpp_generator_core/generator_base.hpp>

class GeneratorCpp : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorCpp(int argc, char** argv);
  virtual ~GeneratorCpp() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;

  std::string _output_path;
};