#pragma once

#include <cstdint>

#include "cismon/history.hpp"
#include "cismon/tracker.hpp"

namespace cismon {

class ICIProvider {
 public:
  virtual ~ICIProvider() = default;
  virtual const char* name() const = 0;
  virtual bool begin() = 0;
  virtual void tick(std::uint64_t nowMs, RunTracker& tracker, IHistoryStore& history) = 0;
};

}  // namespace cismon
