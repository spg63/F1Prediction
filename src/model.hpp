#pragma once
#include "types.hpp"

namespace f1 {
Forecast forecast(const Data& data, int season, int through_round,
                  const std::string& cutoff, int simulations = 10000,
                  unsigned seed = 42, const Config& config = {});
}
