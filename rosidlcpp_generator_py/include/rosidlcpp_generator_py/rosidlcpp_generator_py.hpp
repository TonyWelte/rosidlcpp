#pragma once

#include "inja/inja.hpp"

#include <rosidlcpp_generator_core/generator_base.hpp>

#include <memory>

#include <nlohmann/json.hpp>

class GeneratorPython : public rosidlcpp_core::GeneratorBase {
 public:
  GeneratorPython(int argc, char** argv);
  virtual ~GeneratorPython() = default;

  void run();

 private:
  rosidlcpp_core::GeneratorArguments m_arguments;
  std::vector<std::string> m_typesupport_implementations;

  std::unique_ptr<inja::Environment> p_env;

  nlohmann::json m_global_storage;
};