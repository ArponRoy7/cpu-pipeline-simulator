#pragma once
#include <memory>
#include <string>
#include "predictor.hpp"   // for BranchPredictor base

// Factory function declaration
std::unique_ptr<BranchPredictor> make_predictor(const std::string& name);
