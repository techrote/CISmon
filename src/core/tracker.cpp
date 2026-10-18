#include "cismon/tracker.hpp"

#include <algorithm>

namespace cismon {

TrackedRun& RunTracker::upsert(const RunSnapshot& snapshot, std::uint64_t nowMs) {
  if (auto* existing = find(snapshot.repository, snapshot.runId)) {
    const bool wasTerminal = isTerminal(existing->snapshot.state);
    const bool nowTerminal = isTerminal(snapshot.state);
    RunSnapshot merged = snapshot;
    if (merged.jobs.empty() && !existing->snapshot.jobs.empty()) merged.jobs = existing->snapshot.jobs;
    existing->snapshot = std::move(merged);
    existing->lastSeenMs = nowMs;
    if (!wasTerminal && nowTerminal) existing->terminalSinceMs = nowMs;
    return *existing;
  }

  TrackedRun run;
  run.snapshot = snapshot;
  run.firstSeenMs = nowMs;
  run.lastSeenMs = nowMs;
  if (isTerminal(snapshot.state)) run.terminalSinceMs = nowMs;
  runs_.push_back(std::move(run));
  return runs_.back();
}

TrackedRun* RunTracker::find(const std::string& repository, std::uint64_t runId) {
  for (auto& run : runs_) {
    if (run.snapshot.repository == repository && run.snapshot.runId == runId) return &run;
  }
  return nullptr;
}

const TrackedRun* RunTracker::find(const std::string& repository, std::uint64_t runId) const {
  for (const auto& run : runs_) {
    if (run.snapshot.repository == repository && run.snapshot.runId == runId) return &run;
  }
  return nullptr;
}

bool RunTracker::acknowledge(const std::string& repository, std::uint64_t runId) {
  if (auto* run = find(repository, runId)) {
    run->acknowledged = true;
    return true;
  }
  return false;
}

bool RunTracker::setManualWatch(const std::string& repository, std::uint64_t runId, bool watched) {
  if (auto* run = find(repository, runId)) {
    run->manualWatch = watched;
    return true;
  }
  return false;
}

std::vector<const TrackedRun*> RunTracker::visible(std::uint64_t nowMs,
                                                   std::uint64_t successTtlMs) const {
  std::vector<const TrackedRun*> result;
  for (const auto& run : runs_) {
    const bool active = run.snapshot.state == RunState::Queued ||
                        run.snapshot.state == RunState::InProgress;
    const bool failure = needsAttention(run.snapshot.conclusion);
    const bool recentSuccess = run.snapshot.conclusion == Conclusion::Success &&
        run.terminalSinceMs != 0 && nowMs - run.terminalSinceMs <= successTtlMs;
    if (run.manualWatch || active || (failure && !run.acknowledged) || recentSuccess) {
      result.push_back(&run);
    }
  }
  return result;
}

void RunTracker::prune(std::uint64_t nowMs, std::uint64_t successTtlMs,
                       std::uint64_t terminalTtlMs) {
  runs_.erase(std::remove_if(runs_.begin(), runs_.end(), [&](const TrackedRun& run) {
    if (!isTerminal(run.snapshot.state) || run.manualWatch) return false;
    if (needsAttention(run.snapshot.conclusion) && !run.acknowledged) return false;
    const std::uint64_t ttl = run.snapshot.conclusion == Conclusion::Success
                                  ? successTtlMs
                                  : terminalTtlMs;
    return run.terminalSinceMs != 0 && nowMs - run.terminalSinceMs > ttl;
  }), runs_.end());
}

}  // namespace cismon
