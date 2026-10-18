#ifdef ARDUINO

#include "github_api.hpp"

void GitHubApiClient::begin(const String& token) {
  token_ = token;
  // PoC transport: this avoids certificate-chain fragility across Arduino cores.
  // See SECURITY.md before using a token with anything beyond read-only repo scope.
  client_.setInsecure();
}

GitHubResponse GitHubApiClient::get(const String& path, const String& etag) {
  GitHubResponse response;
  HTTPClient http;
  const String url = path.startsWith("http") ? path : "https://api.github.com" + path;
  if (!http.begin(client_, url)) {
    response.status = -1;
    return response;
  }

  const char* headers[] = {"ETag", "X-RateLimit-Remaining", "X-RateLimit-Reset"};
  http.collectHeaders(headers, 3);
  http.setTimeout(12000);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  http.addHeader("User-Agent", "CISmon-ESP32-PoC");
  http.addHeader("Authorization", "Bearer " + token_);
  if (etag.length()) http.addHeader("If-None-Match", etag);

  response.status = http.GET();
  response.etag = http.header("ETag");
  response.rateRemaining = http.header("X-RateLimit-Remaining").toInt();
  response.rateResetEpoch = strtoull(http.header("X-RateLimit-Reset").c_str(), nullptr, 10);
  if (response.status != 304 && response.status > 0) response.body = http.getString();
  http.end();
  return response;
}

#endif
