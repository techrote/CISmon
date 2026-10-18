#include "cismon/analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cismon {
namespace {
Confidence confidenceFor(std::size_t n) {
  if (n == 0) return Confidence::None;
  if (n < 5) return Confidence::Low;
  if (n < 16) return Confidence::Medium;
  return Confidence::High;
}
}  // namespace

const char* toString(Confidence confidence) {
  switch (confidence) {
    case Confidence::Low: return "low";
    case Confidence::Medium: return "medium";
    case Confidence::High: return "high";
    default: return "none";
  }
}

std::uint64_t elapsedMs(std::uint64_t startedEpoch, std::uint64_t completedEpoch,
                        std::uint64_t nowEpoch) {
  if (startedEpoch == 0) return 0;
  const std::uint64_t end = completedEpoch != 0 ? completedEpoch : nowEpoch;
  if (end <= startedEpoch) return 0;
  return (end - startedEpoch) * 1000ULL;
}

AnalysisResult RuntimeAnalyzer::analyze(const BaselineKey& key, std::uint64_t elapsed,
                                        bool completed, Conclusion conclusion) const {
  AnalysisResult result;
  result.elapsedMs = elapsed;

  if (needsAttention(conclusion)) {
    result.attention = AttentionLevel::ActionRequired;
    result.reason = "failed";
  }

  const auto stored = history_.samples(key.hash(), sampleWindow_);
  std::vector<std::uint64_t> durations;
  durations.reserve(stored.size());
  for (const auto& sample : stored) {
    if (sample.successful && sample.durationMs > 0) durations.push_back(sample.durationMs);
  }
  result.baseline = robustStats(durations);
  result.confidence = confidenceFor(result.baseline.samples);
  if (result.baseline.samples == 0 || result.baseline.median <= 0) return result;

  result.deltaMs = static_cast<std::int64_t>(elapsed) -
                   static_cast<std::int64_t>(std::llround(result.baseline.median));
  result.ratio = static_cast<double>(elapsed) / result.baseline.median;
  result.z = robustZ(static_cast<double>(elapsed), result.baseline);

  if (!completed && elapsed < static_cast<std::uint64_t>(result.baseline.median)) {
    result.estimatedRemainingMs =
        static_cast<std::uint64_t>(result.baseline.median) - elapsed;
  }

  if (result.attention == AttentionLevel::ActionRequired) return result;

  if (result.baseline.samples < 3) {
    if (result.ratio >= 2.0) {
      result.attention = AttentionLevel::Interesting;
      result.reason = "well beyond sparse baseline";
    }
    return result;
  }

  if (result.ratio >= 3.0 || result.z >= 6.0) {
    result.attention = AttentionLevel::Suspicious;
    result.possiblyStalled = !completed;
    result.reason = result.possiblyStalled ? "possible stall" : "extreme runtime";
  } else if (result.ratio >= 1.75 || result.z >= 4.0) {
    result.attention = AttentionLevel::Suspicious;
    result.reason = "far slower than baseline";
  } else if (result.ratio >= 1.30 || result.z >= 2.5) {
    result.attention = AttentionLevel::Interesting;
    result.reason = "slower than baseline";
  }
  return result;
}

AnalysisResult RuntimeAnalyzer::analyzeRun(const RunSnapshot& run,
                                           std::uint64_t nowEpoch) const {
  return analyze(BaselineKey::forRun(run),
                 elapsedMs(run.startedAtEpoch,
                           run.state == RunState::Completed ? run.updatedAtEpoch : 0,
                           nowEpoch),
                 run.state == RunState::Completed, run.conclusion);
}

AnalysisResult RuntimeAnalyzer::analyzeJob(const RunSnapshot& run, const JobSnapshot& job,
                                           std::uint64_t nowEpoch) const {
  return analyze(BaselineKey::forJob(run, job),
                 elapsedMs(job.startedAtEpoch, job.completedAtEpoch, nowEpoch),
                 job.state == RunState::Completed, job.conclusion);
}

AnalysisResult RuntimeAnalyzer::analyzeStep(const RunSnapshot& run, const JobSnapshot& job,
                                            const StepSnapshot& step,
                                            std::uint64_t nowEpoch) const {
  return analyze(BaselineKey::forStep(run, job, step),
                 elapsedMs(step.startedAtEpoch, step.completedAtEpoch, nowEpoch),
                 step.state == RunState::Completed, step.conclusion);
}

}  // namespace cismon
