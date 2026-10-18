# Security notes for the proof of concept

CISmon handles a GitHub credential, so the current PoC security properties should be explicit rather than implied.

## Token scope

Use a **fine-grained token with read-only access and only the repositories CISmon needs to monitor**. Do not reuse a broad personal access token, an Actions write token, or a credential capable of repository administration.

CISmon currently needs repository metadata and read access sufficient to list Actions workflow runs/jobs for the selected repositories.

## Current storage

The first-boot portal writes the GitHub token and Wi-Fi credentials to `/config.json` in LittleFS. They are not compiled into the binary and are not printed to serial output, but LittleFS is not a hardware secret store.

Anyone who can dump flash from the device should be assumed capable of recovering the token.

## Current TLS caveat

For compatibility across Arduino-ESP32 certificate configurations, the PoC currently calls `WiFiClientSecure::setInsecure()`. Traffic remains encrypted but the server certificate is not authenticated, so an active network attacker could impersonate the API endpoint and capture the token.

This is acceptable only as an explicit prototype compromise with a tightly scoped read-only token.

Before calling CISmon production-ready, replace this with one of:

- an ESP-IDF/Arduino certificate bundle with hostname verification;
- a maintained pinned root CA strategy;
- a local relay authenticated by a device-specific credential, with GitHub credentials kept off-device.

## Recommended hardening path

- verified TLS;
- ESP32 secure boot;
- flash encryption;
- credential rotation/reset UI;
- optional relay/webhook mode so the device never stores a GitHub token;
- local configuration authentication if the setup UI becomes available after first boot;
- explicit redaction tests ensuring tokens never enter diagnostic logs or crash telemetry.

Until those land, treat a lost CISmon as a reason to revoke its GitHub token.
