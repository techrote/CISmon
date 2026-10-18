# Architecture

CISmon is split so that GitHub transport, state tracking, statistical interpretation, persistence and the CYD presentation layer are replaceable independently.

## Portable core

Everything under `include/cismon/` and `src/core/` is Arduino-independent C++17.

### Normalized model

`model.hpp` defines provider-neutral `RunSnapshot`, `JobSnapshot` and `StepSnapshot` types. Provider JSON should be discarded after conversion into this model.

`BaselineKey` deliberately separates three statistical levels:

- whole workflow run;
- job;
- step.

Its key includes repository, workflow ID and trigger event. Job and step names add progressively narrower context.

### RunTracker

`RunTracker` is the device's external working memory. A run remains visible when it is:

- queued or in progress;
- manually watched;
- failed and not acknowledged;
- a recently completed success still within the success TTL.

Incoming summary polls preserve previously fetched job detail, so a cheap workflow-list refresh cannot erase richer detail already fetched for the same run.

### RuntimeAnalyzer

`RuntimeAnalyzer` reads historical samples only through `IHistoryStore`.

This is deliberately not an ML component. Robust descriptive statistics are cheaper, deterministic, inspectable and sufficient for the first useful questions: whether a stage is unusually slow, whether it may be stuck, and how long similar stages usually take.

Only successful historical samples contribute to expected-duration baselines. Failure state itself is still retained and escalated by the tracker/UI.

### Dashboard builder

`buildDashboard()` turns tracked normalized runs plus analyzer results into ranked `DashboardItem` objects. Ranking favours:

1. action-required failures;
2. suspicious runtime anomalies;
3. interesting anomalies;
4. active work;
5. queued work;
6. recent terminal state.

The CYD UI therefore receives a small already-prioritised view model rather than implementing CI policy itself.

## Provider boundary

`ICIProvider` currently exposes a deliberately small lifecycle:

```cpp
virtual bool begin() = 0;
virtual void tick(uint64_t nowMs, RunTracker&, IHistoryStore&) = 0;
```

`GitHubActionsSource` owns GitHub-specific concerns:

- repository discovery;
- Actions REST paths;
- ETag handling;
- GitHub status/conclusion parsing;
- request spacing/rate-limit response;
- repo-level adaptive polling;
- active run detail refresh;
- low-priority historical backfill.

A future GitHub Checks provider should emit the same normalized model and need no changes to `RuntimeAnalyzer` or `CydDisplay`.

## Persistence

`FileHistoryStore` keeps an in-memory bounded deque for each timing key and an append-only LittleFS journal for reboot persistence.

The on-disk record stores only compact timing metadata:

```text
repo_hash,run_id,baseline_key_hash,duration_ms,success,completed_epoch
```

It does **not** store action logs, commit contents or secrets.

The journal also lets the backfiller tell whether a completed workflow run has already been ingested, preventing duplicate samples across reboots.

A future storage implementation can add compaction or move the same `IHistoryStore` contract to SD, FRAM or a network relay.

## Scheduler behaviour

There are two scheduling levels:

- `PollPolicy` chooses a repo's next interval according to active/queued/recent/cold/error state.
- `GitHubActionsSource::tick()` serializes actual HTTP requests and prioritises them.

Request priority is:

```text
active run job detail
    > due repository workflow list
        > completed-run history backfill
```

This prevents history acquisition from starving the thing the user is currently waiting for.

## UI boundary

`CydDisplay` is currently the hardware implementation for a common ESP32-2432S028R:

- ILI9341 display through TFT_eSPI;
- XPT2046 resistive touch on its separate SPI bus;
- active-low RGB LED.

The main screen is intentionally text-dense. It is an information radiator, not a miniature GitHub site. Touching a row enters a run/job detail view; terminal failures remain until acknowledged.

A future display abstraction can make other CYD/S3/round-display boards targets without changing the provider or analysis code.

## Data-flow example

A typical long job progresses as follows:

```text
1. repo workflow-list poll observes an in-progress run
2. RunTracker creates/updates the run
3. provider schedules high-priority job detail fetches
4. job response identifies current job/step and timestamps
5. RuntimeAnalyzer compares elapsed time to prior matching samples
6. dashboard raises normal/interesting/suspicious state
7. CYD updates row text and RGB beacon
8. workflow completes
9. final job detail is fetched through backfill/finalisation
10. run/job/step durations are journalled
11. later runs immediately benefit from the new samples
```

This feedback loop is entirely on-device after REST data acquisition.
