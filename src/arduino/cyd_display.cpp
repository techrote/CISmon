#ifdef ARDUINO

#include "cyd_display.hpp"

#include <algorithm>

#ifndef CISMON_TOUCH_MIN_X
#define CISMON_TOUCH_MIN_X 200
#endif
#ifndef CISMON_TOUCH_MAX_X
#define CISMON_TOUCH_MAX_X 3900
#endif
#ifndef CISMON_TOUCH_MIN_Y
#define CISMON_TOUCH_MIN_Y 200
#endif
#ifndef CISMON_TOUCH_MAX_Y
#define CISMON_TOUCH_MAX_Y 3900
#endif

namespace {
constexpr int kBacklightPin = 21;
constexpr int kLedRed = 4;
constexpr int kLedGreen = 16;
constexpr int kLedBlue = 17;
constexpr int kTouchCs = 33;
constexpr int kTouchIrq = 36;
constexpr int kTouchMosi = 32;
constexpr int kTouchMiso = 39;
constexpr int kTouchClk = 25;
constexpr int kHeaderHeight = 28;
constexpr int kRowHeight = 42;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
}

CydDisplay::CydDisplay()
    : touchSpi_(VSPI), touch_(kTouchCs, kTouchIrq) {}

void CydDisplay::begin() {
  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, HIGH);
  pinMode(kLedRed, OUTPUT);
  pinMode(kLedGreen, OUTPUT);
  pinMode(kLedBlue, OUTPUT);
  rgb(false, false, false);

  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextWrap(false);

  touchSpi_.begin(kTouchClk, kTouchMiso, kTouchMosi, kTouchCs);
  touch_.begin(touchSpi_);
  touch_.setRotation(1);
}

void CydDisplay::rgb(bool red, bool green, bool blue) {
  // Common ESP32-2432S028R CYD RGB LED is active-low.
  digitalWrite(kLedRed, red ? LOW : HIGH);
  digitalWrite(kLedGreen, green ? LOW : HIGH);
  digitalWrite(kLedBlue, blue ? LOW : HIGH);
}

void CydDisplay::setBeacon(bool actionRequired, bool suspiciousOrQueued, bool running) {
  if (actionRequired) rgb(true, false, false);
  else if (suspiciousOrQueued) rgb(true, true, false);
  else if (running) rgb(false, false, true);
  else rgb(false, true, false);
}

void CydDisplay::showBoot(const String& line1, const String& line2) {
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.setTextSize(2);
  tft_.drawString("CISmon", 8, 8, 2);
  tft_.setTextSize(1);
  tft_.drawString(line1, 8, 52, 2);
  if (line2.length()) tft_.drawString(line2, 8, 76, 2);
}

String CydDisplay::duration(std::uint64_t ms) {
  const auto totalSeconds = ms / 1000ULL;
  const auto hours = totalSeconds / 3600ULL;
  const auto minutes = (totalSeconds / 60ULL) % 60ULL;
  const auto seconds = totalSeconds % 60ULL;
  char out[20];
  if (hours) snprintf(out, sizeof(out), "%lluh%02llum",
                      static_cast<unsigned long long>(hours),
                      static_cast<unsigned long long>(minutes));
  else if (minutes) snprintf(out, sizeof(out), "%llum%02llus",
                             static_cast<unsigned long long>(minutes),
                             static_cast<unsigned long long>(seconds));
  else snprintf(out, sizeof(out), "%llus", static_cast<unsigned long long>(seconds));
  return String(out);
}

String CydDisplay::shortRepo(const std::string& repository) {
  const auto slash = repository.find('/');
  return String((slash == std::string::npos ? repository : repository.substr(slash + 1)).c_str());
}

String CydDisplay::clip(const String& value, std::size_t chars) {
  if (value.length() <= chars) return value;
  if (chars < 2) return value.substring(0, chars);
  return value.substring(0, chars - 1) + "~";
}

uint16_t CydDisplay::attentionColor(cismon::AttentionLevel level) {
  switch (level) {
    case cismon::AttentionLevel::ActionRequired: return TFT_RED;
    case cismon::AttentionLevel::Suspicious: return TFT_ORANGE;
    case cismon::AttentionLevel::Interesting: return TFT_YELLOW;
    default: return TFT_GREEN;
  }
}

char CydDisplay::stateGlyph(cismon::RunState state, cismon::Conclusion conclusion) {
  if (state == cismon::RunState::InProgress) return '>';
  if (state == cismon::RunState::Queued) return 'Q';
  if (conclusion == cismon::Conclusion::Success) return '+';
  if (cismon::needsAttention(conclusion)) return 'X';
  if (conclusion == cismon::Conclusion::Cancelled) return '-';
  return '?';
}

