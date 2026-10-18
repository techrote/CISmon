#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cismon {

enum class RunState : std::uint8_t { Unknown, Queued, InProgress, Completed };
enum class Conclusion : std::uint8_t {
  Unknown,
  Success,
  Failure,
  Cancelled,
  TimedOut,
  Skipped,
  Neutral,
  ActionRequired,
  StartupFailure
};
enum class AttentionLevel : std::uint8_t { Normal, Interesting, Suspicious, ActionRequired };
enum class BaselineKind : std::uint8_t { Run, Job, Step };

struct StepSnapshot {
  std::string name;
  int number = 0;
  RunState state = RunState::Unknown;
  Conclusion conclusion = Conclusion::Unknown;
  std::uint64_t startedAtEpoch = 0;
  std::uint64_t completedAtEpoch = 0;
};

struct JobSnapshot {
  std::uint64_t id = 0;
  std::string name;
  std::string runnerName;
  RunState state = RunState::Unknown;
  Conclusion conclusion = Conclusion::Unknown;
  std::uint64_t startedAtEpoch = 0;
  std::uint64_t completedAtEpoch = 0;
  std::vector<StepSnapshot> steps;
};

struct RunSnapshot {
  std::string repository;
  std::uint64_t workflowId = 0;
  std::string workflowName;
  std::uint64_t runId = 0;
  std::uint32_t runNumber = 0;
  std::string branch;
  std::string event;
  std::string headSha;
  RunState state = RunState::Unknown;
  Conclusion conclusion = Conclusion::Unknown;
  std::uint64_t startedAtEpoch = 0;
  std::uint64_t updatedAtEpoch = 0;
  std::vector<JobSnapshot> jobs;
};

struct BaselineKey {
  BaselineKind kind = BaselineKind::Run;
  std::string repository;
  std::uint64_t workflowId = 0;
  std::string event;
  std::string job;
  std::string step;

  static BaselineKey forRun(const RunSnapshot& run) {
    return {BaselineKind::Run, run.repository, run.workflowId, run.event, {}, {}};
  }

  static BaselineKey forJob(const RunSnapshot& run, const JobSnapshot& jobSnapshot) {
    return {BaselineKind::Job, run.repository, run.workflowId, run.event, jobSnapshot.name, {}};
  }

  static BaselineKey forStep(const RunSnapshot& run, const JobSnapshot& jobSnapshot,
                             const StepSnapshot& stepSnapshot) {
    return {BaselineKind::Step, run.repository, run.workflowId, run.event,
            jobSnapshot.name, stepSnapshot.name};
  }

  std::string canonical() const;
  std::uint64_t hash() const;
};

struct StageSample {
  std::uint64_t runId = 0;
  std::uint64_t keyHash = 0;
  std::uint64_t durationMs = 0;
  bool successful = false;
  std::uint64_t completedAtEpoch = 0;
};

inline bool isTerminal(RunState state) { return state == RunState::Completed; }
inline bool needsAttention(Conclusion c) {
  return c == Conclusion::Failure || c == Conclusion::TimedOut ||
         c == Conclusion::ActionRequired || c == Conclusion::StartupFailure;
}

const char* toString(RunState state);
const char* toString(Conclusion conclusion);
const char* toString(AttentionLevel level);

}  // namespace cismon
