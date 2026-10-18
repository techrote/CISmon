#ifdef ARDUINO

#include "github_actions_source.hpp"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "cismon/glob.hpp"

namespace {
std::int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

std::string asStd(const String& value) { return std::string(value.c_str()); }
}  // namespace

bool GitHubActionsSource::begin() { return discoverRepositories(); }

bool GitHubActionsSource::repositoryAllowed(const String& fullName) const {
  const std::string candidate(fullName.c_str());
  if (config_.githubOwner.length()) {
    const String prefix = config_.githubOwner + "/";
    if (!fullName.startsWith(prefix)) return false;
  }

  bool included = config_.includePatterns.empty();
  for (const auto& pattern : config_.includePatterns) {
    if (cismon::globMatch(asStd(pattern), candidate)) {
      included = true;
      break;
    }
  }
  if (!included) return false;
  for (const auto& pattern : config_.excludePatterns) {
    if (cismon::globMatch(asStd(pattern), candidate)) return false;
  }
  return true;
}

bool GitHubActionsSource::discoverRepositories() {
  const auto response = api_.get("/user/repos?per_page=100&sort=pushed&affiliation=owner,collaborator,organization_member");
  updateRate(response, millis());
  if (!response.ok()) {
    lastError_ = "repo discovery HTTP " + String(response.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response.body)) {
    lastError_ = "repo discovery JSON";
    return false;
  }

  repositories_.clear();
  for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
    const String fullName = item["full_name"] | "";
    if (!fullName.length() || !repositoryAllowed(fullName)) continue;
    repositories_.push_back({fullName});
    if (repositories_.size() >= config_.maxRepositories) break;
  }

  if (repositories_.empty()) {
    lastError_ = "no matching repositories";
    return false;
  }
  return true;
}

cismon::RunState GitHubActionsSource::parseState(const char* value) {
  if (!value) return cismon::RunState::Unknown;
  if (!strcmp(value, "queued") || !strcmp(value, "waiting") || !strcmp(value, "pending"))
    return cismon::RunState::Queued;
  if (!strcmp(value, "in_progress")) return cismon::RunState::InProgress;
  if (!strcmp(value, "completed")) return cismon::RunState::Completed;
  return cismon::RunState::Unknown;
}

cismon::Conclusion GitHubActionsSource::parseConclusion(const char* value) {
  if (!value || !*value) return cismon::Conclusion::Unknown;
  if (!strcmp(value, "success")) return cismon::Conclusion::Success;
  if (!strcmp(value, "failure")) return cismon::Conclusion::Failure;
  if (!strcmp(value, "cancelled")) return cismon::Conclusion::Cancelled;
  if (!strcmp(value, "timed_out")) return cismon::Conclusion::TimedOut;
  if (!strcmp(value, "skipped")) return cismon::Conclusion::Skipped;
  if (!strcmp(value, "neutral")) return cismon::Conclusion::Neutral;
  if (!strcmp(value, "action_required")) return cismon::Conclusion::ActionRequired;
  if (!strcmp(value, "startup_failure")) return cismon::Conclusion::StartupFailure;
  return cismon::Conclusion::Unknown;
}

std::uint64_t GitHubActionsSource::parseTimestamp(const char* value) {
  if (!value || !*value) return 0;
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (sscanf(value, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) != 6)
    return 0;
  const std::int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  if (days < 0) return 0;
  return static_cast<std::uint64_t>(days * 86400LL + hour * 3600 + minute * 60 + second);
}

cismon::RunSnapshot GitHubActionsSource::parseRun(JsonVariantConst value, const String& repo) {
  cismon::RunSnapshot run;
  run.repository = repo.c_str();
  run.workflowId = value["workflow_id"] | 0ULL;
  run.workflowName = (value["name"] | "workflow");
  run.runId = value["id"] | 0ULL;
  run.runNumber = value["run_number"] | 0U;
  run.branch = (value["head_branch"] | "");
  run.event = (value["event"] | "");
  run.headSha = (value["head_sha"] | "");
  run.state = parseState(value["status"] | "");
  run.conclusion = parseConclusion(value["conclusion"] | "");
  const char* started = value["run_started_at"] | "";
  if (!started || !*started) started = value["created_at"] | "";
  run.startedAtEpoch = parseTimestamp(started);
  run.updatedAtEpoch = parseTimestamp(value["updated_at"] | "");
  return run;
}