void CydDisplay::drawDashboard(const std::vector<cismon::DashboardItem>& items,
                               std::size_t repositoryCount, int rateRemaining,
                               const String& lastError) {
  tft_.fillScreen(TFT_BLACK);
  tft_.fillRect(0, 0, kScreenWidth, kHeaderHeight, TFT_DARKGREY);
  tft_.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft_.setTextSize(1);
  tft_.drawString("CISmon", 5, 5, 2);

  int running = 0, queued = 0, alerts = 0;
  for (const auto& item : items) {
    if (item.state == cismon::RunState::InProgress) ++running;
    if (item.state == cismon::RunState::Queued) ++queued;
    if (item.attention == cismon::AttentionLevel::ActionRequired) ++alerts;
  }
  const String summary = ">" + String(running) + " Q" + String(queued) +
                         " !" + String(alerts) + " R" + String(repositoryCount) +
                         (rateRemaining >= 0 ? " API" + String(rateRemaining) : "");
  tft_.drawRightString(summary, 316, 5, 2);

  for (std::size_t i = 0; i < items.size() && i < 5; ++i) {
    const auto& item = items[i];
    const int y = kHeaderHeight + static_cast<int>(i) * kRowHeight;
    const auto color = attentionColor(item.attention);
    tft_.drawFastHLine(0, y, kScreenWidth, TFT_DARKGREY);
    tft_.fillRect(0, y + 1, 4, kRowHeight - 1, color);
    tft_.setTextColor(color, TFT_BLACK);
    tft_.drawChar(stateGlyph(item.state, item.conclusion), 8, y + 5, 2);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString(clip(shortRepo(item.repository), 16), 25, y + 3, 2);
    tft_.drawRightString(duration(item.elapsedMs), 316, y + 3, 2);

    String second = item.stage.empty() ? String(item.workflow.c_str()) : String(item.stage.c_str());
    if (item.baselineMedianMs > 0 && item.ratio >= 1.15) {
      second += "  " + String(item.ratio, 1) + "x";
    } else if (item.estimatedRemainingMs > 0) {
      second += "  ~" + duration(item.estimatedRemainingMs) + " left";
    }
    tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft_.drawString(clip(second, 39), 25, y + 22, 1);
  }

  if (items.empty()) {
    tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft_.drawCentreString("No watched/running work", 160, 92, 2);
    tft_.drawCentreString("CISmon is still polling", 160, 116, 1);
  }
  if (lastError.length()) {
    tft_.fillRect(0, 228, 320, 12, TFT_MAROON);
    tft_.setTextColor(TFT_WHITE, TFT_MAROON);
    tft_.drawString(clip(lastError, 52), 3, 229, 1);
  }
}

void CydDisplay::drawDetail(const cismon::TrackedRun& tracked,
                            const cismon::RuntimeAnalyzer& analyzer,
                            std::uint64_t nowEpoch) {
  const auto& run = tracked.snapshot;
  tft_.fillScreen(TFT_BLACK);
  tft_.fillRect(0, 0, 320, 28, TFT_DARKGREY);
  tft_.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft_.drawString("<", 5, 5, 2);
  tft_.drawString(clip(shortRepo(run.repository) + " / " + String(run.workflowName.c_str()), 34), 24, 5, 2);

  auto runAnalysis = analyzer.analyzeRun(run, nowEpoch);
  tft_.setTextColor(attentionColor(runAnalysis.attention), TFT_BLACK);
  tft_.drawString(String(stateGlyph(run.state, run.conclusion)) + " " + duration(runAnalysis.elapsedMs), 7, 34, 2);
  if (runAnalysis.baseline.samples) {
    const String baseline = "median " + duration(static_cast<std::uint64_t>(runAnalysis.baseline.median)) +
                            "  " + String(runAnalysis.ratio, 2) + "x  n=" +
                            String(runAnalysis.baseline.samples);
    tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft_.drawString(baseline, 7, 55, 1);
  }

  int y = 72;
  for (const auto& job : run.jobs) {
    if (y > 200) break;
    const auto analysis = analyzer.analyzeJob(run, job, nowEpoch);
    tft_.setTextColor(attentionColor(analysis.attention), TFT_BLACK);
    tft_.drawChar(stateGlyph(job.state, job.conclusion), 8, y, 1);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString(clip(String(job.name.c_str()), 29), 21, y - 1, 1);
    tft_.drawRightString(duration(analysis.elapsedMs), 315, y - 1, 1);
    y += 16;
    for (const auto& step : job.steps) {
      if (step.state != cismon::RunState::InProgress || y > 206) continue;
      const auto sa = analyzer.analyzeStep(run, job, step, nowEpoch);
      tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft_.drawString("  > " + clip(String(step.name.c_str()), 29), 16, y, 1);
      tft_.drawRightString(duration(sa.elapsedMs), 315, y, 1);
      y += 15;
    }
  }

  if (cismon::needsAttention(run.conclusion) && !tracked.acknowledged) {
    tft_.fillRoundRect(238, 210, 77, 25, 4, TFT_RED);
    tft_.setTextColor(TFT_WHITE, TFT_RED);
    tft_.drawCentreString("ACK", 276, 215, 2);
  }
}

CydDisplay::Action CydDisplay::pollAction(bool detailMode, std::size_t visibleRows,
                                          bool ackAvailable) {
  if (!touch_.touched()) return {};
  const auto now = static_cast<std::uint64_t>(millis());
  if (now - lastTouchMs_ < 250) return {};
  const TS_Point point = touch_.getPoint();
  lastTouchMs_ = now;

  const int x = constrain(map(point.x, CISMON_TOUCH_MIN_X, CISMON_TOUCH_MAX_X, 0, 319), 0, 319);
  const int y = constrain(map(point.y, CISMON_TOUCH_MIN_Y, CISMON_TOUCH_MAX_Y, 0, 239), 0, 239);
  if (detailMode) {
    if (y < kHeaderHeight) return {ActionType::Back, -1};
    if (ackAvailable && x >= 230 && y >= 205) return {ActionType::Acknowledge, -1};
    return {};
  }

  if (y < kHeaderHeight) return {};
  const int row = (y - kHeaderHeight) / kRowHeight;
  if (row >= 0 && static_cast<std::size_t>(row) < visibleRows) {
    return {ActionType::SelectRow, row};
  }
  return {};
}

#endif
