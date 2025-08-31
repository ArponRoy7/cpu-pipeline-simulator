#include "predictor_factory.hpp"
#include "predictor.hpp"   // brings in StaticPredictor, OneBitPredictor, etc.
#include <algorithm>
#include <cctype>          // for std::tolower

std::unique_ptr<BranchPredictor> make_predictor(const std::string& raw) {
    std::string name = raw;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c){ return std::tolower(static_cast<unsigned char>(c)); });

    if (name == "static_nt") return std::make_unique<StaticPredictor>(false);
    if (name == "static_t")  return std::make_unique<StaticPredictor>(true);
    if (name == "1bit")      return std::make_unique<OneBitPredictor>();
    if (name == "2bit")      return std::make_unique<TwoBitPredictor>();
    if (name == "tournament")return std::make_unique<TournamentPredictor>();

    // default fallback
    return std::make_unique<StaticPredictor>(false);
}