cismon::JobSnapshot GitHubActionsSource::parseJob(JsonVariantConst value) {
  cismon::JobSnapshot job;
  job.id = value["id"] | 0ULL;
  job.name = (value["name"] | "job");
  job.runnerName = (value["runner_name"] | "");
  job.state = parseState(value["status"] | "");
  job.conclusion = parseConclusion(value["conclusion"] | "");
  job.startedAtEpoch = parseTimestamp(value["started_at"] | "");
  job.completedAtEpoch = parseTimestamp(value["completed_at"] | "");
  for (JsonVariantConst item : value["steps"].as<JsonArrayConst>()) {
    cismon::StepSnapshot step;
    step.name = (item["name"] | "step");
    step.number = item["number"] | 0;
    step.state = parseState(item["status"] | "");
    step.conclusion = parseConclusion(item["conclusion"] | "");
    step.startedAtEpoch = parseTimestamp(item["started_at"] | "");
    step.completedAtEpoch = parseTimestamp(item["completed_at"] | "");
    job.steps.push_back(std::move(step));
  }
  return job;
}

std::string GitHubActionsSource::detailKey(const cismon::RunSnapshot& run) {
  return run.repository + "#" + std::to_string(run.runId);
}

void GitHubActionsSource::updateRate(const GitHubResponse& response, std::uint64_t nowMs) {
  if (response.rateRemaining >= 0) rateRemaining_ = response.rateRemaining;
  if ((response.status == 403 || response.status == 429 || response.rateRemaining == 0) &&
      response.rateResetEpoch > 0) {
    const auto nowEpoch = static_cast<std::uint64_t>(time(nullptr));
    if (nowEpoch > 1000000000ULL && response.rateResetEpoch > nowEpoch) {
      rateLimitedUntilMs_ = nowMs + (response.rateResetEpoch - nowEpoch + 2ULL) * 1000ULL;
    } else {
      rateLimitedUntilMs_ = nowMs + 60000ULL;
    }
  }
}

void GitHubActionsSource::queueBackfill(const cismon::RunSnapshot& run) {
  const auto key = detailKey(run);
  if (queuedBackfill_.insert(key).second) backfill_.push_back({run});
}

bool GitHubActionsSource::pollRepository(RepoState& repo, std::uint64_t nowMs,
                                         cismon::RunTracker& tracker,
                                         cismon::IHistoryStore& history) {
  const String path = "/repos/" + repo.fullName + "/actions/runs?per_page=20";
  const auto response = api_.get(path, repo.etag);
  updateRate(response, nowMs);
  if (response.notModified()) {
    repo.errors = 0;
    const auto age = repo.lastActivityMs ? nowMs - repo.lastActivityMs : policy_.warmWindowMs * 10;
    repo.nextPollMs = nowMs + policy_.nextInterval(repo.hadActive, repo.hadQueued, age, 0);
    return true;
  }
  if (!response.ok()) {
    ++repo.errors;
    repo.nextPollMs = nowMs + policy_.nextInterval(false, false, 0, repo.errors);
    lastError_ = repo.fullName + " runs HTTP " + String(response.status);
    return false;
  }

  if (response.etag.length()) repo.etag = response.etag;
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) {
    ++repo.errors;
    repo.nextPollMs = nowMs + policy_.nextInterval(false, false, 0, repo.errors);
    lastError_ = repo.fullName + " runs JSON";
    return false;
  }

  repo.errors = 0;
  bool hasActive = false;
  bool hasQueued = false;
  std::uint64_t newestUpdated = 0;
  std::uint8_t backfillQueued = 0;

  for (JsonVariantConst item : doc["workflow_runs"].as<JsonArrayConst>()) {
    auto run = parseRun(item, repo.fullName);
    if (run.runId == 0) continue;
    newestUpdated = std::max(newestUpdated, run.updatedAtEpoch);
    tracker.upsert(run, nowMs);

    if (run.state == cismon::RunState::InProgress) {
      hasActive = true;
      const auto key = detailKey(run);
      if (!activeDetailNextMs_.count(key)) activeDetailNextMs_[key] = 0;
    } else if (run.state == cismon::RunState::Queued) {
      hasQueued = true;
    } else if (run.state == cismon::RunState::Completed &&
               backfillQueued < config_.backfillRunsPerRepo &&
               !history.hasRun(run.repository, run.runId)) {
      queueBackfill(run);
      ++backfillQueued;
    }
  }

  if (repo.latestUpdatedEpoch != newestUpdated) {
    repo.latestUpdatedEpoch = newestUpdated;
    repo.lastActivityMs = nowMs;
  }
  repo.hadActive = hasActive;
  repo.hadQueued = hasQueued;
  const auto age = repo.lastActivityMs ? nowMs - repo.lastActivityMs : policy_.warmWindowMs * 10;
  repo.nextPollMs = nowMs + policy_.nextInterval(hasActive, hasQueued, age, 0);
  lastError_ = "";
  return true;
}

