#ifdef ARDUINO

#include "config_store.hpp"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace {
constexpr const char* kConfigPath = "/config.json";
}

bool ConfigStore::begin() { return LittleFS.begin(true); }

bool ConfigStore::load(DeviceConfig& config) {
  if (!LittleFS.exists(kConfigPath)) return false;
  File file = LittleFS.open(kConfigPath, "r");
  if (!file) return false;

  JsonDocument doc;
  const auto error = deserializeJson(doc, file);
  file.close();
  if (error) return false;

  config.wifiSsid = doc["wifi"]["ssid"] | "";
  config.wifiPassword = doc["wifi"]["password"] | "";
  config.githubToken = doc["github"]["token"] | "";
  config.githubOwner = doc["github"]["owner"] | "";
  config.maxRepositories = doc["github"]["max_repositories"] | 32;
  config.backfillRunsPerRepo = doc["analysis"]["backfill_runs_per_repo"] | 12;
  config.demoMode = doc["demo_mode"] | false;

  config.includePatterns.clear();
  for (JsonVariant value : doc["github"]["include"].as<JsonArray>()) {
    config.includePatterns.emplace_back(value.as<const char*>());
  }
  config.excludePatterns.clear();
  for (JsonVariant value : doc["github"]["exclude"].as<JsonArray>()) {
    config.excludePatterns.emplace_back(value.as<const char*>());
  }
  return config.valid();
}

bool ConfigStore::save(const DeviceConfig& config) {
  JsonDocument doc;
  doc["wifi"]["ssid"] = config.wifiSsid;
  doc["wifi"]["password"] = config.wifiPassword;
  doc["github"]["token"] = config.githubToken;
  doc["github"]["owner"] = config.githubOwner;
  doc["github"]["max_repositories"] = config.maxRepositories;
  doc["analysis"]["backfill_runs_per_repo"] = config.backfillRunsPerRepo;
  doc["demo_mode"] = config.demoMode;

  auto include = doc["github"]["include"].to<JsonArray>();
  for (const auto& pattern : config.includePatterns) include.add(pattern);
  auto exclude = doc["github"]["exclude"].to<JsonArray>();
  for (const auto& pattern : config.excludePatterns) exclude.add(pattern);

  File file = LittleFS.open(kConfigPath, "w");
  if (!file) return false;
  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool ConfigStore::clear() {
  if (!LittleFS.exists(kConfigPath)) return true;
  return LittleFS.remove(kConfigPath);
}

#endif
