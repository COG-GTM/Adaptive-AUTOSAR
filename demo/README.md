# A repository-grounded AUTOSAR verification command center with modeled OEM variants

A static, offline browser demo that joins two sources of truth about this
repository:

- **repository-grounded facts** — parsed out of this checkout by
  `generate_data.py` (`configuration/*.arxml`, `src/`, `test/`);
- **execution evidence** — recorded by `runner/run_scenarios.py`, which builds
  the platform, boots it, and drives real SOME/IP, DoIP/UDS and ctest scenarios
  against it.

Anything that is neither of those — the BMW / Audi / GM profiles and the two OEM
requirement packages — is **modeled** and labelled as such everywhere it
appears. Every number on every screen carries an `R` (repository), `E`
(execution) or `M` (modeled) provenance mark, explained by the legend in the
header.

## Screens

1. **Current State & Challenges** — the deck slide, quantified from the repo and
   from the selected run.
2. **Requirements Intake** — diff of `data/oem_reqs_v1.json` against
   `data/oem_reqs_v2.json` (both modeled), with the ARXML elements, UDS
   services, DIDs and C++ files each change touches (all real).
3. **V-Cycle Traceability** — OEM requirement → ARXML manifest element →
   `ara::` service/module → source file → unit test. Click any node for its real
   file path and a code/XML excerpt.
4. **Modeled OEM Variant Profiles** — modeled profiles against platform
   capabilities derived from real function groups, SOME/IP services and UDS
   handlers. Every badge is derived at load time; click one for the evidence.
5. **Execution Evidence** — the selected run itself: host, commit, start mode,
   and every scenario with its command, exit code, duration, assertions and
   platform log excerpt.

Navigate with the left nav or the ← / → arrow keys. `Esc` closes the drawer.

## Run the dashboard

```bash
cd demo
python3 -m http.server 8099
# open http://localhost:8099/
```

No build step, no npm, no CDN — plain HTML/CSS/vanilla JS, works offline.

## Run the scenarios

From the repository root:

```bash
python3 demo/runner/run_scenarios.py
```

Python 3 standard library only. The runner:

1. builds with `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --config Debug -j$(nproc)`;
2. runs the unit suite (`cd build && ctest -C Debug --output-on-failure`) as a scenario;
3. starts `build/bin/adaptive_autosar` with the four manifests and drives it:
   function-group state transitions, SOME/IP Service Discovery (the multicast
   group is joined *before* boot so the initial `FindService` is captured),
   SOME/IP RPC on TCP 8080, and DoIP + UDS `ReadDataByIdentifier (0x22)` on the
   DIDs implemented in `src/application/helper/` over TCP 8081.

Useful flags: `--skip-build`, `--skip-tests`, `--out-dir`, `--timeout-scale`.

`main.cpp` skips its blocking `getchar()` calls when `VCC_API_KEY` and
`BEARER_TOKEN` are both set in the environment, which makes an env-var start
exit right after initialisation. To keep a real platform alive for the socket
scenarios the runner starts the binary on a pseudo terminal and answers its
prompts with the same secret values; secrets are never echoed into the artifact.
If the credentials are missing the run is recorded as **degraded** (the socket
scenarios are skipped with that reason) — the runner never prompts and never
hangs. Every scenario is timeout-bounded and the simulator's process group is
always terminated, including on failure.

## Artifacts and how badges are derived

Each run writes `data/runs/<UTC-ISO8601>.json` and rewrites
`data/runs/index.json` (newest first, used by the run selector in the header).
An artifact holds the run's host, commit, branch, start mode, degraded flag, and
per scenario: command, exit code, duration, assertions, stdout/stderr tail and
the platform log excerpt.

The dashboard loads `data/model.json` plus the selected artifact and derives
every capability badge at load time — nothing is hardcoded:

| State | Meaning |
| --- | --- |
| `verified` | a scenario in the selected run exercised the capability and passed |
| `observed` | present in the repository/manifests, but no scenario in this run touched it |
| `gap` | declared or expected, but missing an implementation, a test, or a passing run (a failed scenario always wins) |

Scenarios declare which capabilities they cover, so a failing scenario turns
every capability it covers red. The committed run does contain failures — the
extended-vehicle REST call cannot resolve a VIN in this environment, so the DoIP
server is never constructed and both the SOME/IP offer/subscribe and the
UDS-over-DoIP scenarios fail. Those are surfaced as truthful gaps rather than
hidden.

## Regenerate the static model

```bash
python3 demo/generate_data.py     # from the repo root
```

Standard library only and idempotent: it re-parses `configuration/*.arxml`,
walks `src/` for `ara::` namespaces, classes, LOC and UDS SIDs/DIDs, maps each
source file to its unit test, and rewrites `data/model.json`.

## Files

| Path | What it is |
| --- | --- |
| `index.html` | App shell (nav, legend, run selector, slide frame, evidence drawer) |
| `styles.css` | Deck design system (white page, rounded panels, single blue accent) |
| `app.js` | Renders the five screens by joining `data/model.json` with the selected run |
| `generate_data.py` | Repository parser / static model generator |
| `runner/run_scenarios.py` | Execution runner (build, ctest, platform, SOME/IP, DoIP/UDS) |
| `data/model.json` | Generated static model (committed) |
| `data/runs/*.json` | Recorded run artifacts, plus `index.json` |
| `data/oem_reqs_v1.json`, `data/oem_reqs_v2.json` | Modeled OEM requirement packages |

The demo is additive: it does not touch the C++ sources, CMake or the manifests.
