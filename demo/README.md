# V-Cycle Command Center (demo)

A static, offline browser demo that visualises this repository as an automotive
Tier-1 V-cycle: requirements intake, traceability from OEM requirement down to
unit test, and an OEM variant capability matrix.

Every number shown is derived from this checkout. Nothing is hand-written except
the two OEM requirement packages, and those only reference identifiers that
actually exist in `configuration/*.arxml` and `src/`.

## Screens

1. **Current State & Challenges** — the deck slide, with the "negative
   consequences" quantified from the repo (source/test counts, manifest
   elements, untested files, traceability gaps).
2. **Requirements Intake** — diff of `data/oem_reqs_v1.json` against
   `data/oem_reqs_v2.json`, with the ARXML elements, UDS services, DIDs and C++
   files each change touches.
3. **V-Cycle Traceability** — OEM requirement → ARXML manifest element →
   `ara::` service/module → source file → unit test. Chains missing a test or an
   implementation owner get a red gap badge. Click any node for its real file
   path and a code/XML excerpt.
4. **OEM Variant Matrix** — BMW / Audi / GM against platform capabilities
   derived from real function groups, SOME/IP services and UDS handlers, plus a
   per-directory unit-test coverage strip.

Navigate with the left nav or the ← / → arrow keys. `Esc` closes the excerpt
drawer.

## Run

```bash
cd demo
python3 -m http.server 8099
# open http://localhost:8099/
```

No build step, no npm, no CDN — plain HTML/CSS/vanilla JS, works offline.

## Regenerate the data

`data/model.json` is committed so the demo runs as-is. To rebuild it after the
repository changes:

```bash
python3 demo/generate_data.py     # from the repo root
```

The generator is Python-3 standard library only and idempotent: it re-parses
`configuration/*.arxml` (processes, function groups, machine states, service
instances, endpoints, DoIP, supervision checkpoints, DTCs), walks `src/` for
`ara::` namespaces, classes, LOC and UDS SIDs/DIDs, walks `test/` to map each
source file to its unit test, and rewrites `data/model.json`.

## Files

| Path | What it is |
| --- | --- |
| `index.html` | App shell (nav, slide frame, excerpt drawer) |
| `styles.css` | Deck design system (white page, rounded panels, single blue accent) |
| `app.js` | Renders the four screens from `data/model.json` |
| `generate_data.py` | Repository parser / model generator |
| `data/model.json` | Generated model (committed) |
| `data/oem_reqs_v1.json`, `data/oem_reqs_v2.json` | OEM requirement packages, baseline and delivery 2 |

The demo is additive: it does not touch the C++ sources, CMake or the manifests.
