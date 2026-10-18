#ifdef ARDUINO

#include "provisioning.hpp"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <vector>

namespace {
std::vector<String> splitPatterns(const String& input) {
  std::vector<String> result;
  int start = 0;
  while (start < static_cast<int>(input.length())) {
    int comma = input.indexOf(',', start);
    if (comma < 0) comma = input.length();
    String item = input.substring(start, comma);
    item.trim();
    if (item.length()) result.push_back(item);
    start = comma + 1;
  }
  return result;
}

String htmlPage() {
  return R"HTML(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:system-ui;background:#111;color:#eee;max-width:42rem;margin:auto;padding:1rem}input{box-sizing:border-box;width:100%;padding:.6rem;margin:.25rem 0 1rem;background:#222;color:#fff;border:1px solid #555}button{padding:.8rem 1.2rem;font-weight:700}small{color:#aaa}</style></head><body>
<h2>CISmon setup</h2><form method="post" action="/save">
<label>Wi-Fi SSID</label><input name="ssid" required>
<label>Wi-Fi password</label><input name="wifi_password" type="password">
<label>GitHub fine-grained token</label><input name="token" type="password" required>
<small>PoC target: read-only Actions/metadata access to the repositories you want to monitor.</small><br><br>
<label>Owner filter (optional)</label><input name="owner" placeholder="techrote">
<label>Include globs (comma-separated)</label><input name="include" placeholder="techrote/*">
<label>Exclude globs (comma-separated)</label><input name="exclude" placeholder="*/archive-*,*/scratch-*">
<label>Max repositories</label><input name="maxrepos" type="number" min="1" max="100" value="32">
<label>History backfill runs / repo</label><input name="backfill" type="number" min="0" max="30" value="12">
<button type="submit">Save and reboot</button></form></body></html>)HTML";
}
}  // namespace

bool ProvisioningPortal::run(ConfigStore& store, DeviceConfig& config) {
  WiFi.mode(WIFI_AP);
  const String apName = "CISmon-" + String(static_cast<std::uint32_t>(ESP.getEfuseMac()), HEX);
  if (!WiFi.softAP(apName.c_str())) return false;

  DNSServer dns;
  dns.start(53, "*", WiFi.softAPIP());
  WebServer server(80);
  bool saved = false;

  server.on("/", HTTP_GET, [&]() { server.send(200, "text/html", htmlPage()); });
  server.on("/save", HTTP_POST, [&]() {
    config.wifiSsid = server.arg("ssid");
    config.wifiPassword = server.arg("wifi_password");
    config.githubToken = server.arg("token");
    config.githubOwner = server.arg("owner");
    config.includePatterns = splitPatterns(server.arg("include"));
    config.excludePatterns = splitPatterns(server.arg("exclude"));
    config.maxRepositories = constrain(server.arg("maxrepos").toInt(), 1, 100);
    config.backfillRunsPerRepo = constrain(server.arg("backfill").toInt(), 0, 30);
    if (!config.valid() || !store.save(config)) {
      server.send(500, "text/plain", "Could not save CISmon configuration");
      return;
    }
    server.send(200, "text/html", "<h2>Saved.</h2><p>CISmon is rebooting.</p>");
    saved = true;
  });
  server.onNotFound([&]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  const std::uint64_t started = millis();
  while (!saved) {
    dns.processNextRequest();
    server.handleClient();
    delay(5);
    // Stay available indefinitely; the timestamp only prevents millis() optimization warnings.
    (void)started;
  }
  delay(500);
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  return true;
}

#endif
