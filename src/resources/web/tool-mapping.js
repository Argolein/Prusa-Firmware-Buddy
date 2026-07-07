"use strict";
// Argo: web tool mapping — the browser counterpart of the printer's on-screen
// "Tools Mapping" preview. When a print is uploaded-and-started from OrcaSlicer
// (or any PrusaLink client) and the file needs tool mapping (MMU / INDX / multi-
// tool), the firmware now HOLDS in the print-preview "tools_mapping" phase instead
// of auto-starting (see wui_start_print → PreviewSkipIfAble::preview). This module
// polls GET /api/v1/mapping; while that phase is active it shows an overlay letting
// you assign each G-code filament to a printer tool (with type + color for
// context), optionally chain tools via spool join, then PUT the mapping and POST
// /api/v1/mapping/confirm to start — the exact same flow the LCD drives, over the
// web. It is a separate module layered on the untouched upstream bundle, like
// link-controls.js / mesh-view.js.
(function () {
  var ACCENT = "#fa6831";

  var STYLE = [
    ".tm-overlay{position:fixed;inset:0;z-index:90;background:rgba(0,0,0,.62);display:none;align-items:center;justify-content:center;font-family:Helvetica,Arial,sans-serif}",
    ".tm-overlay.open{display:flex}",
    ".tm-modal{width:min(560px,94vw);max-height:90vh;overflow:auto;background:#161616;border:1px solid #3a3a3a;border-radius:8px;box-shadow:0 12px 40px rgba(0,0,0,.7)}",
    ".tm-head{padding:16px 18px 10px;font-size:1.05rem;color:#efefef;font-weight:700}",
    ".tm-sub{padding:0 18px 12px;font-size:.8rem;color:#909090;border-bottom:1px solid #2a2a2a}",
    ".tm-body{padding:8px 18px}",
    ".tm-row{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid #232323}",
    ".tm-gc{display:flex;align-items:center;gap:8px;flex:1 1 42%;min-width:0}",
    ".tm-idx{font-size:.66rem;color:#707070;min-width:12px;text-align:right}",
    ".tm-sw{width:14px;height:14px;border-radius:50%;border:1px solid rgba(255,255,255,.3);flex:none;box-sizing:border-box}",
    ".tm-sw.none{background:transparent;border-style:dashed}",
    ".tm-type{color:#efefef;font-size:.92rem;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}",
    ".tm-arrow{color:#707070;flex:none}",
    ".tm-sel{flex:1 1 42%;min-width:0;font:inherit;font-size:.88rem;color:#efefef;background:#0e0e0e;border:1px solid #3a3a3a;border-radius:4px;padding:7px 8px}",
    ".tm-sel:focus{outline:none;border-color:" + ACCENT + "}",
    ".tm-warn{flex:none;color:#e0a83a;font-size:.72rem;cursor:default}",
    ".tm-warn.hidden{display:none}",
    ".tm-sec{margin-top:12px}",
    ".tm-sec-h{width:100%;text-align:left;background:transparent;border:0;color:#909090;font:inherit;font-size:.74rem;text-transform:uppercase;letter-spacing:.04em;padding:8px 0;cursor:pointer}",
    ".tm-sec-h:hover{color:#efefef}",
    ".tm-sec-body{display:none;padding-bottom:6px}",
    ".tm-sec-body.open{display:block}",
    ".tm-srow{display:flex;align-items:center;gap:10px;padding:6px 0}",
    ".tm-slabel{flex:1 1 42%;color:#cfcfcf;font-size:.86rem}",
    ".tm-foot{display:flex;justify-content:flex-end;gap:10px;padding:14px 18px 16px;border-top:1px solid #2a2a2a;position:sticky;bottom:0;background:#161616}",
    ".tm-btn{font:inherit;font-size:.9rem;border-radius:5px;padding:9px 18px;cursor:pointer;border:1px solid #3a3a3a;background:#232323;color:#efefef}",
    ".tm-btn:hover{background:#2c2c2c}",
    ".tm-btn.primary{background:" + ACCENT + ";border-color:" + ACCENT + ";color:#0a0a0a;font-weight:700}",
    ".tm-btn.primary:hover{background:#ff7a45}",
    ".tm-btn[disabled]{opacity:.5;cursor:default}",
    ".tm-toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(8px);background:#161616;border:1px solid #3a3a3a;border-left:3px solid " + ACCENT + ";color:#efefef;font-family:Helvetica,Arial,sans-serif;font-size:.85rem;padding:10px 16px;border-radius:5px;box-shadow:0 8px 28px rgba(0,0,0,.6);opacity:0;pointer-events:none;transition:opacity .18s,transform .18s;z-index:120}",
    ".tm-toast.show{opacity:1;transform:translateX(-50%) translateY(0)}",
  ].join("");

  var overlay = null, bodyEl = null, printBtn = null, backBtn = null, toastEl = null;
  var shown = false;
  var tools = []; // enabled printer tools available as mapping targets
  var busyAction = false; // guards confirm/cancel while a request is in flight

  // ---------- backend (mirror link-controls.js: hard-timeout every request so a
  // stalled socket can't starve Prusa Connect's shared lwIP TCP-PCB pool) ----------
  var REQ_TIMEOUT_MS = 6000;
  function timedFetch(url, opts) {
    opts = opts || {};
    opts.credentials = "same-origin";
    if (typeof AbortController === "function") {
      var ctrl = new AbortController();
      opts.signal = ctrl.signal;
      var timer = setTimeout(function () { ctrl.abort(); }, REQ_TIMEOUT_MS);
      var clear = function () { clearTimeout(timer); };
      return fetch(url, opts).then(
        function (r) { clear(); return r; },
        function (e) { clear(); throw e; }
      );
    }
    return fetch(url, opts);
  }

  function toast(msg) {
    if (!toastEl) { toastEl = document.createElement("div"); toastEl.className = "tm-toast"; document.body.appendChild(toastEl); }
    toastEl.textContent = msg;
    toastEl.classList.add("show");
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { toastEl.classList.remove("show"); }, 2600);
  }

  function validColor(c) { return c && /^#[0-9a-f]{6}$/i.test(c) ? c : null; }

  // ---------- poll ----------
  var pollBusy = false;
  function poll() {
    if (pollBusy) { return; }
    pollBusy = true;
    timedFetch("/api/v1/mapping", { headers: { Accept: "application/json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) {
        pollBusy = false;
        if (j && j.active) {
          // Only (re)render when opening. While the overlay is already open we
          // leave the user's in-progress selections alone (don't clobber edits);
          // we just keep the poll alive so we can auto-close if the print gets
          // started or aborted elsewhere (e.g. from the printer's touchscreen).
          if (!shown) { show(j); }
        } else if (shown && !busyAction) {
          hide();
        }
      })
      .catch(function () { pollBusy = false; });
  }

  // ---------- overlay ----------
  function build() {
    overlay = document.createElement("div");
    overlay.className = "tm-overlay";
    overlay.innerHTML =
      '<div class="tm-modal" role="dialog" aria-modal="true" aria-label="Assign filaments to tools">' +
      '<div class="tm-head">Assign filaments to tools</div>' +
      '<div class="tm-sub">Map each G-code filament to a printer tool, then print.</div>' +
      '<div class="tm-body"></div>' +
      '<div class="tm-foot">' +
      '<button class="tm-btn" type="button" data-act="back">Back</button>' +
      '<button class="tm-btn primary" type="button" data-act="print">Print</button>' +
      '</div></div>';
    document.body.appendChild(overlay);
    bodyEl = overlay.querySelector(".tm-body");
    backBtn = overlay.querySelector('[data-act="back"]');
    printBtn = overlay.querySelector('[data-act="print"]');
    backBtn.addEventListener("click", cancel);
    printBtn.addEventListener("click", collectAndPrint);
    // Backdrop click / Esc do NOT confirm — they cancel, to avoid an accidental
    // print. The user must pick Back or Print explicitly for a destructive action,
    // so a stray click just leaves the print waiting (closes the overlay only).
    overlay.addEventListener("click", function (e) { if (e.target === overlay) { hide(); } });
    document.addEventListener("keydown", function (e) { if (e.key === "Escape" && shown) { hide(); } });
  }

  function makeSwatch(rgb) {
    var sw = document.createElement("span");
    var hex = validColor(rgb);
    sw.className = "tm-sw" + (hex ? "" : " none");
    if (hex) { sw.style.background = hex; }
    return sw;
  }

  // Option label like "Tool 2 — PETG".
  function toolLabel(t) {
    return "Tool " + t.index + (t.type ? " — " + t.type : "");
  }

  function toolByIndex(idx) {
    for (var i = 0; i < tools.length; i++) { if (tools[i].index === idx) { return tools[i]; } }
    return null;
  }

  function render(data) {
    tools = (data.tools || []).filter(function (t) { return t.enabled; });
    bodyEl.textContent = "";

    var filaments = data.gcode_filaments || [];
    filaments.forEach(function (f) {
      var row = document.createElement("div");
      row.className = "tm-row";

      var gc = document.createElement("div");
      gc.className = "tm-gc";
      var idx = document.createElement("span");
      idx.className = "tm-idx";
      idx.textContent = String(f.index);
      gc.appendChild(idx);
      gc.appendChild(makeSwatch(f.color_rgb));
      var type = document.createElement("span");
      type.className = "tm-type";
      type.textContent = f.type || "—"; // textContent: never trust network strings as HTML
      gc.appendChild(type);
      row.appendChild(gc);

      var arrow = document.createElement("span");
      arrow.className = "tm-arrow";
      arrow.textContent = "→";
      row.appendChild(arrow);

      var sel = document.createElement("select");
      sel.className = "tm-sel";
      sel.setAttribute("data-gc", String(f.index));
      tools.forEach(function (t) {
        var opt = document.createElement("option");
        opt.value = String(t.index);
        opt.textContent = toolLabel(t);
        sel.appendChild(opt);
      });
      // Preselect the current mapping if any, else the tool sharing this index.
      if (typeof f.tool === "number" && toolByIndex(f.tool)) {
        sel.value = String(f.tool);
      } else if (toolByIndex(f.index)) {
        sel.value = String(f.index);
      }
      row.appendChild(sel);

      var warn = document.createElement("span");
      warn.className = "tm-warn hidden";
      warn.textContent = "⚠";
      row.appendChild(warn);

      var updateWarn = function () {
        var t = toolByIndex(parseInt(sel.value, 10));
        var mismatch = f.type && t && t.type && f.type !== t.type;
        warn.classList.toggle("hidden", !mismatch);
        if (mismatch) { warn.title = "G-code wants " + f.type + " but this tool has " + t.type; }
      };
      sel.addEventListener("change", updateWarn);
      updateWarn();

      bodyEl.appendChild(row);
    });

    renderSpoolJoin(data);
  }

  // Optional spool join: chain a tool so that when it runs out, another continues.
  // Kept as a secondary, collapsed section so the core mapping stays front-and-centre.
  function renderSpoolJoin(data) {
    if (tools.length < 2) { return; }
    var sec = document.createElement("div");
    sec.className = "tm-sec";
    var head = document.createElement("button");
    head.type = "button";
    head.className = "tm-sec-h";
    head.textContent = "▸ Spool join (optional)";
    var secBody = document.createElement("div");
    secBody.className = "tm-sec-body";
    head.addEventListener("click", function () {
      var open = secBody.classList.toggle("open");
      head.textContent = (open ? "▾" : "▸") + " Spool join (optional)";
    });
    sec.appendChild(head);

    var joinByFrom = {};
    (data.spool_join || []).forEach(function (j) { joinByFrom[j.from] = j.to; });

    tools.forEach(function (t) {
      var srow = document.createElement("div");
      srow.className = "tm-srow";
      var label = document.createElement("span");
      label.className = "tm-slabel";
      label.textContent = "When Tool " + t.index + " runs out";
      srow.appendChild(label);

      var arrow = document.createElement("span");
      arrow.className = "tm-arrow";
      arrow.textContent = "→";
      srow.appendChild(arrow);

      var sel = document.createElement("select");
      sel.className = "tm-sel tm-spool-sel";
      sel.setAttribute("data-tool", String(t.index));
      var none = document.createElement("option");
      none.value = "0";
      none.textContent = "— stop —";
      sel.appendChild(none);
      tools.forEach(function (o) {
        if (o.index === t.index) { return; }
        var opt = document.createElement("option");
        opt.value = String(o.index);
        opt.textContent = "continue on Tool " + o.index;
        sel.appendChild(opt);
      });
      if (joinByFrom[t.index]) { sel.value = String(joinByFrom[t.index]); }
      srow.appendChild(sel);
      secBody.appendChild(srow);
    });

    sec.appendChild(secBody);
    bodyEl.appendChild(sec);
  }

  function show(data) {
    render(data);
    overlay.classList.add("open");
    shown = true;
  }

  function hide() {
    overlay.classList.remove("open");
    shown = false;
  }

  // ---------- actions ----------
  function collectAndPrint() {
    var sels = bodyEl.querySelectorAll(".tm-sel:not(.tm-spool-sel)");
    var chosen = [];
    var maxG = 0;
    var seen = {};
    for (var i = 0; i < sels.length; i++) {
      var gc = parseInt(sels[i].getAttribute("data-gc"), 10);
      var tool = parseInt(sels[i].value, 10);
      if (!tool) { continue; }
      // The tool mapper is a bijection: two filaments can't share one tool (the
      // second would un-map the first and fatal-error at print). Catch it here
      // with a clear message instead of surfacing the backend's 400.
      if (seen[tool]) { toast("Each filament needs a different tool (use spool join to combine)"); return; }
      seen[tool] = true;
      chosen.push({ gc: gc, tool: tool });
      if (gc > maxG) { maxG = gc; }
    }
    if (!chosen.length) { toast("Assign at least one filament"); return; }

    var mapping = [];
    for (var g = 0; g < maxG; g++) { mapping.push(0); }
    chosen.forEach(function (c) { mapping[c.gc - 1] = c.tool; });

    var spoolSels = bodyEl.querySelectorAll(".tm-spool-sel");
    var joins = [];
    var maxT = 0;
    for (var s = 0; s < spoolSels.length; s++) {
      var from = parseInt(spoolSels[s].getAttribute("data-tool"), 10);
      var to = parseInt(spoolSels[s].value, 10);
      if (from > maxT) { maxT = from; }
      if (to) { joins.push({ from: from, to: to }); }
    }
    var spool = [];
    if (joins.length) {
      for (var k = 0; k < maxT; k++) { spool.push(0); }
      joins.forEach(function (j) { spool[j.from - 1] = j.to; });
    }

    var payload = { mapping: mapping };
    if (spool.length) { payload.spool_join = spool; }

    busyAction = true;
    printBtn.disabled = true;
    backBtn.disabled = true;
    timedFetch("/api/v1/mapping", {
      method: "PUT",
      headers: { "Content-Type": "application/json", Accept: "application/json" },
      body: JSON.stringify(payload),
    })
      .then(function (r) {
        if (r.status === 401 || r.status === 403) { throw new Error("Sign in to PrusaLink first"); }
        if (!r.ok) {
          throw new Error(r.status === 409 ? "The printer isn’t waiting for mapping" : "Check the mapping — each filament needs a distinct tool");
        }
        return timedFetch("/api/v1/mapping/confirm", { method: "POST", headers: { Accept: "application/json" } });
      })
      .then(function (r) {
        if (r.status === 401 || r.status === 403) { throw new Error("Sign in to PrusaLink first"); }
        if (!r.ok && r.status !== 204) { throw new Error("Couldn’t start the print"); }
        toast("Printing…");
        finishAction();
        hide();
      })
      .catch(function (e) {
        finishAction();
        toast(e && e.message ? e.message : "Couldn’t reach the printer");
      });
  }

  function cancel() {
    busyAction = true;
    printBtn.disabled = true;
    backBtn.disabled = true;
    timedFetch("/api/v1/mapping/cancel", { method: "POST", headers: { Accept: "application/json" } })
      .then(function (r) {
        finishAction();
        if (r.status === 401 || r.status === 403) { toast("Sign in to PrusaLink first"); return; }
        hide();
      })
      .catch(function () { finishAction(); hide(); });
  }

  function finishAction() {
    busyAction = false;
    if (printBtn) { printBtn.disabled = false; }
    if (backBtn) { backBtn.disabled = false; }
  }

  // ---------- init ----------
  // Visibility-gated poller (same rationale as link-controls.js: keep local
  // polling gentle so the shared network stack stays free for Prusa Connect).
  var pollTimer = null;
  function startPolling() {
    if (pollTimer === null) { pollTimer = setInterval(function () { if (document.visibilityState === "visible") { poll(); } }, 4000); }
  }
  function stopPolling() { if (pollTimer !== null) { clearInterval(pollTimer); pollTimer = null; } }

  function init() {
    if (window.__tmInit) { return; }
    window.__tmInit = true;
    var style = document.createElement("style");
    style.id = "tm-style";
    style.textContent = STYLE;
    document.head.appendChild(style);

    build();

    document.addEventListener("visibilitychange", function () {
      if (document.visibilityState === "visible") { poll(); startPolling(); } else { stopPolling(); }
    });
    // A #print deep-link (the URL configured as OrcaSlicer's Device-UI) just nudges
    // an immediate poll; the overlay still only opens when the printer is actually
    // holding at the mapping phase.
    window.addEventListener("hashchange", poll);

    poll();
    startPolling();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
