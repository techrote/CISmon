#ifdef ARDUINO

#include "file_history_store.hpp"

#include <LittleFS.h>
#include <cstdio>

#include "cismon/analyzer.hpp"

namespace {
constexpr const char* kHistoryPath = "/history.csv";

std::uint64_t fnv1a64(const std::string& input) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : input) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}
}  // namespace

std::uint64_t FileHistoryStore::repositoryHash(const std::string& repository) {
  return fnv1a64(repository);
}

std::uint64_t FileHistoryStore::runIdentity(const std::string& repository,
                                            std::uint64_t runId) {
  std::uint64_t h = repositoryHash(repository);
  h ^= runId + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  return h;
}

bool FileHistoryStore::begin() {
  if (!LittleFS.exists(kHistoryPath)) {
    File file = LittleFS.open(kHistoryPath, "w");
    if (!file) return false;
    file.close();
    return true;
  }

  File file = LittleFS.open(kHistoryPath, "r");
  if (!file) return false;
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    unsigned long long repoHash = 0, runId = 0, keyHash = 0, duration = 0, completed = 0;
    unsigned success = 0;
    if (sscanf(line.c_str(), "%llx,%llu,%llx,%llu,%u,%llu", &repoHash, &runId,
               &keyHash, &duration, &success, &completed) == 6) {
      cismon::StageSample sample{static_cast<std::uint64_t>(runId),
                                static_cast<std::uint64_t>(keyHash),
                                static_cast<std::uint64_t>(duration), success != 0,
                                static_cast<std::uint64_t>(completed)};
      retain(sample);
      std::uint64_t identity = static_cast<std::uint64_t>(repoHash);
      identity ^= sample.runId + 0x9e3779b97f4a7c15ULL + (identity << 6) + (identity >> 2);
      runs_.insert(identity);
    }
  }
  file.close();
  return true;
}

void FileHistoryStore::retain(const cismon::StageSample& sample) {
  auto& queue = byKey_[sample.keyHash];
  queue.push_back(sample);
  while (queue.size() > perKeyLimit_) queue.pop_front();
}

std::vector<cismon::StageSample> FileHistoryStore::samples(std::uint64_t keyHash,
                                                           std::size_t limit) const {
  auto it = byKey_.find(keyHash);
  if (it == byKey_.end()) return {};
  const auto& queue = it->second;
  const std::size_t start = queue.size() > limit ? queue.size() - limit : 0;
  return std::vector<cismon::StageSample>(queue.begin() + start, queue.end());
}

bool FileHistoryStore::hasRun(const std::string& repository, std::uint64_t runId) const {
  return runs_.count(runIdentity(repository, runId)) != 0;
}

bool FileHistoryStore::append(std::uint64_t repoHash, const cismon::StageSample& sample) {
  File file = LittleFS.open(kHistoryPath, "a");
  if (!file) return false;
  const int written = file.printf("%016llx,%llu,%016llx,%llu,%u,%llu\n",
      static_cast<unsigned long long>(repoHash),
      static_cast<unsigned long long>(sample.runId),
      static_cast<unsigned long long>(sample.keyHash),
      static_cast<unsigned long long>(sample.durationMs), sample.successful ? 1U : 0U,
      static_cast<unsigned long long>(sample.completedAtEpoch));
  file.close();
  if (written <= 0) return false;
  retain(sample);
  return true;
}

bool FileHistoryStore::recordRun(const cismon::RunSnapshot& run) {
  if (run.state != cismon::RunState::Completed || run.runId == 0 ||
      hasRun(run.repository, run.runId)) return false;

  const std::uint64_t repoHash = repositoryHash(run.repository);
  const bool runSuccess = run.conclusion == cismon::Conclusion::Success;
  bool wroteAny = false;
  const auto runDuration = cismon::elapsedMs(run.startedAtEpoch, run.updatedAtEpoch,
                                              run.updatedAtEpoch);
  if (runDuration > 0) {
    cismon::StageSample sample{run.runId, cismon::BaselineKey::forRun(run).hash(),
                              runDuration, runSuccess, run.updatedAtEpoch};
    wroteAny |= append(repoHash, sample);
  }

  for (const auto& job : run.jobs) {
    const auto jobDuration = cismon::elapsedMs(job.startedAtEpoch, job.completedAtEpoch,
                                               job.completedAtEpoch);
    if (jobDuration > 0) {
      cismon::StageSample sample{run.runId, cismon::BaselineKey::forJob(run, job).hash(),
                                jobDuration, job.conclusion == cismon::Conclusion::Success,
                                job.completedAtEpoch};
      wroteAny |= append(repoHash, sample);
    }
    for (const auto& step : job.steps) {
      const auto stepDuration = cismon::elapsedMs(step.startedAtEpoch, step.completedAtEpoch,
                                                  step.completedAtEpoch);
      if (stepDuration == 0) continue;
      cismon::StageSample sample{run.runId,
                                cismon::BaselineKey::forStep(run, job, step).hash(),
                                stepDuration, step.conclusion == cismon::Conclusion::Success,
                                step.completedAtEpoch};
      wroteAny |= append(repoHash, sample);
    }
  }

  if (wroteAny) runs_.insert(runIdentity(run.repository, run.runId));
  return wroteAny;
}

#endif
