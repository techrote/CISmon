#include "cismon/glob.hpp"

namespace cismon {

bool globMatch(const std::string& pattern, const std::string& value) {
  std::size_t p = 0, v = 0, star = std::string::npos, match = 0;
  while (v < value.size()) {
    if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == value[v])) {
      ++p;
      ++v;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      match = v;
    } else if (star != std::string::npos) {
      p = star + 1;
      v = ++match;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') ++p;
  return p == pattern.size();
}

}  // namespace cismon
