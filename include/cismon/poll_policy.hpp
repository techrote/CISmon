#pragma once

#include <cstdint>

namespace cismon {

struct PollPolicy {
  std::uint64_t activeMs = 15000;
  std::uint64_t queuedMs = 20000;
  std::uint64_t warmIdleMs = 60000;
  std::uint64_t idleMs = 120000;
  std::uint64_t coldMs = 300000;
  std::uint64_t warmWindowMs = 3600000;
  std::uint64_t errorBaseMs = 15000;
  std::uint64_t errorMaxMs = 600000;

  std::uint64_t nextInterval(bool hasActive, bool hasQueued,
                             std::uint64_t ageSinceActivityMs,
                             unsigned consecutiveErrors) const;
};

}  // namespace cismon
