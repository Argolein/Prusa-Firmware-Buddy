"use strict";
// Argo: interactive PrusaLink dashboard controls layered on top of the prebuilt,
// minified upstream bundle (which is left untouched — this is a separate module,
// like mesh-view.js). It makes the static "Nozzle Temperature" / "Heatbed"
// telemetry rows clickable (a floating preset + numeric dropdown that sets the
// target via POST /api/v1/printer/{nozzle,bed}/<°C>), and drives the "Chamber
// Light" on/off switch (POST /api/v1/printer/chamber-light/<0|1>). All state is
// read back from GET /api/v1/status. The telemetry sidebar is static HTML that
// lives outside the SPA's #root, so these hooks are never clobbered by re-renders.
(function () {
  // Preheat presets mirror the firmware table (src/common/filament_presets.cpp),
  // which is also what Prusa Connect shows. "Cooldown" sets the target to 0.
  var PRESETS = {
    nozzle: [
      { n: "PLA", t: 215 }, { n: "PETG", t: 230 }, { n: "ASA", t: 260 },
      { n: "PC", t: 275 }, { n: "PVB", t: 215 }, { n: "ABS", t: 255 },
      { n: "HIPS", t: 220 }, { n: "PP", t: 240 }, { n: "FLEX", t: 240 },
      { n: "PA", t: 285 }, { n: "Cooldown", t: 0 },
    ],
    bed: [
      { n: "PLA", t: 60 }, { n: "PETG", t: 85 }, { n: "ASA", t: 100 },
      { n: "PC", t: 100 }, { n: "PVB", t: 75 }, { n: "ABS", t: 100 },
      { n: "HIPS", t: 100 }, { n: "PP", t: 100 }, { n: "FLEX", t: 50 },
      { n: "PA", t: 100 }, { n: "Cooldown", t: 0 },
    ],
  };
  // Hard interface limits (UX only; firmware clamps server-side too).
  var MAXT = { nozzle: 295, bed: 115 };
  var TITLE = { nozzle: "Nozzle target", bed: "Heatbed target" };
  var LABEL = { nozzle: "Nozzle", bed: "Heatbed" };

  var STYLE = [
    ".lc-clickable{cursor:pointer;border-radius:4px;transition:background .12s}",
    ".lc-clickable:hover{background:rgba(255,255,255,.05)}",
    ".lc-clickable:focus-visible{outline:2px solid #fa6831;outline-offset:2px}",
    ".lc-panel{position:fixed;z-index:60;min-width:210px;background:#161616;border:1px solid #3a3a3a;border-radius:6px;box-shadow:0 8px 28px rgba(0,0,0,.6);padding:8px;font-family:Helvetica,Arial,sans-serif;display:none}",
    ".lc-panel.open{display:block}",
    ".lc-title{font-size:.72rem;color:#707070;text-transform:uppercase;letter-spacing:.04em;padding:4px 8px 8px}",
    ".lc-presets{display:flex;flex-direction:column;max-height:248px;overflow:auto}",
    ".lc-preset{display:flex;justify-content:space-between;gap:18px;align-items:center;font:inherit;font-size:.86rem;color:#efefef;background:transparent;border:0;border-radius:4px;padding:7px 8px;cursor:pointer;text-align:left}",
    ".lc-preset:hover,.lc-preset:focus-visible{background:rgba(250,104,49,.14);outline:none}",
    ".lc-preset .lc-pt{color:#fa6831;font-variant-numeric:tabular-nums}",
    ".lc-preset.lc-cool .lc-pt{color:#6fb0d8}",
    ".lc-row{display:flex;gap:6px;align-items:center;border-top:1px solid #313131;margin-top:6px;padding:8px 4px 2px}",
    ".lc-input{flex:1 1 auto;min-width:0;font:inherit;font-size:.9rem;color:#efefef;background:#0e0e0e;border:1px solid #3a3a3a;border-radius:4px;padding:7px 8px}",
    ".lc-input:focus{outline:none;border-color:#fa6831}",
    ".lc-unit{color:#707070;font-size:.8rem}",
    ".lc-set{font:inherit;font-size:.82rem;font-weight:700;color:#0a0a0a;background:#fa6831;border:0;border-radius:4px;padding:7px 12px;cursor:pointer}",
    ".lc-set:hover{background:#ff7a45}",
    ".lc-hint{font-size:.7rem;color:#707070;padding:6px 8px 2px}",
    ".lc-switch{display:inline-flex;align-items:center;box-sizing:content-box;width:46px;height:24px;background:#3a3a3a;border-radius:999px;cursor:pointer;padding:0;transition:background .15s;vertical-align:middle}",
    ".lc-switch .lc-knob{width:20px;height:20px;margin:2px;border-radius:50%;background:#efefef;transition:transform .15s}",
    ".lc-switch.on{background:#fa6831}",
    ".lc-switch.on .lc-knob{transform:translateX(22px)}",
    ".lc-switch:focus-visible{outline:2px solid #fa6831;outline-offset:2px}",
    ".lc-toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(8px);background:#161616;border:1px solid #3a3a3a;border-left:3px solid #fa6831;color:#efefef;font-family:Helvetica,Arial,sans-serif;font-size:.85rem;padding:10px 16px;border-radius:5px;box-shadow:0 8px 28px rgba(0,0,0,.6);opacity:0;pointer-events:none;transition:opacity .18s,transform .18s;z-index:80}",
    ".lc-toast.show{opacity:1;transform:translateX(-50%) translateY(0)}",
    ".lc-fil-wrap{display:flex;flex-wrap:wrap;gap:6px 12px;align-items:center}",
    ".lc-fil-chip{display:inline-flex;align-items:center;gap:5px;font-size:.92rem;line-height:1.2}",
    ".lc-fil-tno{font-size:.62rem;color:#707070;min-width:8px;text-align:right}",
    ".lc-fil-sw{width:11px;height:11px;border-radius:50%;border:1px solid rgba(255,255,255,.3);flex:none}",
    ".lc-fil-name{color:#efefef}",
    ".lc-fil-chip.empty .lc-fil-name{color:#707070}",
    ".lc-fil-chip.empty .lc-fil-sw{background:transparent;border-style:dashed}",
    ".lc-fil-sw{box-sizing:border-box}",
    ".lc-fil-swbtn{cursor:pointer;padding:0}",
    ".lc-fil-swbtn:hover{box-shadow:0 0 0 2px rgba(250,104,49,.45)}",
    ".lc-colors-panel{min-width:0}",
    ".lc-colors{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;padding:4px}",
    ".lc-color-sw{width:26px;height:26px;border-radius:50%;border:2px solid #3a3a3a;cursor:pointer;padding:0;box-sizing:border-box}",
    ".lc-color-sw:hover{border-color:#efefef}",
    ".lc-color-sw.sel{border-color:#fa6831;box-shadow:0 0 0 2px rgba(250,104,49,.35)}",
    ".lc-color-none{background:transparent;border-style:dashed;position:relative}",
    ".lc-color-none::after{content:'';position:absolute;left:3px;right:3px;top:10px;height:2px;background:#c0504a;transform:rotate(-45deg)}",
  ].join("");

  var panel, titleEl, presetsEl, inputEl, setBtn, hintEl, toastEl;
  var curHeater = null, openAnchor = null;
  var lastTargets = { nozzle: 0, bed: 0 };
  var lightSwitch = null, switchOn = false, lightPending = false;
  var palette = [], colorPanel = null, colorAnchor = null;

  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }

  function toast(msg) {
    if (!toastEl) { toastEl = document.createElement("div"); toastEl.className = "lc-toast"; document.body.appendChild(toastEl); }
    toastEl.textContent = msg;
    toastEl.classList.add("show");
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { toastEl.classList.remove("show"); }, 2200);
  }

  // ---------- backend ----------
  function postControl(path) {
    return fetch("/api/v1/printer/" + path, { method: "POST", credentials: "same-origin", headers: { Accept: "application/json" } })
      .then(function (r) {
        if (r.status === 401 || r.status === 403) { toast("Sign in to PrusaLink first"); return false; }
        if (!r.ok) { throw new Error("HTTP " + r.status); }
        return true;
      });
  }

  function poll() {
    fetch("/api/v1/status", { credentials: "same-origin", headers: { Accept: "application/json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) {
        var p = j && j.printer;
        if (!p) { return; }
        if (typeof p.target_nozzle === "number") { lastTargets.nozzle = Math.round(p.target_nozzle); }
        if (typeof p.target_bed === "number") { lastTargets.bed = Math.round(p.target_bed); }
        if ("chamber_light" in p) {
          showLightRow(true);
          if (!lightPending) { setSwitch(!!p.chamber_light); }
        } else {
          showLightRow(false);
        }
      })
      .catch(function () {});
  }

  // ---------- temperature dropdown ----------
  function buildPanel() {
    panel = document.createElement("div");
    panel.className = "lc-panel";
    panel.innerHTML =
      '<div class="lc-title"></div>' +
      '<div class="lc-presets"></div>' +
      '<div class="lc-row"><input class="lc-input" type="text" inputmode="numeric" autocomplete="off" maxlength="3" aria-label="Custom temperature">' +
      '<span class="lc-unit">&deg;C</span><button class="lc-set" type="button">Set</button></div>' +
      '<div class="lc-hint"></div>';
    document.body.appendChild(panel);
    titleEl = panel.querySelector(".lc-title");
    presetsEl = panel.querySelector(".lc-presets");
    inputEl = panel.querySelector(".lc-input");
    setBtn = panel.querySelector(".lc-set");
    hintEl = panel.querySelector(".lc-hint");

    inputEl.addEventListener("input", function () {
      var v = inputEl.value.replace(/[^0-9]/g, "");
      if (v !== "" && curHeater && parseInt(v, 10) > MAXT[curHeater]) { v = String(MAXT[curHeater]); }
      inputEl.value = v;
    });
    inputEl.addEventListener("keydown", function (e) {
      if (e.key === "Enter") { e.preventDefault(); applyInput(); } else if (e.key === "Escape") { closePanel(); }
    });
    setBtn.addEventListener("click", applyInput);
    panel.addEventListener("click", function (e) { e.stopPropagation(); });
  }

  function populate(heater) {
    var list = PRESETS[heater], frag = document.createDocumentFragment();
    list.forEach(function (p) {
      var cool = p.t === 0;
      var b = document.createElement("button");
      b.type = "button";
      b.className = "lc-preset" + (cool ? " lc-cool" : "");
      b.innerHTML = '<span>' + p.n + '</span><span class="lc-pt">' + (cool ? "0 &deg;C" : p.t + " &deg;C") + "</span>";
      b.addEventListener("click", function () { applyTemp(p.t); });
      frag.appendChild(b);
    });
    presetsEl.innerHTML = "";
    presetsEl.appendChild(frag);
  }

  function applyTemp(v) {
    v = clamp(v | 0, 0, MAXT[curHeater]);
    var heater = curHeater;
    postControl(heater + "/" + v)
      .then(function (ok) { if (ok) { toast(LABEL[heater] + (v === 0 ? " → cooldown" : " → " + v + " °C")); poll(); } })
      .catch(function () { toast("Couldn’t reach the printer"); });
    closePanel();
  }

  function applyInput() {
    var v = parseInt(inputEl.value, 10);
    applyTemp(isNaN(v) ? 0 : v);
  }

  function openPanel(heater, anchor) {
    curHeater = heater;
    openAnchor = anchor;
    titleEl.textContent = TITLE[heater];
    hintEl.textContent = "Preset or 0–" + MAXT[heater] + " °C";
    populate(heater);
    inputEl.value = String(lastTargets[heater] || 0);
    panel.classList.add("open");
    placeNear(panel, anchor);
    setTimeout(function () { inputEl.focus(); inputEl.select(); }, 0);
  }

  function closePanel() { panel.classList.remove("open"); openAnchor = null; }

  // Position a floating panel just below an anchor, kept within the viewport.
  function placeNear(el, anchor) {
    var r = anchor.getBoundingClientRect();
    var pw = el.offsetWidth, ph = el.offsetHeight;
    var left = r.left, top = r.bottom + 6;
    if (left + pw > window.innerWidth - 8) { left = window.innerWidth - 8 - pw; }
    if (left < 8) { left = 8; }
    if (top + ph > window.innerHeight - 8) { top = Math.max(8, r.top - ph - 6); }
    el.style.left = left + "px";
    el.style.top = top + "px";
  }

  function makeClickable(row, heater) {
    if (!row) { return; }
    row.classList.add("lc-clickable");
    row.setAttribute("role", "button");
    row.setAttribute("tabindex", "0");
    row.setAttribute("aria-label", "Set " + LABEL[heater].toLowerCase() + " temperature");
    row.addEventListener("click", function () { openPanel(heater, row); });
    row.addEventListener("keydown", function (e) {
      if (e.key === "Enter" || e.key === " ") { e.preventDefault(); openPanel(heater, row); }
    });
  }

  // ---------- chamber light switch ----------
  function showLightRow(show) {
    var row = document.getElementById("tel-light");
    if (row) { row.style.display = show ? "" : "none"; }
  }
  function setSwitch(on) {
    switchOn = on;
    if (lightSwitch) {
      lightSwitch.classList.toggle("on", on);
      lightSwitch.setAttribute("aria-checked", on ? "true" : "false");
    }
  }
  function toggleLight() {
    var next = !switchOn;
    lightPending = true;
    setSwitch(next);
    postControl("chamber-light/" + (next ? 1 : 0))
      .then(function (ok) {
        lightPending = false;
        if (!ok) { setSwitch(!next); } else { toast("Chamber light " + (next ? "on" : "off")); }
        poll();
      })
      .catch(function () { lightPending = false; setSwitch(!next); toast("Couldn’t reach the printer"); });
  }

  // ---------- per-tool filament (Filament Color Manager) ----------
  // GET /api/v1/filament returns one entry per physical tool (1 on Core One, up
  // to 8 on INDX), so the row adapts to the tool count for free. Each tool's type
  // is shown with a swatch in its assigned color; empty slots are still listed.
  function renderFilament(tools) {
    var wrap = document.getElementById("lc-filament");
    var row = document.getElementById("tel-filament");
    if (!wrap || !row) { return; }
    if (!tools || !tools.length) { row.style.display = "none"; return; }
    var canPick = palette.length > 0;
    wrap.textContent = "";
    tools.forEach(function (t) {
      var loaded = !!t.type;
      var hex = t.color_rgb && /^#[0-9a-f]{6}$/i.test(t.color_rgb) ? t.color_rgb : null;
      var chip = document.createElement("span");
      chip.className = "lc-fil-chip" + (loaded ? "" : " empty");
      // Always label the tool number (1-based) — shown even on a single-tool printer.
      var tno = document.createElement("span");
      tno.className = "lc-fil-tno";
      tno.textContent = typeof t.tool === "number" ? String(t.tool + 1) : "";
      chip.appendChild(tno);
      // Loaded tools get a clickable swatch (opens the color picker); empty slots
      // stay a non-interactive placeholder.
      var sw;
      if (loaded && canPick) {
        sw = document.createElement("button");
        sw.type = "button";
        sw.className = "lc-fil-sw lc-fil-swbtn";
        sw.style.background = hex || "#888";
        sw.title = t.color ? "Color: " + t.color + " — click to change" : "Set color";
        sw.setAttribute("aria-label", "Set tool " + ((t.tool || 0) + 1) + " color");
        sw.addEventListener("click", function (e) { e.stopPropagation(); openColorPicker(t.tool, t.color, sw); });
      } else {
        sw = document.createElement("span");
        sw.className = "lc-fil-sw";
        if (loaded) { sw.style.background = hex || "#888"; }
      }
      chip.appendChild(sw);
      var name = document.createElement("span");
      name.className = "lc-fil-name";
      name.textContent = loaded ? t.type : "—"; // textContent: never trust network strings as HTML
      chip.appendChild(name);
      wrap.appendChild(chip);
    });
    row.style.display = "";
  }

  function pollFilament() {
    fetch("/api/v1/filament", { credentials: "same-origin", headers: { Accept: "application/json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) {
        if (j && j.tools) {
          if (Array.isArray(j.palette)) { palette = j.palette; }
          renderFilament(j.tools);
        } else {
          var row = document.getElementById("tel-filament");
          if (row) { row.style.display = "none"; }
        }
      })
      .catch(function () {});
  }

  // ---------- tool color picker ----------
  function buildColorPanel() {
    colorPanel = document.createElement("div");
    colorPanel.className = "lc-panel lc-colors-panel";
    colorPanel.innerHTML = '<div class="lc-title"></div><div class="lc-colors"></div>';
    document.body.appendChild(colorPanel);
    colorPanel.addEventListener("click", function (e) { e.stopPropagation(); });
  }

  function closeColors() { colorPanel.classList.remove("open"); colorAnchor = null; }

  function openColorPicker(tool, currentName, anchor) {
    colorAnchor = anchor;
    colorPanel.querySelector(".lc-title").textContent = "Tool " + (tool + 1) + " color";
    var grid = colorPanel.querySelector(".lc-colors");
    grid.textContent = "";
    palette.forEach(function (c) {
      var b = document.createElement("button");
      b.type = "button";
      b.className = "lc-color-sw" + (c.name === currentName ? " sel" : "");
      b.style.background = c.color_rgb && /^#[0-9a-f]{6}$/i.test(c.color_rgb) ? c.color_rgb : "#888";
      b.title = c.name;
      b.addEventListener("click", function () { applyColor(tool, c.name); });
      grid.appendChild(b);
    });
    var none = document.createElement("button");
    none.type = "button";
    none.className = "lc-color-sw lc-color-none" + (currentName ? "" : " sel");
    none.title = "No color";
    none.addEventListener("click", function () { applyColor(tool, null); });
    grid.appendChild(none);
    colorPanel.classList.add("open");
    placeNear(colorPanel, anchor);
  }

  function applyColor(tool, name) {
    fetch("/api/v1/filament/" + tool, {
      method: "PUT",
      credentials: "same-origin",
      headers: { "Content-Type": "application/json", Accept: "application/json" },
      body: JSON.stringify({ color: name }),
    })
      .then(function (r) {
        if (r.status === 401 || r.status === 403) { toast("Sign in to PrusaLink first"); return; }
        if (!r.ok) { throw new Error("HTTP " + r.status); }
        toast("Tool " + (tool + 1) + (name ? " color set" : " color cleared"));
        pollFilament();
      })
      .catch(function () { toast("Couldn’t reach the printer"); });
    closeColors();
  }

  // ---------- init ----------
  function rowFor(where) {
    var s = document.querySelector('[data-where="' + where + '"]');
    return s ? s.closest(".tel-prop") : null;
  }

  function init() {
    if (window.__lcInit) { return; }
    window.__lcInit = true;
    var style = document.createElement("style");
    style.id = "lc-style";
    style.textContent = STYLE;
    document.head.appendChild(style);

    buildPanel();
    buildColorPanel();
    makeClickable(rowFor("telemetry.temperature.nozzle.current"), "nozzle");
    makeClickable(rowFor("telemetry.temperature.bed.current"), "bed");

    lightSwitch = document.getElementById("lc-light-switch");
    if (lightSwitch) {
      lightSwitch.addEventListener("click", toggleLight);
      lightSwitch.addEventListener("keydown", function (e) {
        if (e.key === "Enter" || e.key === " ") { e.preventDefault(); toggleLight(); }
      });
    }

    document.addEventListener("click", function (e) {
      if (panel.classList.contains("open") && !panel.contains(e.target) && !(openAnchor && openAnchor.contains(e.target))) { closePanel(); }
      if (colorPanel.classList.contains("open") && !colorPanel.contains(e.target) && !(colorAnchor && colorAnchor.contains(e.target))) { closeColors(); }
    });
    document.addEventListener("keydown", function (e) { if (e.key === "Escape") { closePanel(); closeColors(); } });
    window.addEventListener("resize", function () { closePanel(); closeColors(); });
    // Close on PAGE scroll only — not when scrolling inside a panel's own list
    // (this capture-phase listener also sees the preset list's scroll events).
    window.addEventListener("scroll", function (e) {
      if (panel.contains(e.target) || colorPanel.contains(e.target)) { return; }
      closePanel();
      closeColors();
    }, true);

    poll();
    setInterval(poll, 3000);
    pollFilament();
    setInterval(pollFilament, 8000);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
