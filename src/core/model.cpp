#include "cismon/model.hpp"

#include <sstream>

namespace cismon {
namespace {
std::uint64_t fnv1a64(const std::string& input) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : input) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}
}  // namespace

std::string BaselineKey::canonical() const {
  std::ostringstream out;
  out << static_cast<int>(kind) << '\x1f' << repository << '\x1f' << workflowId
      << '\x1f' << event << '\x1f' << job << '\x1f' << step;
  return out.str();
}

std::uint64_t BaselineKey::hash() const { return fnv1a64(canonical()); }

const char* toString(RunState state) {
  switch (state) {
    case RunState::Queued: return "queued";
    case RunState::InProgress: return "in_progress";
    case RunState::Completed: return "completed";
    default: return "unknown";
  }
}

const char* toString(Conclusion conclusion) {
  switch (conclusion) {
    case Conclusion::Success: return "success";
    case Conclusion::Failure: return "failure";
    case Conclusion::Cancelled: return "cancelled";
    case Conclusion::TimedOut: return "timed_out";
    case Conclusion::Skipped: return "skipped";
    case Conclusion::Neutral: return "neutral";
    case Conclusion::ActionRequired: return "action_required";
    case Conclusion::StartupFailure: return "startup_failure";
    default: return "unknown";
  }
}

const char* toString(AttentionLevel level) {
  switch (level) {
    case AttentionLevel::Interesting: return "interesting";
    case AttentionLevel::Suspicious: return "suspicious";
    case AttentionLevel::ActionRequired: return "action_required";
    default: return "normal";
  }
}

}  // namespace cismon
