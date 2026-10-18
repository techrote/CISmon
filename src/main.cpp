#ifdef ARDUINO

#include <Arduino.h>
#include <WiFi.h>
#include <ctime>
#include <algorithm>
#include <memory>

#include "cismon/analyzer.hpp"
#include "cismon/dashboard.hpp"
#include "cismon/tracker.hpp"
#include "arduino/config_store.hpp"
#include "arduino/cyd_display.hpp"
#include "arduino/file_history_store.hpp"
#include "arduino/github_actions_source.hpp"
#include "arduino/github_api.hpp"
#include "arduino/provisioning.hpp"

namespace {
ConfigStore configStore;
DeviceConfig config;
CydDisplay display;
GitHubApiClient github;
FileHistoryStore history(64);
cismon::RunTracker tracker;
std::unique_ptr<cismon::RuntimeAnalyzer> analyzer;
std::unique_ptr<GitHubActionsSource> actions;

bool providerReady = false;
std::uint64_t nextProviderBeginMs = 0;
std::uint64_t nextRenderMs = 0;
std::string selectedRepository;
std::uint64_t selectedRunId = 0;
std::vector<cismon::DashboardItem> currentRows;

std::uint64_t nowEpoch() {
  const auto value = static_cast<std::uint64_t>(time(nullptr));
  return value > 1000000000ULL ? value : 0;
}

bool connectWifi() {
  display.showBoot("Connecting Wi-Fi", config.wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  const auto started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000UL) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

void provision() {
  const String suffix = String(static_cast<std::uint32_t>(ESP.getEfuseMac()), HEX);
  display.showBoot("Setup required", "Join CISmon-" + suffix + " then open 192.168.4.1");
  ProvisioningPortal portal;
  if (portal.run(configStore, config)) ESP.restart();
}

void updateBeacon(const std::vector<cismon::DashboardItem>& items) {
  bool action = false, warning = false, running = false;
  for (const auto& item : items) {
    action |= item.attention == cismon::AttentionLevel::ActionRequired;
    warning |= item.attention == cismon::AttentionLevel::Suspicious ||
               item.attention == cismon::AttentionLevel::Interesting ||
               item.state == cismon::RunState::Queued;
    running |= item.state == cismon::RunState::InProgress;
  }
  display.setBeacon(action, warning, running);
}

void render(std::uint64_t nowMs) {
  const auto epoch = nowEpoch();
  auto allRows = cismon::buildDashboard(tracker, *analyzer, nowMs, epoch, 64);
  updateBeacon(allRows);
  currentRows.assign(allRows.begin(), allRows.begin() + std::min<std::size_t>(5, allRows.size()));

  if (selectedRunId != 0) {
    const auto* tracked = tracker.find(selectedRepository, selectedRunId);
    if (tracked) {
      display.drawDetail(*tracked, *analyzer, epoch);
      return;
    }
    selectedRunId = 0;
    selectedRepository.clear();
  }

  display.drawDashboard(currentRows, actions ? actions->repositoryCount() : 0,
                        actions ? actions->rateRemaining() : -1,
                        actions ? actions->lastError() : String());
}

void handleTouch() {
  const bool detail = selectedRunId != 0;
  const auto* selected = detail ? tracker.find(selectedRepository, selectedRunId) : nullptr;
  const bool ackAvailable = selected && cismon::needsAttention(selected->snapshot.conclusion) &&
                            !selected->acknowledged;
  const auto action = display.pollAction(detail, currentRows.size(), ackAvailable);
  switch (action.type) {
    case CydDisplay::ActionType::SelectRow:
      if (action.row >= 0 && static_cast<std::size_t>(action.row) < currentRows.size()) {
        selectedRepository = currentRows[action.row].repository;
        selectedRunId = currentRows[action.row].runId;
        nextRenderMs = 0;
      }
      break;
    case CydDisplay::ActionType::Back:
      selectedRunId = 0;
      selectedRepository.clear();
      nextRenderMs = 0;
      break;
    case CydDisplay::ActionType::Acknowledge:
      if (selected) {
        tracker.acknowledge(selectedRepository, selectedRunId);
        selectedRunId = 0;
        selectedRepository.clear();
        nextRenderMs = 0;
      }
      break;
    default:
      break;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  display.begin();
  display.showBoot("Starting");

  if (!configStore.begin()) {
    display.showBoot("LittleFS failed");
    delay(2000);
  }
  if (!configStore.load(config) || !config.valid()) provision();
  if (!connectWifi()) provision();

  display.showBoot("Wi-Fi connected", WiFi.localIP().toString());
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  github.begin(config.githubToken);
  history.begin();
  analyzer = std::make_unique<cismon::RuntimeAnalyzer>(history, 32);
  actions = std::make_unique<GitHubActionsSource>(github, config);
  providerReady = actions->begin();
  if (!providerReady) nextProviderBeginMs = millis() + 30000ULL;
  delay(250);
}

void loop() {
  const auto nowMs = static_cast<std::uint64_t>(millis());
  if (!providerReady && nowMs >= nextProviderBeginMs) {
    providerReady = actions->begin();
    nextProviderBeginMs = nowMs + 60000ULL;
  }
  if (providerReady) actions->tick(nowMs, tracker, history);

  tracker.prune(nowMs);
  handleTouch();
  if (nowMs >= nextRenderMs) {
    render(nowMs);
    nextRenderMs = nowMs + 1000ULL;
  }
  delay(10);
}

#endif