bool GitHubActionsSource::fetchRunDetails(cismon::RunSnapshot run, std::uint64_t nowMs,
                                          cismon::RunTracker& tracker,
                                          cismon::IHistoryStore& history,
                                          bool recordWhenComplete) {
  char runId[24];
  snprintf(runId, sizeof(runId), "%llu", static_cast<unsigned long long>(run.runId));
  const String path = "/repos/" + String(run.repository.c_str()) + "/actions/runs/" +
                      String(runId) + "/jobs?per_page=100";
  const auto response = api_.get(path);
  updateRate(response, nowMs);
  if (!response.ok()) {
    lastError_ = String(run.repository.c_str()) + " jobs HTTP " + String(response.status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response.body)) {
    lastError_ = String(run.repository.c_str()) + " jobs JSON";
    return false;
  }
  run.jobs.clear();
  for (JsonVariantConst item : doc["jobs"].as<JsonArrayConst>()) {
    run.jobs.push_back(parseJob(item));
  }
  tracker.upsert(run, nowMs);
  if (recordWhenComplete && run.state == cismon::RunState::Completed) history.recordRun(run);
  return true;
}

void GitHubActionsSource::tick(std::uint64_t nowMs, cismon::RunTracker& tracker,
                               cismon::IHistoryStore& history) {
  if (nowMs < nextRequestAllowedMs_ || nowMs < rateLimitedUntilMs_) return;

  // Active runs get first priority so the screen follows long tasks closely.
  for (const auto& tracked : tracker.all()) {
    if (tracked.snapshot.state != cismon::RunState::InProgress) continue;
    const auto key = detailKey(tracked.snapshot);
    auto it = activeDetailNextMs_.find(key);
    if (it == activeDetailNextMs_.end()) {
      activeDetailNextMs_[key] = 0;
      it = activeDetailNextMs_.find(key);
    }
    if (nowMs < it->second) continue;
    fetchRunDetails(tracked.snapshot, nowMs, tracker, history, false);
    it->second = nowMs + policy_.activeMs;
    nextRequestAllowedMs_ = nowMs + 1000;
    return;
  }

  // Then poll the next due repository.
  for (auto& repo : repositories_) {
    if (nowMs < repo.nextPollMs) continue;
    pollRepository(repo, nowMs, tracker, history);
    nextRequestAllowedMs_ = nowMs + 1000;
    return;
  }

  // Historical job/step backfill is deliberately lowest priority.
  while (!backfill_.empty()) {
    auto task = backfill_.front();
    backfill_.pop_front();
    queuedBackfill_.erase(detailKey(task.run));
    if (history.hasRun(task.run.repository, task.run.runId)) continue;
    fetchRunDetails(task.run, nowMs, tracker, history, true);
    nextRequestAllowedMs_ = nowMs + 1200;
    return;
  }
}

#endif
