#pragma once

#ifdef ARDUINO

#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "cismon/history.hpp"

class FileHistoryStore final : public cismon::IHistoryStore {
 public:
  explicit FileHistoryStore(std::size_t perKeyLimit = 64) : perKeyLimit_(perKeyLimit) {}

  bool begin();
  std::vector<cismon::StageSample> samples(std::uint64_t keyHash,
                                           std::size_t limit) const override;
  bool hasRun(const std::string& repository, std::uint64_t runId) const override;
  bool recordRun(const cismon::RunSnapshot& run) override;

 private:
  static std::uint64_t repositoryHash(const std::string& repository);
  static std::uint64_t runIdentity(const std::string& repository, std::uint64_t runId);
  void retain(const cismon::StageSample& sample);
  bool append(std::uint64_t repoHash, const cismon::StageSample& sample);

  std::size_t perKeyLimit_;
  std::unordered_map<std::uint64_t, std::deque<cismon::StageSample>> byKey_;
  std::unordered_set<std::uint64_t> runs_;
};

#endif
