# CISmon

**CISmon** is a desk-side CI monitor for the ESP32 "Cheap Yellow Display" (CYD). Its job is not to reproduce GitHub's Actions UI. It is an external working-memory appliance for long-running automation:

> What is running? What is taking too long? What just broke? Is there anything I need to care about?

The proof of concept polls GitHub Actions directly, remembers active work after the browser tab is gone, learns normal run/job/step timings from previous executions, and raises attention when current work diverges from its own history.

## Current PoC

- Targets the common **ESP32-2432S028R / 2.8-inch CYD** in 320x240 landscape mode.
- First-boot captive portal for Wi-Fi, GitHub token, owner/repository filters and history depth.
- Automatic repository discovery from the authenticated GitHub account.
- Adaptive repository polling with `ETag` / `If-None-Match` support.
- Active run -> job -> step detail polling, prioritised ahead of history backfill.
- Persistent LittleFS timing journal; no CI logs are stored.
- Automatic background backfill of recent completed workflow job/step timing data.
- Median, quartile, MAD and EWMA baselines on-device.
- `NORMAL`, `INTERESTING`, `SUSPICIOUS` and `ACTION REQUIRED` attention states.
- Rough remaining-time estimates once enough history exists.
- "Possible stall" detection for runs/stages well outside their robust historical baseline.
- Touch drill-down into the currently tracked workflow and jobs.
- Persistent unacknowledged failures; successful completions age off the main view.
- CYD RGB LED as an across-the-room state beacon.
- Portable C++ core with host tests; GitHub Actions builds the actual ESP32 firmware.

## How the timing model works

History is keyed narrowly enough to compare like with like:

```text
repository
+ workflow_id
+ GitHub event (pull_request / push / ...)
+ job name
+ step name
```

Run, job and step durations are journalled independently. Only successful historical observations are used for timing baselines, so a failed two-hour hang does not poison the expected runtime of later successful work.

For a current stage CISmon computes:

- median runtime;
- P25/P75;
- median absolute deviation (MAD);
- an EWMA showing recent movement;
- current/median ratio;
- robust z-score;
- a coarse remaining-time estimate.

The present PoC escalates roughly as follows once at least three comparable samples exist:

```text
>= 1.30x median or robust-z >= 2.5  -> INTERESTING
>= 1.75x median or robust-z >= 4.0  -> SUSPICIOUS
>= 3.00x median or robust-z >= 6.0  -> SUSPICIOUS + possible stall while active
failed/timed-out/action-required     -> ACTION REQUIRED
```

These thresholds live in the portable analysis layer and are deliberately easy to replace with percentile or workflow-specific policies later.

## First boot

1. Flash the firmware.
2. Join the `CISmon-xxxx` access point shown on screen.
3. Open `http://192.168.4.1/` if the captive page does not open automatically.
4. Enter Wi-Fi details and a **fine-grained, read-only GitHub token** with access only to repositories you want CISmon to see.
5. Optionally set an owner and include/exclude globs such as `techrote/*`.
6. CISmon reboots, discovers repositories, then starts monitoring and backfilling history.

The PoC stores credentials in LittleFS. Read [docs/SECURITY.md](docs/SECURITY.md) before treating the current credential path as production-grade.

## Build

Install PlatformIO, then:

```bash
pio run -e esp32-cyd
pio run -e esp32-cyd -t upload
pio device monitor -b 115200
```

The display/touch pinout is configured in `platformio.ini` for the common ESP32-2432S028R. Touch calibration values are compile-time defines there and may need adjustment for a different CYD batch.

Portable analysis/tracking code can be tested without Arduino dependencies:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude \
  src/core/*.cpp test/host_core.cpp -o /tmp/cismon-core-tests
/tmp/cismon-core-tests
```

## Architecture

```text
                         +-----------------------+
                         |   GitHub Actions API  |
                         +-----------+-----------+
                                     |
                           conditional REST polling
                                     |
              +----------------------v----------------------+
              | GitHubActionsSource : ICIProvider           |
              | repo discovery / runs / jobs / step parsing |
              +----------+----------------------+-----------+
                         |                      |
                    current state          completed samples
                         |                      |
                 +-------v------+       +-------v---------+
                 |  RunTracker  |       | FileHistoryStore |
                 +-------+------+       | LittleFS journal |
                         |              +-------+---------+
                         |                      |
                         +----------+-----------+
                                    |
                            +-------v--------+
                            | RuntimeAnalyzer |
                            | median/MAD/EWMA |
                            +-------+--------+
                                    |
                           +--------v---------+
                           | Dashboard builder |
                           +--------+---------+
                                    |
                         +----------v-----------+
                         | CYD TFT / touch / RGB |
                         +----------------------+
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for module boundaries and extension points.

## Polling behaviour

Requests are serialized and spaced rather than burst continuously. The default policy is approximately:

| Repository state | Poll interval |
| --- | ---: |
| Active workflow | 15 s |
| Queued workflow | 20 s |
| Recently active | 60 s |
| Idle | 120 s |
| Cold | 300 s |
| Errors | exponential backoff, capped at 10 min |

Active job detail refreshes take priority over repository polling, which takes priority over historical backfill. That means history learning should not make a currently-running three-hour qualification job less responsive on the screen.

## Extension points

The important boundary is `cismon::ICIProvider`. `GitHubActionsSource` is only the first implementation. Natural follow-ons are:

- GitHub Checks adapter for external CI providers reporting check runs;
- CircleCI or other native provider adapters;
- webhook/relay source for installations that do not want device polling;
- multiple CYDs sharing a compact relay feed;
- per-workflow analysis policy plugins;
- richer trend/regression detection (for example, "this workflow became 18% slower after commit X").

The display consumes normalized run state rather than GitHub JSON, so new providers should not require a UI rewrite.

## Known PoC limitations

- GitHub Actions is the only provider implemented so far.
- Repository discovery reads the first 100 repositories sorted by recently pushed, then applies the configured cap/filter.
- Job listing currently uses the first 100 jobs of the latest workflow attempt.
- History is an append-only LittleFS journal; compaction/retention policy is a follow-up.
- Touch calibration is a common CYD default, not runtime-calibrated yet.
- Credentials are stored locally and the current HTTPS client uses insecure certificate verification for PoC compatibility. **Use a least-privilege read-only token.** See `docs/SECURITY.md`.
- No OTA update flow yet.

## Why not a backend first?

The standalone device is intentional. For one desk display, GitHub is already the source of truth and the data rate is tiny. A relay/webhook service becomes worthwhile when there are several displays, very large repo sets, or a requirement to remove GitHub credentials from the device. The provider boundary leaves that path open without making it a prerequisite for the first useful firmware.
