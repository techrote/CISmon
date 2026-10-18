#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cismon/model.hpp"

namespace cismon {

struct TrackedRun {
  RunSnapshot snapshot;
  std::uint64_t firstSeenMs = 0;
  std::uint64_t lastSeenMs = 0;
  std::uint64_t terminalSinceMs = 0;
  bool acknowledged = false;
  bool manualWatch = false;
};

class RunTracker {
 public:
  TrackedRun& upsert(const RunSnapshot& snapshot, std::uint64_t nowMs);
  TrackedRun* find(const std::string& repository, std::uint64_t runId);
  const TrackedRun* find(const std::string& repository, std::uint64_t runId) const;
  bool acknowledge(const std::string& repository, std::uint64_t runId);
  bool setManualWatch(const std::string& repository, std::uint64_t runId, bool watched);

  std::vector<const TrackedRun*> visible(std::uint64_t nowMs,
                                         std::uint64_t successTtlMs = 3600000) const;
  void prune(std::uint64_t nowMs, std::uint64_t successTtlMs = 3600000,
             std::uint64_t terminalTtlMs = 86400000);

  const std::vector<TrackedRun>& all() const { return runs_; }

 private:
  std::vector<TrackedRun> runs_;
};

}  // namespace cismon
