#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include <vector>

#include "cismon/dashboard.hpp"

class CydDisplay {
 public:
  enum class ActionType { None, SelectRow, Back, Acknowledge };
  struct Action {
    ActionType type = ActionType::None;
    int row = -1;
  };

  CydDisplay();
  void begin();
  void showBoot(const String& line1, const String& line2 = String());
  void drawDashboard(const std::vector<cismon::DashboardItem>& items,
                     std::size_t repositoryCount, int rateRemaining,
                     const String& lastError);
  void drawDetail(const cismon::TrackedRun& tracked,
                  const cismon::RuntimeAnalyzer& analyzer,
                  std::uint64_t nowEpoch);
  Action pollAction(bool detailMode, std::size_t visibleRows, bool ackAvailable);
  void setBeacon(bool actionRequired, bool suspiciousOrQueued, bool running);

 private:
  static String duration(std::uint64_t ms);
  static String shortRepo(const std::string& repository);
  static uint16_t attentionColor(cismon::AttentionLevel level);
  static char stateGlyph(cismon::RunState state, cismon::Conclusion conclusion);
  static String clip(const String& value, std::size_t chars);
  void rgb(bool red, bool green, bool blue);

  TFT_eSPI tft_;
  SPIClass touchSpi_;
  XPT2046_Touchscreen touch_;
  std::uint64_t lastTouchMs_ = 0;
};

#endif
