#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "cismon/analyzer.hpp"
#include "cismon/dashboard.hpp"
#include "cismon/glob.hpp"
#include "cismon/poll_policy.hpp"
#include "cismon/tracker.hpp"

using namespace cismon;

class MemoryHistory final : public IHistoryStore {
 public:
  std::unordered_map<std::uint64_t, std::vector<StageSample>> byKey;

  std::vector<StageSample> samples(std::uint64_t keyHash, std::size_t limit) const override {
    auto it = byKey.find(keyHash);
    if (it == byKey.end()) return {};
    auto result = it->second;
    if (result.size() > limit) result.erase(result.begin(), result.end() - limit);
    return result;
  }
  bool hasRun(const std::string&, std::uint64_t) const override { return false; }
  bool recordRun(const RunSnapshot&) override { return true; }
};

static void addSuccess(MemoryHistory& h, const BaselineKey& key, std::uint64_t ms) {
  h.byKey[key.hash()].push_back({1, key.hash(), ms, true, 1});
}

int main() {
  const auto stats = robustStats({1000, 1100, 1200, 1300, 50000});
  assert(stats.samples == 5);
  assert(stats.median == 1200);
  assert(stats.mad == 100);
  assert(stats.maximum == 50000);

  MemoryHistory history;
  RunSnapshot run;
  run.repository = "techrote/CISmon";
  run.workflowId = 42;
  run.runId = 100;
  run.event = "pull_request";
  run.state = RunState::InProgress;
  run.startedAtEpoch = 1000;
  const auto key = BaselineKey::forRun(run);
  for (auto ms : {9000ULL, 10000ULL, 10500ULL, 11000ULL, 9500ULL, 10000ULL}) addSuccess(history, key, ms);

  RuntimeAnalyzer analyzer(history);
  auto normal = analyzer.analyze(key, 8000, false, Conclusion::Unknown);
  assert(normal.attention == AttentionLevel::Normal);
  assert(normal.estimatedRemainingMs > 0);

  auto suspicious = analyzer.analyze(key, 30000, false, Conclusion::Unknown);
  assert(suspicious.attention == AttentionLevel::Suspicious);
  assert(suspicious.possiblyStalled);

  auto failed = analyzer.analyze(key, 5000, true, Conclusion::Failure);
  assert(failed.attention == AttentionLevel::ActionRequired);

  RunTracker tracker;
  tracker.upsert(run, 100);
  assert(tracker.visible(100).size() == 1);
  run.state = RunState::Completed;
  run.conclusion = Conclusion::Failure;
  tracker.upsert(run, 200);
  assert(tracker.visible(201).size() == 1);
  assert(tracker.acknowledge(run.repository, run.runId));
  assert(tracker.visible(201).empty());

  RunTracker dashboardTracker;
  run.state = RunState::InProgress;
  run.conclusion = Conclusion::Unknown;
  run.startedAtEpoch = 1000;
  dashboardTracker.upsert(run, 100);
  auto dashboard = buildDashboard(dashboardTracker, analyzer, 30100, 1030, 5);
  assert(dashboard.size() == 1);
  assert(dashboard.front().attention == AttentionLevel::Suspicious);

  PollPolicy policy;
  assert(policy.nextInterval(true, false, 0, 0) == policy.activeMs);
  assert(policy.nextInterval(false, true, 0, 0) == policy.queuedMs);
  assert(policy.nextInterval(false, false, policy.warmWindowMs * 10, 0) == policy.coldMs);
  assert(policy.nextInterval(false, false, 0, 3) == policy.errorBaseMs * 4);

  assert(globMatch("techrote/*", "techrote/CISmon"));
  assert(globMatch("*Doom*", "techrote/ShadeDoomVK"));
  assert(!globMatch("foo/*", "techrote/CISmon"));

  std::cout << "CISmon portable core tests passed\n";
  return 0;
}
