#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "cismon/history.hpp"
#include "cismon/statistics.hpp"

namespace cismon {

enum class Confidence : std::uint8_t { None, Low, Medium, High };

struct AnalysisResult {
  AttentionLevel attention = AttentionLevel::Normal;
  Confidence confidence = Confidence::None;
  RobustStats baseline;
  std::uint64_t elapsedMs = 0;
  std::int64_t deltaMs = 0;
  double ratio = 0;
  double z = 0;
  bool possiblyStalled = false;
  std::uint64_t estimatedRemainingMs = 0;
  std::string reason;
};

class RuntimeAnalyzer {
 public:
  explicit RuntimeAnalyzer(const IHistoryStore& history, std::size_t sampleWindow = 32)
      : history_(history), sampleWindow_(sampleWindow) {}

  AnalysisResult analyze(const BaselineKey& key, std::uint64_t elapsedMs,
                         bool completed, Conclusion conclusion) const;

  AnalysisResult analyzeRun(const RunSnapshot& run, std::uint64_t nowEpoch) const;
  AnalysisResult analyzeJob(const RunSnapshot& run, const JobSnapshot& job,
                            std::uint64_t nowEpoch) const;
  AnalysisResult analyzeStep(const RunSnapshot& run, const JobSnapshot& job,
                             const StepSnapshot& step, std::uint64_t nowEpoch) const;

 private:
  const IHistoryStore& history_;
  std::size_t sampleWindow_;
};

const char* toString(Confidence confidence);
std::uint64_t elapsedMs(std::uint64_t startedEpoch, std::uint64_t completedEpoch,
                        std::uint64_t nowEpoch);

}  // namespace cismon
