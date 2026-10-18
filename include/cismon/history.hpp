#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cismon/model.hpp"

namespace cismon {

class IHistoryStore {
 public:
  virtual ~IHistoryStore() = default;
  virtual std::vector<StageSample> samples(std::uint64_t keyHash,
                                           std::size_t limit) const = 0;
  virtual bool hasRun(const std::string& repository, std::uint64_t runId) const = 0;
  virtual bool recordRun(const RunSnapshot& run) = 0;
};

}  // namespace cismon
