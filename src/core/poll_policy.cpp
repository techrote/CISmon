#include "cismon/poll_policy.hpp"

#include <algorithm>

namespace cismon {

std::uint64_t PollPolicy::nextInterval(bool hasActive, bool hasQueued,
                                       std::uint64_t ageSinceActivityMs,
                                       unsigned consecutiveErrors) const {
  if (consecutiveErrors > 0) {
    std::uint64_t interval = errorBaseMs;
    for (unsigned i = 1; i < consecutiveErrors; ++i) {
      if (interval >= errorMaxMs / 2) return errorMaxMs;
      interval *= 2;
    }
    return std::min(interval, errorMaxMs);
  }
  if (hasActive) return activeMs;
  if (hasQueued) return queuedMs;
  if (ageSinceActivityMs < warmWindowMs) return warmIdleMs;
  if (ageSinceActivityMs < warmWindowMs * 6) return idleMs;
  return coldMs;
}

}  // namespace cismon
