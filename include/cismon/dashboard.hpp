#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cismon/analyzer.hpp"
#include "cismon/tracker.hpp"

namespace cismon {

struct DashboardItem {
  std::string repository;
  std::string workflow;
  std::string stage;
  std::uint64_t runId = 0;
  std::uint32_t runNumber = 0;
  RunState state = RunState::Unknown;
  Conclusion conclusion = Conclusion::Unknown;
  AttentionLevel attention = AttentionLevel::Normal;
  Confidence confidence = Confidence::None;
  std::uint64_t elapsedMs = 0;
  std::uint64_t baselineMedianMs = 0;
  std::uint64_t estimatedRemainingMs = 0;
  double ratio = 0;
  bool possiblyStalled = false;
  bool manualWatch = false;
  bool acknowledged = false;
};

std::vector<DashboardItem> buildDashboard(const RunTracker& tracker,
                                          const RuntimeAnalyzer& analyzer,
                                          std::uint64_t nowMs,
                                          std::uint64_t nowEpoch,
                                          std::size_t limit = 5);

}  // namespace cismon
