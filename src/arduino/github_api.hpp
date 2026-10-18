#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

struct GitHubResponse {
  int status = 0;
  String body;
  String etag;
  int rateRemaining = -1;
  std::uint64_t rateResetEpoch = 0;

  bool ok() const { return status >= 200 && status < 300; }
  bool notModified() const { return status == 304; }
};

class GitHubApiClient {
 public:
  void begin(const String& token);
  GitHubResponse get(const String& path, const String& etag = String());

 private:
  String token_;
  WiFiClientSecure client_;
};

#endif
