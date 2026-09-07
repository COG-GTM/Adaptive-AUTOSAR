# ECU Cockpit: live telemetry dashboard

The ECU cockpit is a browser dashboard which visualizes the runtime state of the
running Adaptive AUTOSAR platform simulation. Every panel is fed by the running
binary; no panel is mocked or simulated.

## One-command demo

```bash
VCC_API_KEY=$VCC_API_KEY BEARER_TOKEN=$BEARER_TOKEN ./run_cockpit.sh
```

The script configures and builds the project, and then runs the simulation with
the dashboard served at <http://127.0.0.1:8088>. Press `Ctrl+C` to terminate the
platform gracefully.

| Environment variable | Default  | Description                                    |
| -------------------- | -------- | ---------------------------------------------- |
| `DASHBOARD_PORT`     | `8088`   | Loopback TCP port of the dashboard server      |
| `DASHBOARD_ROOT`     | `./web`  | Directory which contains the dashboard assets  |

When `VCC_API_KEY` and `BEARER_TOKEN` are set, the platform runs
non-interactively and stays alive until `SIGINT`/`SIGTERM` so that the dashboard
can observe the runtime. Without those variables the original interactive
behavior (keystroke driven) is preserved.

## Telemetry path architecture

```
ara::log::Logger ─┐
                  │  LoggingFramework::Log
                  v
       TelemetryLogSink ──► ConsoleLogSink / FileLogSink   (unchanged output)
                  │
                  │ parsed structured record
                  v
ExecutionServer ─►┌──────────────────┐   SerializeSnapshot(since)   ┌───────────┐
StateServer    ─► │ TelemetryHub     │ ───────────────────────────► │ Telemetry │
PHM            ─► │ (process-wide,   │                              │ Server    │
RpcClient/Server► │  thread-safe)    │                              └─────┬─────┘
                  └──────────────────┘                                    │
                                                    HTTP + server-sent events
                                                                          v
                                                                 web/ cockpit UI
```

- **`ara::log::sink::TelemetryLogSink`** decorates the configured console or file
  sink. It first delegates the log stream to the wrapped sink — so console and
  file logging behavior is byte-for-byte unchanged — and then parses the
  logger-stamped string (`Context ID:…;Context Description:…;Log Level:…;message`)
  into a structured record that is published to the hub.
- **`ara::telemetry::TelemetryHub`** is the process-wide, mutex-protected store of
  the runtime state: execution states, function group states and transitions,
  PHM checkpoints and supervision statuses, SOME/IP records, and the log tail.
  Records carry a monotonic sequence number so that streaming clients receive
  only what they have not seen. Buffers are bounded (256 logs, 64 SOME/IP
  messages, 32 transitions). Publishing is a no-op while the hub is disabled,
  which keeps the platform free of telemetry cost when no dashboard is served.
- **`ara::telemetry::TelemetryServer`** binds a loopback socket, enables the hub,
  serves the static dashboard assets, exposes `GET /api/telemetry` for a JSON
  snapshot, and streams snapshots every 200 ms over `GET /api/stream` using
  server-sent events. Server-sent events are used instead of WebSockets because
  they need no framing library and no new dependency; JSON serialization reuses
  the JsonCpp dependency already used by the Extended Vehicle application.

### Instrumentation points

| Source                                       | Telemetry                                             |
| -------------------------------------------- | ----------------------------------------------------- |
| `ExecutionServer::handleExecutionStateReport` | Application `ExecutionState`                          |
| `StateServer` constructor and `handleSetState`| Function group states and state transitions           |
| `PlatformHealthManagement::fillCheckpoints`   | Checkpoint IDs and short-names from the ARXML manifest |
| `PlatformHealthManagement::onReportCheckpoint`| Per-checkpoint reports, alive/deadline supervision status |
| `PlatformHealthManagement::onGlobalStatusChanged` | Global supervision status and dominant supervision |
| `RpcClient::Send` / `RpcServer::TryInvokeHandler` | SOME/IP message records, totals, and rolling rate |

## Dashboard panels

- **Global supervision** — `OK` / `FAILED` / `EXPIRED` / `DEACTIVATED` plus the
  status of every elementary supervision.
- **Execution management** — every application which reported an execution state.
- **State management** — the current state of each function group and a log of
  the recent transitions.
- **PHM checkpoints** — every checkpoint of
  `configuration/health_monitoring_manifest.arxml` with its report count and the
  time of the last report.
- **SOME/IP** — rolling message rate over a 5 s window, the total message count,
  and a scrolling list of the recent messages.
- **Log tail** — the structured log records with severity coloring.

## HTTP API

| Endpoint                  | Description                                            |
| ------------------------- | ------------------------------------------------------ |
| `GET /`                   | Dashboard single-page UI                               |
| `GET /api/telemetry?since=N` | JSON snapshot; sequence-based delta when `since` is given |
| `GET /api/stream`         | `text/event-stream` of JSON snapshots every 200 ms      |

The server binds `127.0.0.1` only and serves `GET` requests exclusively.

## External API limitation

The Extended Vehicle application calls the Volvo Extended Vehicle REST API. With
the credentials available in this environment the call fails with
`Setting the VIN failed due to unexpected RESTful response format.`, which is
logged as an error and shown in the dashboard log tail. The failure is local to
that application: execution management, state management, PHM, SOME/IP, and
therefore every dashboard panel keep working against the locally running
platform.

## Tests

`test/ara/telemetry/telemetry_hub_test.cpp`,
`test/ara/telemetry/telemetry_server_test.cpp`, and
`test/ara/log/sink/telemetry_log_sink_test.cpp` are part of the `ara_unit_test`
target and are discovered by CTest:

```bash
cd build && ctest -C Debug --output-on-failure
```
