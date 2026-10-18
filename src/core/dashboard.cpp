#include "cismon/dashboard.hpp"

#include <algorithm>

namespace cismon {
namespace {
int attentionRank(AttentionLevel level) {
  switch (level) {
    case AttentionLevel::ActionRequired: return 4;
    case AttentionLevel::Suspicious: return 3;
    case AttentionLevel::Interesting: return 2;
    default: return 1;
  }
}

void applyAnalysis(DashboardItem& item, const AnalysisResult& analysis) {
  if (attentionRank(analysis.attention) > attentionRank(item.attention))
    item.attention = analysis.attention;
  item.possiblyStalled = item.possiblyStalled || analysis.possiblyStalled;
  if (analysis.baseline.samples > 0) {
    item.confidence = analysis.confidence;
    item.baselineMedianMs = static_cast<std::uint64_t>(analysis.baseline.median);
    item.estimatedRemainingMs = analysis.estimatedRemainingMs;
    item.ratio = analysis.ratio;
  }
}

int score(const DashboardItem& item) {
  int result = attentionRank(item.attention) * 1000;
  if (item.state == RunState::InProgress) result += 300;
  else if (item.state == RunState::Queued) result += 200;
  if (item.manualWatch) result += 100;
  result += static_cast<int>(std::min<std::uint64_t>(item.elapsedMs / 60000ULL, 99ULL));
  return result;
}
}  // namespace

std::vector<DashboardItem> buildDashboard(const RunTracker& tracker,
                                          const RuntimeAnalyzer& analyzer,
                                          std::uint64_t nowMs,
                                          std::uint64_t nowEpoch,
                                          std::size_t limit) {
  std::vector<DashboardItem> result;
  for (const auto* tracked : tracker.visible(nowMs)) {
    const auto& run = tracked->snapshot;
    DashboardItem item;
    item.repository = run.repository;
    item.workflow = run.workflowName;
    item.runId = run.runId;
    item.runNumber = run.runNumber;
    item.state = run.state;
    item.conclusion = run.conclusion;
    item.manualWatch = tracked->manualWatch;
    item.acknowledged = tracked->acknowledged;

    AnalysisResult runAnalysis;
    if (nowEpoch > 1000000000ULL) {
      runAnalysis = analyzer.analyzeRun(run, nowEpoch);
      item.elapsedMs = runAnalysis.elapsedMs;
    } else {
      const std::uint64_t localElapsed = isTerminal(run.state) && tracked->terminalSinceMs
                                             ? tracked->terminalSinceMs - tracked->firstSeenMs
                                             : nowMs - tracked->firstSeenMs;
      runAnalysis = analyzer.analyze(BaselineKey::forRun(run), localElapsed,
                                     isTerminal(run.state), run.conclusion);
      item.elapsedMs = localElapsed;
    }
    applyAnalysis(item, runAnalysis);

    const JobSnapshot* currentJob = nullptr;
    const StepSnapshot* currentStep = nullptr;
    for (const auto& job : run.jobs) {
      if (job.state == RunState::InProgress) {
        currentJob = &job;
        break;
      }
    }
    if (!currentJob) {
      for (const auto& job : run.jobs) {
        if (job.state == RunState::Queued) {
          currentJob = &job;
          break;
        }
      }
    }

    if (currentJob) {
      item.stage = currentJob->name;
      for (const auto& step : currentJob->steps) {
        if (step.state == RunState::InProgress) {
          currentStep = &step;
          break;
        }
      }
      if (nowEpoch > 1000000000ULL) {
        if (currentStep) {
          item.stage = currentStep->name;
          applyAnalysis(item, analyzer.analyzeStep(run, *currentJob, *currentStep, nowEpoch));
        } else {
          applyAnalysis(item, analyzer.analyzeJob(run, *currentJob, nowEpoch));
        }
      }
    }

    result.push_back(std::move(item));
  }

  std::stable_sort(result.begin(), result.end(), [](const DashboardItem& a, const DashboardItem& b) {
    const int as = score(a), bs = score(b);
    if (as != bs) return as > bs;
    return a.elapsedMs > b.elapsedMs;
  });
  if (result.size() > limit) result.resize(limit);
  return result;
}

}  // namespace cismon
