#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cismon {

struct RobustStats {
  std::size_t samples = 0;
  double minimum = 0;
  double p25 = 0;
  double median = 0;
  double p75 = 0;
  double maximum = 0;
  double mad = 0;
  double ewma = 0;
};

RobustStats robustStats(const std::vector<std::uint64_t>& values);
double robustZ(double value, const RobustStats& stats, double madFloor = 1000.0);

}  // namespace cismon
