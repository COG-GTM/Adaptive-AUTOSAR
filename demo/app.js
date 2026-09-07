/* A repository-grounded AUTOSAR verification command center with modeled OEM variants.
   Static facts come from demo/data/model.json (parsed from this checkout), execution
   evidence from demo/data/runs/*.json (written by demo/runner/run_scenarios.py).
   Badge states are derived at load time by joining the two — never hardcoded. */
(function () {
  "use strict";

  var model = null;
  var runIndex = [];
  var run = null;
  var current = 0;

  var SLIDES = [
    { title: "Current State & Challenges", nav: "Current State", render: renderCurrentState },
    { title: "Requirements Intake", nav: "Requirements Intake", render: renderIntake },
    { title: "V-Cycle Traceability", nav: "V-Cycle Traceability", render: renderTrace },
    { title: "Modeled OEM Variant Profiles", nav: "Variant Profiles", render: renderMatrix },
    { title: "Execution Evidence", nav: "Execution Evidence", render: renderRun }
  ];

  var PROVENANCE = {
    repo: { letter: "R", label: "Repository-grounded", hint: "parsed from this checkout" },
    run: { letter: "E", label: "Execution evidence", hint: "measured by a recorded run" },
    model: { letter: "M", label: "Modeled assumption", hint: "modeled, not supplied by an OEM" }
  };

  var STATE_COPY = {
    verified: "a scenario in this run exercised the capability and passed",
    observed: "present in the repository/manifests, not exercised by this run",
    gap: "declared or expected, but missing an implementation, a test, or a passing run"
  };

  var el = {
    nav: document.getElementById("nav"),
    title: document.getElementById("slide-title"),
    body: document.getElementById("slide-body"),
    foot: document.getElementById("foot-right"),
    legend: document.getElementById("legend"),
    runbar: document.getElementById("runbar"),
    drawer: document.getElementById("drawer"),
    drawerKind: document.getElementById("drawer-kind"),
    drawerTitle: document.getElementById("drawer-title"),
    drawerPath: document.getElementById("drawer-path"),
    drawerBody: document.getElementById("drawer-body")
  };

  function esc(text) {
    return String(text == null ? "" : text)
      .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
  }

  function pct(value) { return Math.round(value * 100) + "%"; }

  function mark(kind) {
    var provenance = PROVENANCE[kind];
    return '<span class="prov ' + kind + '" title="' + esc(provenance.label + " — " + provenance.hint) +
      '">' + provenance.letter + "</span>";
  }

  function ms(value) {
    if (value == null) { return "—"; }
    return value >= 1000 ? (value / 1000).toFixed(1) + " s" : value + " ms";
  }

  // ------------------------------------------------------------------ nav
  function buildNav() {
    el.nav.innerHTML = SLIDES.map(function (slide, index) {
      return '<button class="nav-item' + (index === current ? " active" : "") +
        '" data-index="' + index + '"><span class="n">0' + (index + 1) +
        '</span><span>' + esc(slide.nav) + "</span></button>";
    }).join("") + '<div class="nav-hint">← → to navigate</div>';
    Array.prototype.forEach.call(el.nav.querySelectorAll(".nav-item"), function (button) {
      button.addEventListener("click", function () { go(Number(button.dataset.index)); });
    });
  }

  function go(index) {
    current = (index + SLIDES.length) % SLIDES.length;
    closeDrawer();
    if (location.hash !== "#" + (current + 1)) { location.hash = "#" + (current + 1); }
    buildNav();
    el.title.textContent = SLIDES[current].title;
    el.body.innerHTML = "";
    el.body.style.animation = "none";
    void el.body.offsetWidth;
    el.body.style.animation = "";
    SLIDES[current].render(el.body);
    el.foot.textContent = "0" + (current + 1) + " / 0" + SLIDES.length;
  }

  // ------------------------------------------------------- legend & run bar
  function renderLegend() {
    el.legend.innerHTML = Object.keys(PROVENANCE).map(function (kind) {
      return '<span class="legend-item">' + mark(kind) + esc(PROVENANCE[kind].label) +
        ' <em>' + esc(PROVENANCE[kind].hint) + "</em></span>";
    }).join("");
  }

  function renderRunBar() {
    if (!run) {
      el.runbar.innerHTML = '<span class="run-meta">no recorded run found in data/runs</span>';
      return;
    }
    var commit = run.commit || {};
    var host = run.host || {};
    var options = runIndex.map(function (entry) {
      return '<option value="' + esc(entry.runId) + '"' +
        (entry.runId === run.runId ? " selected" : "") + ">" + esc(entry.startedAt) +
        " · " + entry.summary.passed + "/" + entry.summary.total + " passed</option>";
    }).join("");
    el.runbar.innerHTML =
      '<span class="micro">Recorded run</span>' +
      '<select class="run-select" id="run-select" aria-label="Recorded run">' + options + "</select>" +
      '<span class="run-meta">' + mark("run") + esc(run.startedAt) + " · " + esc(host.hostname || "?") +
      " · " + esc((commit.shortSha || "?") + (commit.dirty ? "+local" : "")) +
      " · " + run.summary.passed + " passed / " + run.summary.failed + " failed" +
      (run.environment && run.environment.degraded ? " · degraded" : "") + "</span>";
    var select = document.getElementById("run-select");
    select.addEventListener("change", function () { loadRun(select.value).then(function () { go(current); }); });
  }

  // --------------------------------------------------------------- drawer
  /* `title` may contain provenance markers, so it is passed through as HTML. */
  function section(title, html) {
    return '<section class="drawer-section"><h3>' + title + "</h3>" + html + "</section>";
  }

  function pre(text) { return '<pre class="excerpt">' + esc(text || "(no output captured)") + "</pre>"; }

  function openDrawer(kind, title, path, bodyHtml) {
    el.drawerKind.textContent = kind;
    el.drawerTitle.textContent = title;
    el.drawerPath.textContent = path || "";
    el.drawerBody.innerHTML = bodyHtml;
    el.drawer.hidden = false;
  }

  function openExcerptDrawer(kind, title, path, excerpt) {
    openDrawer(kind, title, path, section("Repository evidence " + mark("repo"), pre(excerpt)));
  }

  function closeDrawer() { el.drawer.hidden = true; }
  document.getElementById("drawer-close").addEventListener("click", closeDrawer);
  el.drawer.addEventListener("click", function (event) {
    if (event.target === el.drawer) { closeDrawer(); }
  });
  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape") { closeDrawer(); }
    else if (event.key === "ArrowRight") { go(current + 1); }
    else if (event.key === "ArrowLeft") { go(current - 1); }
  });

  // ------------------------------------------------- static/dynamic join
  function scenariosFor(capability) {
    if (!run) { return []; }
    return run.scenarios.filter(function (scenario) {
      return (scenario.capabilities || []).indexOf(capability) !== -1;
    });
  }

  /* Derives verified / observed / gap for a capability by joining the static model
     with the selected run. A failing scenario always wins over a passing one: an
     honest red badge beats a green one. */
  function deriveState(row) {
    var scenarios = scenariosFor(row.id);
    var failed = scenarios.filter(function (s) { return s.status === "fail"; });
    var passed = scenarios.filter(function (s) { return s.status === "pass"; });
    if (failed.length) {
      return {
        state: "gap", scenarios: scenarios,
        why: failed[0].name + " failed (exit " + (failed[0].exitCode == null ? "—" : failed[0].exitCode) + ")"
      };
    }
    if (passed.length) {
      return {
        state: "verified", scenarios: scenarios,
        why: passed.length + " passing scenario" + (passed.length === 1 ? "" : "s") + " in this run"
      };
    }
    if (!row.tested) {
      return { state: "gap", scenarios: scenarios, why: "no unit test and no scenario in this run" };
    }
    return {
      state: "observed", scenarios: scenarios,
      why: row.files.length + " source files, " + row.tested + " under test, not exercised by this run"
    };
  }

  function derivedStates() {
    var states = {};
    model.matrix.rows.forEach(function (row) { states[row.id] = deriveState(row); });
    return states;
  }

  function stateCounts(states) {
    var counts = { verified: 0, observed: 0, gap: 0 };
    Object.keys(states).forEach(function (id) { counts[states[id].state] += 1; });
    return counts;
  }

  function badge(state, label) {
    return '<span class="state ' + state + '">' + esc(label || state) + "</span>";
  }

  function scenarioEvidenceHtml(scenario) {
    var assertions = (scenario.assertions || []).map(function (assertion) {
      return '<li class="' + (assertion.ok ? "ok" : "bad") + '"><b>' + esc(assertion.name) + "</b>" +
        '<span>expected ' + esc(assertion.expected) + " · actual " + esc(assertion.actual) + "</span></li>";
    }).join("");
    return '<div class="ev-scenario">' +
      '<div class="ev-head">' + badge(scenario.status === "pass" ? "verified" :
        scenario.status === "fail" ? "gap" : "observed", scenario.status) +
      "<b>" + esc(scenario.name) + "</b>" +
      '<span class="ev-meta">exit ' + (scenario.exitCode == null ? "—" : scenario.exitCode) +
      " · " + ms(scenario.durationMs) + "</span></div>" +
      '<div class="ev-cmd">' + esc(scenario.command) + "</div>" +
      (assertions ? '<ul class="ev-asserts">' + assertions + "</ul>" : "") +
      (scenario.notes ? '<p class="ev-note">' + esc(scenario.notes) + "</p>" : "") +
      ((scenario.logExcerpt && scenario.logExcerpt.length)
        ? pre(scenario.logExcerpt.slice(0, 8).join("\n"))
        : pre(scenario.stdoutTail)) +
      "</div>";
  }

  function openCapabilityDrawer(row, derived, oem) {
    var staticEvidence =
      '<ul class="ev-files">' +
      (row.evidence || []).map(function (item) {
        return "<li>" + mark("repo") + esc(item) + "</li>";
      }).join("") +
      row.files.slice(0, 8).map(function (file) {
        return "<li>" + mark("repo") + esc(file) + "</li>";
      }).join("") +
      (row.files.length > 8 ? '<li class="muted">+' + (row.files.length - 8) + " more source files</li>" : "") +
      "</ul>";

    var dynamic = derived.scenarios.length
      ? derived.scenarios.map(scenarioEvidenceHtml).join("")
      : '<p class="ev-note">No scenario in this run exercised this capability.</p>';

    var oemNote = oem
      ? section("Modeled OEM profile " + mark("model"),
        '<p class="ev-note"><b>' + esc(oem.name) + "</b> — " + esc(oem.note) +
        ". Modeled from public AUTOSAR/UDS conventions; not a supplied OEM requirement package.</p>")
      : "";

    openDrawer("Badge evidence · " + derived.state, row.name,
      derived.why,
      '<p class="drawer-lede">' + badge(derived.state) + esc(STATE_COPY[derived.state]) + "</p>" +
      oemNote +
      section("Execution evidence " + mark("run") + " · run " + (run ? run.runId : "—"), dynamic) +
      section("Repository evidence " + mark("repo") + " · " + row.files.length + " files, " +
        row.tested + " with a unit test", staticEvidence));
  }

  // ------------------------------------------------------- slide 1
  function renderCurrentState(root) {
    var c = model.counts;
    var states = derivedStates();
    var counts = stateCounts(states);
    var untestedLoc = model.untested.reduce(function (sum, item) { return sum + item.loc; }, 0);
    var worst = model.modules.filter(function (m) { return m.files >= 4; })
      .sort(function (a, b) { return a.coverage - b.coverage; })[0];

    var left = [
      ["Safety-critical hardware", " + embedded software for OEMs (BMW, Audi, GM " + mark("model") + " modeled profiles)"],
      ["Full V-cycle", ": OEM requirements, architecture, SW requirements, implementation, test, validation"],
      ["Stack", ": Adaptive AUTOSAR (" + c.sourceFiles + " " + mark("repo") + " C++ files, " +
        c.sourceLoc.toLocaleString() + " " + mark("repo") + " LOC), SOME/IP, DoIP/UDS, POSIX/Linux; manifests in ARXML"],
      ["Configuration", ": " + c.manifests + " " + mark("repo") + " ARXML manifests, " + c.manifestElements +
        " " + mark("repo") + " deployment elements, " + c.functionGroups + " " + mark("repo") +
        " function group, " + c.checkpoints + " " + mark("repo") + " supervision checkpoints"],
      ["Global system engineering", " (NA, Europe, Poland, India)"]
    ];

    var right = [
      ["Requirements intake", ": diffing OEM packages and assessing implementation <b>takes 2-3 weeks</b> " +
        mark("model") + " — " + c.changedRequirements + " " + mark("model") + " of " +
        (c.tracedRequirements + 1) + " modeled requirements moved in the last delivery"],
      ["AUTOSAR/ECU integration", ": ~1 month of team effort per request " + mark("model") + " across " +
        c.serviceInstances + " " + mark("repo") + " SOME/IP service instances and " + c.udsServices +
        " " + mark("repo") + " UDS services"],
      ["Traceability gaps", " across the V-cycle: <span class='risk'>" + c.requirementGaps + " of " +
        c.tracedRequirements + "</span> " + mark("model") +
        " modeled requirement chains break before a unit test or a manifest element"],
      ["Unverified capabilities", ": <span class='risk'>" + counts.gap + " of " +
        model.matrix.rows.length + "</span> " + mark("run") +
        " platform capabilities have no passing scenario in the selected run"],
      ["OEM variants", " growing a “giant codebase”; <span class='risk'>" + c.untestedFiles +
        " of " + c.sourceFiles + "</span> " + mark("repo") + " source files (" + pct(1 - c.coverageRatio) +
        ") have no corresponding unit test — worst module <b>" + worst.module + "</b> at " +
        pct(worst.coverage)]
    ];

    root.innerHTML =
      '<div class="cols">' +
        '<section class="panel">' +
          "<h2>Current State</h2>" + bullets(left) +
          '<div class="caption">Aptiv ADAS environment · repository facts parsed from COG-GTM/Adaptive-AUTOSAR</div>' +
        "</section>" +
        '<div class="arrow">→</div>' +
        '<section class="panel accent">' +
          "<h2>Negative Consequences</h2>" + bullets(right) +
          '<div class="caption">Every figure is marked ' + mark("repo") + " repository, " + mark("run") +
          " execution or " + mark("model") + " modeled</div>" +
        "</section>" +
      "</div>" +
      '<div class="stat-row">' +
        stat(c.testCases.toLocaleString() + mark("repo"), c.testFiles + " gtest files, " + c.testCases + " cases") +
        stat((run ? run.summary.passed + " / " + run.summary.total : "—") + mark("run"),
          "scenarios passing in the selected recorded run") +
        stat(counts.verified + " · " + counts.observed + " · " + counts.gap + mark("run"),
          "capabilities verified · observed · gap (derived at load time)") +
        stat(c.untestedFiles + " / " + c.sourceFiles + mark("repo"), "source files with no unit test (" +
          untestedLoc.toLocaleString() + " LOC in the top 40)") +
      "</div>";
  }

  function bullets(items) {
    return '<ul class="bullets">' + items.map(function (item) {
      return "<li><b>" + esc(item[0]) + "</b>" + item[1] + "</li>";
    }).join("") + "</ul>";
  }

  function stat(value, key) {
    return '<div class="stat"><div class="v">' + value + '</div><div class="k">' + esc(key) + "</div></div>";
  }

  // ------------------------------------------------------- slide 2
  function renderIntake(root) {
    var v1 = model.requirementPackages.v1;
    var v2 = model.requirementPackages.v2;
    var diff = model.requirementDiff.slice().sort(function (a, b) {
      var order = { added: 0, changed: 1, removed: 2, unchanged: 3 };
      return order[a.status] - order[b.status] || a.id.localeCompare(b.id);
    });
    var moved = diff.filter(function (item) { return item.status !== "unchanged"; }).length;

    root.innerHTML =
      '<div class="lede"><span><b>Manual:</b> 2-3 weeks of package diffing and impact assessment ' +
      mark("model") + "</span>" +
      '<span class="flip">→ This view: seconds</span>' +
      "<span>" + moved + " " + mark("model") +
      " modeled requirements moved · impact resolved against real ARXML and C++ " + mark("repo") + "</span></div>" +
      '<div class="synthetic-banner">' + mark("model") +
      "<b>Synthetic requirement packages.</b> Both deliveries below are modeled from public " +
      "AUTOSAR/UDS conventions for this demo — they are not supplied OEM requirement packages. " +
      "Every impact link, file path and test on the right-hand side is real and parsed from this checkout." +
      "</div>" +
      '<div class="diff-head">' +
        '<div class="pkg"><b>' + esc(v1.release) + "</b> · " + esc(v1.issued) + " · " +
          v1.requirements + " requirements " + mark("model") + " · modeled</div>" +
        '<div class="pkg"><b>' + esc(v2.release) + "</b> · " + esc(v2.issued) + " · " +
          v2.requirements + " requirements " + mark("model") + " · modeled</div>" +
      "</div>" +
      '<div class="diff-list">' + diff.map(diffRow).join("") + "</div>";

    bindChips(root);
  }

  function diffRow(item) {
    var before = item.before
      ? '<div class="req-text' + (item.status === "changed" ? " old" : "") + '">' + esc(item.before) + "</div>"
      : '<div class="req-text old">— not in baseline —</div>';
    var after = item.after
      ? '<div class="req-text">' + esc(item.after) + "</div>"
      : '<div class="req-text old">— dropped in delivery 2 —</div>';
    var asil = "";
    if (item.asilAfter && item.asilBefore && item.asilAfter !== item.asilBefore) {
      asil = ' <span class="tag changed">ASIL ' + esc(item.asilBefore) + " → " + esc(item.asilAfter) + "</span>";
    } else if (item.asilAfter) {
      asil = ' <span class="tag">ASIL ' + esc(item.asilAfter) + "</span>";
    }
    if (item.status === "unchanged") {
      return '<div class="diff-row unchanged">' +
        '<div><div class="tag unchanged">unchanged ' + mark("model") + "</div>" +
          '<div class="req-id">' + esc(item.id) + "</div></div>" +
        '<div class="req-text">carried over from baseline</div>' +
        '<div><div class="req-title">' + esc(item.title) + asil + "</div></div></div>";
    }
    return '<div class="diff-row ' + item.status + '">' +
      '<div><div class="tag ' + item.status + '">' + item.status + " " + mark("model") + "</div>" +
        '<div class="req-id">' + esc(item.id) + "</div></div>" +
      "<div>" + before + "</div>" +
      '<div><div class="req-title">' + esc(item.title) + asil + "</div>" + after +
        '<div class="impacts">' + item.impacts.map(impactChip).join("") + "</div></div>" +
      "</div>";
  }

  function impactChip(impact) {
    var gap = impact.type === "source" && !impact.test;
    var label = impact.type === "source"
      ? impact.label + (gap ? " · no test" : " · " + impact.test.split("/").pop())
      : impact.type.toUpperCase() + " " + impact.label;
    return '<span class="chip' + (gap ? " gap" : "") + '" data-file="' + esc(impact.file || "") +
      '" data-line="' + (impact.line || 1) + '" data-kind="' + esc(impact.type) + '">' + esc(label) + "</span>";
  }

  function bindChips(root) {
    Array.prototype.forEach.call(root.querySelectorAll(".chip"), function (chip) {
      chip.addEventListener("click", function () {
        var file = chip.dataset.file;
        if (!file) { return; }
        openExcerptDrawer(chip.dataset.kind + " impact", chip.textContent,
          file + ":" + chip.dataset.line, excerptFor(file, Number(chip.dataset.line)));
      });
    });
  }

  // ------------------------------------------------------- slide 3
  var STAGES = [
    ["requirement", "OEM Requirement", "model"],
    ["manifest", "ARXML Manifest Element", "repo"],
    ["service", "ara:: Service / Module", "repo"],
    ["source", "Source File", "repo"],
    ["test", "Unit Test", "run"]
  ];

  function renderTrace(root) {
    var chains = model.traceability;
    var broken = chains.filter(function (chain) { return chain.gaps.length; }).length;
    var states = derivedStates();

    root.innerHTML =
      '<div class="lede"><span><b>' + chains.length + "</b> " + mark("model") +
      " modeled requirement chains resolved across five V-cycle stages</span>" +
      '<span class="flip">' + broken + " chains carry a gap badge</span>" +
      "<span>the unit-test column is green only where the selected run executed it " + mark("run") + "</span></div>" +
      '<div class="trace-head">' + STAGES.map(function (stage) {
        return '<div class="micro">' + esc(stage[1]) + " " + mark(stage[2]) + "</div>";
      }).join("") + "</div>" +
      '<div class="chains">' + chains.map(function (chain) {
        return chainRow(chain, states[chain.capability]);
      }).join("") + "</div>";

    Array.prototype.forEach.call(root.querySelectorAll(".node:not(.static)"), function (node) {
      node.addEventListener("click", function () {
        openExcerptDrawer(node.dataset.kind, node.dataset.label,
          node.dataset.file ? node.dataset.file + ":" + node.dataset.line : "",
          node.dataset.excerpt);
      });
    });
  }

  function chainRow(chain, derived) {
    return '<div class="chain">' + STAGES.map(function (stage) {
      var nodes = chain.stages[stage[0]] || [];
      if (!nodes.length) {
        var gap = stage[0] === "test" ? "no unit test"
          : stage[0] === "manifest" ? "no manifest element"
          : stage[0] === "source" ? "no implementation owner" : "not linked";
        var badgeHtml = (stage[0] === "service")
          ? '<div class="node static"><div class="l">—</div>' +
            '<div class="s">no dedicated service id</div></div>'
          : '<div class="gap-badge">' + gap + "</div>";
        return '<div class="cell empty">' + badgeHtml + "</div>";
      }
      var shown = nodes.slice(0, 2);
      var more = nodes.length - shown.length;
      var runState = stage[0] === "test" && derived
        ? '<div class="cell-state ' + derived.state + '">' + derived.state + " " + mark("run") + "</div>"
        : "";
      return '<div class="cell">' + shown.map(function (node) {
        return '<div class="node' + (stage[0] === "requirement" ? " req" : "") +
          '" data-kind="' + esc(stage[1]) + '" data-label="' + esc(node.label) +
          '" data-file="' + esc(node.file || "") + '" data-line="' + (node.line || 1) +
          '" data-excerpt="' + esc(node.excerpt || "") + '">' +
          '<div class="l">' + esc(node.label) + "</div>" +
          '<div class="s">' + esc(node.sub || "") + "</div></div>";
      }).join("") + (more > 0 ? '<div class="more">+' + more + " more</div>" : "") + runState + "</div>";
    }).join("") + "</div>";
  }

  // ------------------------------------------------------- slide 4
  function renderMatrix(root) {
    var matrix = model.matrix;
    var states = derivedStates();
    var counts = stateCounts(states);

    var head = "<thead><tr><th>Platform capability</th>" + matrix.oems.map(function (oem) {
      return "<th>" + esc(oem.name) + " " + mark("model") + "</th>";
    }).join("") + "<th>Evidence in repo " + mark("repo") + "</th></tr></thead>";

    var body = "<tbody>" + matrix.rows.map(function (row) {
      var derived = states[row.id];
      return '<tr data-row="' + esc(row.id) + '"><td><div class="cap-name">' + esc(row.name) + "</div>" +
        '<div class="cap-ev">' + row.files.length + " files · " + row.tested + " with tests " + mark("repo") + "</div></td>" +
        matrix.oems.map(function (oem) {
          var cell = row.cells[oem.id];
          if (cell.state === "absent") {
            return '<td><span class="state out-of-scope">not in profile</span>' +
              '<div class="note">' + mark("model") + esc(cell.note) + "</div></td>";
          }
          return '<td><button class="state-btn ' + derived.state + '" data-row="' + esc(row.id) +
            '" data-oem="' + esc(oem.id) + '">' + derived.state + "</button>" +
            '<div class="note">' + mark("run") + esc(derived.why) + "</div></td>";
        }).join("") +
        '<td class="cap-ev">' + esc(row.evidence.join(" · ") || row.files.slice(0, 1).join("")) + "</td></tr>";
    }).join("") + "</tbody>";

    root.innerHTML =
      '<div class="lede"><span><b>' + matrix.rows.length + "</b> " + mark("repo") +
      " platform capabilities × " + matrix.oems.length + " " + mark("model") + " modeled OEM profiles</span>" +
      '<span class="flip">' + counts.verified + " verified · " + counts.observed +
      " observed · " + counts.gap + " gap " + mark("run") + "</span>" +
      "<span>click a badge for the scenario, command, exit code and log lines behind it</span></div>" +
      '<div class="synthetic-banner">' + mark("model") +
      "<b>Modeled OEM variant profiles.</b> BMW, Audi and GM here are modeled from public " +
      "AUTOSAR/UDS conventions — not supplied OEM requirement packages, and no OEM ever executed " +
      "anything shown. The columns only model <i>which capabilities a profile asks for</i>; every " +
      "badge state is derived from this checkout joined with the selected recorded run, and is " +
      "replaceable the moment real packages are provided." +
      "</div>" +
      '<table class="matrix">' + head + body + "</table>" +
      '<div class="state-legend">' + Object.keys(STATE_COPY).map(function (state) {
        return '<span class="legend-item">' + badge(state) + "<em>" + esc(STATE_COPY[state]) + "</em></span>";
      }).join("") + "</div>";

    Array.prototype.forEach.call(root.querySelectorAll(".state-btn"), function (button) {
      button.addEventListener("click", function () {
        var row = matrix.rows.filter(function (item) { return item.id === button.dataset.row; })[0];
        var oem = matrix.oems.filter(function (item) { return item.id === button.dataset.oem; })[0];
        var cell = row.cells[button.dataset.oem];
        openCapabilityDrawer(row, states[row.id], { name: oem.name, note: cell.note });
      });
    });
  }

  // ------------------------------------------------------- slide 5
  function renderRun(root) {
    if (!run) {
      root.innerHTML = '<div class="panel"><h2>No recorded run</h2><p>Run <code>python3 demo/runner/run_scenarios.py</code>' +
        " to record one into <code>demo/data/runs/</code>.</p></div>";
      return;
    }
    var host = run.host || {};
    var commit = run.commit || {};
    var environment = run.environment || {};

    root.innerHTML =
      '<div class="lede"><span><b>' + run.summary.total + "</b> " + mark("run") +
      " scenarios executed against this checkout in " + ms(run.durationMs) + "</span>" +
      '<span class="flip">' + run.summary.passed + " passed · " + run.summary.failed +
      " failed · " + run.summary.skipped + " skipped</span>" +
      "<span>every badge on the other screens is derived from this artifact</span></div>" +
      '<div class="run-facts">' +
        stat(esc(run.startedAt) + mark("run"), "run started (UTC) · " + esc(run.runId)) +
        stat(esc(host.hostname || "?") + mark("run"), esc((host.os || "") + " · " + (host.cpus || "?") + " CPUs")) +
        stat(esc(commit.shortSha || "?") + (commit.dirty ? " +local" : "") + mark("run"),
          "commit under test · branch " + esc(commit.branch || "?")) +
        stat(esc(environment.startMode || "?") + mark("run"),
          environment.degraded ? "degraded: platform secrets unavailable" : "simulator start mode") +
      "</div>" +
      '<div class="scenario-list">' + run.scenarios.map(function (scenario) {
        var state = scenario.status === "pass" ? "verified" : scenario.status === "fail" ? "gap" : "observed";
        return '<button class="scenario-row" data-id="' + esc(scenario.id) + '">' +
          "<div>" + badge(state, scenario.status) + "</div>" +
          '<div><div class="cap-name">' + esc(scenario.name) + "</div>" +
          '<div class="cap-ev">' + esc(scenario.command) + "</div></div>" +
          '<div class="cap-ev">exit ' + (scenario.exitCode == null ? "—" : scenario.exitCode) +
          " · " + ms(scenario.durationMs) + " · " +
          (scenario.assertions || []).filter(function (a) { return a.ok; }).length + "/" +
          (scenario.assertions || []).length + " assertions</div></button>";
      }).join("") + "</div>" +
      '<div class="caption">' + esc(environment.startModeReason || "") + "</div>";

    Array.prototype.forEach.call(root.querySelectorAll(".scenario-row"), function (button) {
      button.addEventListener("click", function () {
        var scenario = run.scenarios.filter(function (item) { return item.id === button.dataset.id; })[0];
        openDrawer("Scenario evidence · " + scenario.status, scenario.name,
          "run " + run.runId + " · " + (scenario.capabilities || []).join(", "),
          '<p class="drawer-lede">' + esc(scenario.description) + "</p>" +
          section("Execution evidence " + mark("run"), scenarioEvidenceHtml(scenario)) +
          (scenario.stdoutTail ? section("Captured output " + mark("run"), pre(scenario.stdoutTail)) : ""));
      });
    });
  }

  // ------------------------------------------------------- excerpt lookup
  var excerptIndex = null;
  function buildExcerptIndex() {
    excerptIndex = {};
    model.manifests.forEach(function (manifest) {
      manifest.elements.forEach(function (element) {
        excerptIndex[element.file + ":" + element.line] = element.excerpt;
      });
    });
    model.traceability.forEach(function (chain) {
      Object.keys(chain.stages).forEach(function (stage) {
        chain.stages[stage].forEach(function (node) {
          if (node.file && node.excerpt) {
            excerptIndex[node.file + ":" + node.line] = node.excerpt;
            excerptIndex[node.file] = excerptIndex[node.file] || node.excerpt;
          }
        });
      });
    });
  }

  function excerptFor(file, line) {
    if (!excerptIndex) { buildExcerptIndex(); }
    return excerptIndex[file + ":" + line] || excerptIndex[file] ||
      "Referenced artifact: " + file + " (line " + line + ").";
  }

  function slideFromHash() {
    var index = parseInt(String(location.hash).replace(/[^0-9]/g, ""), 10) - 1;
    return index >= 0 && index < SLIDES.length ? index : 0;
  }

  // ------------------------------------------------------- boot
  function loadJson(path) {
    return fetch(path).then(function (response) {
      if (!response.ok) { throw new Error(path + ": " + response.status); }
      return response.json();
    });
  }

  function loadRun(runId) {
    var entry = runIndex.filter(function (item) { return item.runId === runId; })[0] || runIndex[0];
    if (!entry) { return Promise.resolve(); }
    return loadJson("data/runs/" + entry.file).then(function (data) {
      run = data;
      renderRunBar();
    });
  }

  loadJson("data/model.json")
    .then(function (data) {
      model = data;
      renderLegend();
      return loadJson("data/runs/index.json")
        .then(function (index) {
          runIndex = index.runs || [];
          return runIndex.length ? loadRun(runIndex[0].runId) : null;
        })
        .catch(function (error) { console.warn("no recorded runs:", error); });
    })
    .then(function () {
      renderRunBar();
      go(slideFromHash());
      window.addEventListener("hashchange", function () {
        var index = slideFromHash();
        if (index !== current) { go(index); }
      });
    })
    .catch(function (error) {
      el.body.innerHTML = '<div class="panel"><h2>Data not found</h2><p>Run <code>python3 demo/generate_data.py</code> ' +
        "and <code>python3 demo/runner/run_scenarios.py</code>, then serve this directory with " +
        "<code>python3 -m http.server</code>.</p></div>";
      console.error(error);
    });
})();
