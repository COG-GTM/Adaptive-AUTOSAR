/* V-Cycle Command Center — static demo driven by demo/data/model.json */
(function () {
  "use strict";

  var model = null;
  var current = 0;

  var SLIDES = [
    { title: "Current State & Challenges", nav: "Current State", render: renderCurrentState },
    { title: "Requirements Intake", nav: "Requirements Intake", render: renderIntake },
    { title: "V-Cycle Traceability", nav: "V-Cycle Traceability", render: renderTrace },
    { title: "OEM Variant Matrix", nav: "OEM Variant Matrix", render: renderMatrix }
  ];

  var el = {
    nav: document.getElementById("nav"),
    title: document.getElementById("slide-title"),
    body: document.getElementById("slide-body"),
    foot: document.getElementById("foot-right"),
    drawer: document.getElementById("drawer"),
    drawerKind: document.getElementById("drawer-kind"),
    drawerTitle: document.getElementById("drawer-title"),
    drawerPath: document.getElementById("drawer-path"),
    drawerExcerpt: document.getElementById("drawer-excerpt")
  };

  function esc(text) {
    return String(text == null ? "" : text)
      .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  function pct(value) { return Math.round(value * 100) + "%"; }

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
    buildNav();
    el.title.textContent = SLIDES[current].title;
    el.body.innerHTML = "";
    el.body.style.animation = "none";
    void el.body.offsetWidth;
    el.body.style.animation = "";
    SLIDES[current].render(el.body);
    el.foot.textContent = "0" + (current + 1) + " / 0" + SLIDES.length;
  }

  // --------------------------------------------------------------- drawer
  function openDrawer(kind, title, path, excerpt) {
    el.drawerKind.textContent = kind;
    el.drawerTitle.textContent = title;
    el.drawerPath.textContent = path || "";
    el.drawerExcerpt.textContent = excerpt || "(no excerpt available)";
    el.drawer.hidden = false;
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

  // ------------------------------------------------------- slide 1
  function renderCurrentState(root) {
    var c = model.counts;
    var untestedLoc = model.untested.reduce(function (sum, item) { return sum + item.loc; }, 0);
    var worst = model.modules.filter(function (m) { return m.files >= 4; })
      .sort(function (a, b) { return a.coverage - b.coverage; })[0];

    var left = [
      ["Safety-critical hardware", " + embedded software for OEMs (BMW, Audi, GM)"],
      ["Full V-cycle", ": OEM requirements, architecture, SW requirements, implementation, test, validation"],
      ["Stack", ": Adaptive AUTOSAR (" + c.sourceFiles + " C++ files, " + c.sourceLoc.toLocaleString() +
        " LOC), SOME/IP, DoIP/UDS, POSIX/Linux; manifests in ARXML"],
      ["Configuration", ": " + c.manifests + " ARXML manifests, " + c.manifestElements +
        " deployment elements, " + c.functionGroups + " function group, " + c.checkpoints +
        " supervision checkpoints"],
      ["Global system engineering", " (NA, Europe, Poland, India)"]
    ];

    var right = [
      ["Requirements intake", ": diffing OEM packages and assessing implementation <b>takes 2-3 weeks</b> — " +
        c.changedRequirements + " of " + (c.tracedRequirements + 1) + " requirements moved in the last delivery"],
      ["AUTOSAR/ECU integration", ": ~1 month of team effort per request across " +
        c.serviceInstances + " SOME/IP service instances and " + c.udsServices + " UDS services"],
      ["Traceability gaps", " across the V-cycle: <span class='risk'>" + c.requirementGaps + " of " +
        c.tracedRequirements + "</span> requirement chains break before a unit test or a manifest element"],
      ["OEM variants", " growing a “giant codebase”; <span class='risk'>" + c.untestedFiles +
        " of " + c.sourceFiles + " source files</span> (" + pct(1 - c.coverageRatio) +
        ") have no corresponding unit test — worst module <b>" + worst.module + "</b> at " +
        pct(worst.coverage)],
      ["Fragmented AI tooling", " with no scalable ROI path"]
    ];

    root.innerHTML =
      '<div class="cols">' +
        '<section class="panel">' +
          "<h2>Current State</h2>" + bullets(left) +
          '<div class="caption">Aptiv ADAS environment · parsed from COG-GTM/Adaptive-AUTOSAR</div>' +
        "</section>" +
        '<div class="arrow">→</div>' +
        '<section class="panel accent">' +
          "<h2>Negative Consequences</h2>" + bullets(right) +
          '<div class="caption">Bottlenecks from discovery · every number computed at build time</div>' +
        "</section>" +
      "</div>" +
      '<div class="stat-row">' +
        stat(c.testCases.toLocaleString(), c.testFiles + " gtest files, " + c.testCases + " cases") +
        stat(c.untestedFiles + " / " + c.sourceFiles, "source files with no unit test (" +
          untestedLoc.toLocaleString() + " LOC in the top 40)") +
        stat(c.dids + " DIDs · " + c.dtcs + " DTC", "diagnostic identifiers declared in code and ARXML") +
        stat(c.requirementGaps + " / " + c.tracedRequirements, "requirement chains with a traceability gap") +
      "</div>";
  }

  function bullets(items) {
    return '<ul class="bullets">' + items.map(function (item) {
      return "<li><b>" + esc(item[0]) + "</b>" + item[1] + "</li>";
    }).join("") + "</ul>";
  }

  function stat(value, key) {
    return '<div class="stat"><div class="v">' + esc(value) + '</div><div class="k">' + esc(key) + "</div></div>";
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
      '<div class="lede"><span><b>Manual:</b> 2-3 weeks of package diffing and impact assessment</span>' +
      '<span class="flip">→ This view: seconds</span>' +
      "<span>" + moved + " requirements moved · impact resolved against real ARXML and C++</span></div>" +
      '<div class="diff-head">' +
        '<div class="pkg"><b>' + esc(v1.release) + "</b> · " + esc(v1.issued) + " · " +
          v1.requirements + " requirements</div>" +
        '<div class="pkg"><b>' + esc(v2.release) + "</b> · " + esc(v2.issued) + " · " +
          v2.requirements + " requirements</div>" +
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
        '<div><div class="tag unchanged">unchanged</div>' +
          '<div class="req-id">' + esc(item.id) + "</div></div>" +
        '<div class="req-text">carried over from baseline</div>' +
        '<div><div class="req-title">' + esc(item.title) + asil + "</div></div></div>";
    }
    return '<div class="diff-row ' + item.status + '">' +
      '<div><div class="tag ' + item.status + '">' + item.status + '</div>' +
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
        openDrawer(chip.dataset.kind + " impact", chip.textContent,
          file + ":" + chip.dataset.line, excerptFor(file, Number(chip.dataset.line)));
      });
    });
  }

  // ------------------------------------------------------- slide 3
  var STAGES = [
    ["requirement", "OEM Requirement"],
    ["manifest", "ARXML Manifest Element"],
    ["service", "ara:: Service / Module"],
    ["source", "Source File"],
    ["test", "Unit Test"]
  ];

  function renderTrace(root) {
    var chains = model.traceability;
    var broken = chains.filter(function (chain) { return chain.gaps.length; }).length;

    root.innerHTML =
      '<div class="lede"><span><b>' + chains.length + "</b> requirement chains resolved across five V-cycle stages</span>" +
      '<span class="flip">' + broken + " chains carry a gap badge</span>" +
      "<span>click any node for its real file path and excerpt</span></div>" +
      '<div class="trace-head">' + STAGES.map(function (stage) {
        return '<div class="micro">' + esc(stage[1]) + "</div>";
      }).join("") + "</div>" +
      '<div class="chains">' + chains.map(chainRow).join("") + "</div>";


    Array.prototype.forEach.call(root.querySelectorAll(".node"), function (node) {
      node.addEventListener("click", function () {
        openDrawer(node.dataset.kind, node.dataset.label,
          node.dataset.file ? node.dataset.file + ":" + node.dataset.line : "",
          node.dataset.excerpt);
      });
    });
  }

  function chainRow(chain) {
    return '<div class="chain">' + STAGES.map(function (stage) {
      var nodes = chain.stages[stage[0]] || [];
      if (!nodes.length) {
        var gap = stage[0] === "test" ? "no unit test"
          : stage[0] === "manifest" ? "no manifest element"
          : stage[0] === "source" ? "no implementation owner" : "not linked";
        var badge = (stage[0] === "service")
          ? '<div class="node" data-kind="service" data-label="' + esc(chain.title) +
            '" data-excerpt="Covered by the module that owns the requirement."><div class="l">—</div>' +
            '<div class="s">no dedicated service id</div></div>'
          : '<div class="gap-badge">' + gap + "</div>";
        return '<div class="cell empty">' + badge + "</div>";
      }
      var shown = nodes.slice(0, 2);
      var more = nodes.length - shown.length;
      return '<div class="cell">' + shown.map(function (node) {
        return '<div class="node' + (stage[0] === "requirement" ? " req" : "") +
          '" data-kind="' + esc(stage[1]) + '" data-label="' + esc(node.label) +
          '" data-file="' + esc(node.file || "") + '" data-line="' + (node.line || 1) +
          '" data-excerpt="' + esc(node.excerpt || "") + '">' +
          '<div class="l">' + esc(node.label) + "</div>" +
          '<div class="s">' + esc(node.sub || "") + "</div></div>";
      }).join("") + (more > 0 ? '<div class="more">+' + more + " more</div>" : "") + "</div>";
    }).join("") + "</div>";
  }

  // ------------------------------------------------------- slide 4
  function renderMatrix(root) {
    var matrix = model.matrix;
    var counts = { supported: 0, partial: 0, absent: 0 };
    matrix.rows.forEach(function (row) {
      matrix.oems.forEach(function (oem) { counts[row.cells[oem.id].state] += 1; });
    });

    var head = "<thead><tr><th>Platform capability</th>" + matrix.oems.map(function (oem) {
      return "<th>" + esc(oem.name) + "</th>";
    }).join("") + "<th>Evidence in repo</th></tr></thead>";

    var body = "<tbody>" + matrix.rows.map(function (row) {
      return "<tr><td><div class='cap-name'>" + esc(row.name) + "</div>" +
        "<div class='cap-ev'>" + row.files.length + " files · " + row.tested + " with tests</div></td>" +
        matrix.oems.map(function (oem) {
          var cell = row.cells[oem.id];
          return "<td><span class='state " + cell.state + "'>" + cell.state +
            "</span><div class='note'>" + esc(cell.note) + "</div></td>";
        }).join("") +
        "<td class='cap-ev'>" + esc(row.evidence.join(" · ") || row.files.slice(0, 1).join("")) + "</td></tr>";
    }).join("") + "</tbody>";

    var modules = model.modules.filter(function (module) { return module.files >= 3; })
      .sort(function (a, b) { return a.coverage - b.coverage; });

    root.innerHTML =
      '<div class="lede"><span><b>' + matrix.rows.length + "</b> capabilities × " +
      matrix.oems.length + " OEM variants, derived from function groups, SOME/IP services and UDS handlers</span>" +
      '<span class="flip">' + counts.supported + " supported · " + counts.partial +
      " partial · " + counts.absent + " absent</span></div>" +
      '<table class="matrix">' + head + body + "</table>" +
      '<div class="debt"><h3>Tech debt — unit-test coverage per source directory</h3><div class="bars">' +
      modules.map(function (module) {
        var band = module.coverage < 0.34 ? " low" : module.coverage < 0.67 ? " mid" : "";
        return '<div class="bar-row"><div class="m"><span>' + esc(module.module) + "</span><span>" +
          module.tested + "/" + module.files + "</span></div>" +
          '<div class="bar' + band + '"><i style="width:' + pct(module.coverage) + '"></i></div></div>';
      }).join("") + "</div></div>";
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

  // ------------------------------------------------------- boot
  fetch("data/model.json")
    .then(function (response) { return response.json(); })
    .then(function (data) {
      model = data;
      go(0);
    })
    .catch(function (error) {
      el.body.innerHTML = '<div class="panel"><h2>Data not found</h2><p>Run <code>python3 demo/generate_data.py</code> ' +
        "and serve this directory with <code>python3 -m http.server</code>.</p></div>";
      console.error(error);
    });
})();
