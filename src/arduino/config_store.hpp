#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <cstdint>
#include <vector>

struct DeviceConfig {
  String wifiSsid;
  String wifiPassword;
  String githubToken;
  String githubOwner;
  std::vector<String> includePatterns;
  std::vector<String> excludePatterns;
  std::uint16_t maxRepositories = 32;
  std::uint8_t backfillRunsPerRepo = 12;
  bool demoMode = false;

  bool valid() const { return wifiSsid.length() > 0 && githubToken.length() > 0; }
};

class ConfigStore {
 public:
  bool begin();
  bool load(DeviceConfig& config);
  bool save(const DeviceConfig& config);
  bool clear();
};

#endif
