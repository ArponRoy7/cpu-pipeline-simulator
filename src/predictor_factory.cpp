#include "predictor_factory.hpp"
#include <algorithm>

std::unique_ptr<BranchPredictor> make_predictor(const std::string& raw) {
    std::string name = raw;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (name == "static_nt") return std::make_unique<StaticPredictor>(false);
    if (name == "static_t")  return std::make_unique<StaticPredictor>(true);
    if (name == "1bit")      return std::make_unique<OneBitPredictor>();
    if (name == "2bit")      return std::make_unique<TwoBitPredictor>();

    // simple tournament: prefer 2bit over 1bit by accuracy, but we’ll just return 2bit for now
    if (name == "tournament") return std::make_unique<TwoBitPredictor>();

    // default
    return std::make_unique<StaticPredictor>(false);
}
