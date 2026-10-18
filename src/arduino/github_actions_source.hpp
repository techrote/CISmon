#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <ArduinoJson.h>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cismon/poll_policy.hpp"
#include "cismon/provider.hpp"
#include "config_store.hpp"
#include "github_api.hpp"

class GitHubActionsSource final : public cismon::ICIProvider {
 public:
  GitHubActionsSource(GitHubApiClient& api, const DeviceConfig& config)
      : api_(api), config_(config) {}

  const char* name() const override { return "GitHub Actions"; }
  bool begin() override;
  void tick(std::uint64_t nowMs, cismon::RunTracker& tracker,
            cismon::IHistoryStore& history) override;

  std::size_t repositoryCount() const { return repositories_.size(); }
  int rateRemaining() const { return rateRemaining_; }
  String lastError() const { return lastError_; }

 private:
  struct RepoState {
    String fullName;
    String etag;
    std::uint64_t nextPollMs = 0;
    std::uint64_t lastActivityMs = 0;
    std::uint64_t latestUpdatedEpoch = 0;
    unsigned errors = 0;
    bool hadActive = false;
    bool hadQueued = false;
  };

  struct DetailTask {
    cismon::RunSnapshot run;
  };

  bool discoverRepositories();
  bool repositoryAllowed(const String& fullName) const;
  bool pollRepository(RepoState& repo, std::uint64_t nowMs,
                      cismon::RunTracker& tracker, cismon::IHistoryStore& history);
  bool fetchRunDetails(cismon::RunSnapshot run, std::uint64_t nowMs,
                       cismon::RunTracker& tracker, cismon::IHistoryStore& history,
                       bool recordWhenComplete);
  void queueBackfill(const cismon::RunSnapshot& run);
  void updateRate(const GitHubResponse& response, std::uint64_t nowMs);

  static cismon::RunSnapshot parseRun(JsonVariantConst value, const String& repo);
  static cismon::JobSnapshot parseJob(JsonVariantConst value);
  static cismon::RunState parseState(const char* value);
  static cismon::Conclusion parseConclusion(const char* value);
  static std::uint64_t parseTimestamp(const char* value);
  static std::string detailKey(const cismon::RunSnapshot& run);

  GitHubApiClient& api_;
  const DeviceConfig& config_;
  cismon::PollPolicy policy_;
  std::vector<RepoState> repositories_;
  std::deque<DetailTask> backfill_;
  std::unordered_set<std::string> queuedBackfill_;
  std::unordered_map<std::string, std::uint64_t> activeDetailNextMs_;
  std::uint64_t nextRequestAllowedMs_ = 0;
  std::uint64_t rateLimitedUntilMs_ = 0;
  int rateRemaining_ = -1;
  String lastError_;
};

#endif
