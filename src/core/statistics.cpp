#include "cismon/statistics.hpp"

#include <algorithm>
#include <cmath>

namespace cismon {
namespace {
double percentile(const std::vector<std::uint64_t>& sorted, double q) {
  if (sorted.empty()) return 0;
  if (sorted.size() == 1) return static_cast<double>(sorted.front());
  const double position = q * static_cast<double>(sorted.size() - 1);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = static_cast<std::size_t>(std::ceil(position));
  if (lower == upper) return static_cast<double>(sorted[lower]);
  const double fraction = position - static_cast<double>(lower);
  return static_cast<double>(sorted[lower]) * (1.0 - fraction) +
         static_cast<double>(sorted[upper]) * fraction;
}
}  // namespace

RobustStats robustStats(const std::vector<std::uint64_t>& values) {
  RobustStats result;
  if (values.empty()) return result;

  std::vector<std::uint64_t> sorted = values;
  std::sort(sorted.begin(), sorted.end());
  result.samples = sorted.size();
  result.minimum = static_cast<double>(sorted.front());
  result.p25 = percentile(sorted, 0.25);
  result.median = percentile(sorted, 0.50);
  result.p75 = percentile(sorted, 0.75);
  result.maximum = static_cast<double>(sorted.back());

  std::vector<std::uint64_t> deviations;
  deviations.reserve(sorted.size());
  for (const auto v : sorted) {
    deviations.push_back(static_cast<std::uint64_t>(std::llround(std::abs(static_cast<double>(v) - result.median))));
  }
  std::sort(deviations.begin(), deviations.end());
  result.mad = percentile(deviations, 0.50);

  constexpr double alpha = 0.30;
  double ewma = static_cast<double>(values.front());
  for (std::size_t i = 1; i < values.size(); ++i) {
    ewma = alpha * static_cast<double>(values[i]) + (1.0 - alpha) * ewma;
  }
  result.ewma = ewma;
  return result;
}

double robustZ(double value, const RobustStats& stats, double madFloor) {
  if (stats.samples == 0) return 0;
  const double scale = 1.4826 * std::max(stats.mad, madFloor);
  return scale > 0 ? (value - stats.median) / scale : 0;
}

}  // namespace cismon
