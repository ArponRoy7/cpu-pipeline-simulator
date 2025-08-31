#pragma once
#include <memory>
#include <string>
#include "predictor.hpp"

std::unique_ptr<BranchPredictor> make_predictor(const std::string& name);
