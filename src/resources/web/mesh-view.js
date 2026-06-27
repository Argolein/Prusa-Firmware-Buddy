"use strict";
// Bed Mesh as a first-class PrusaLink route (#mesh), rendered inside the SPA's
// own content area (#root) so it reuses the real header, nav and telemetry
// sidebar. The SPA's router ignores unknown hashes (Zt: `if(!a) return false`),
// so we own #mesh: on hashchange we render our UI into #root and mark the nav
// link active; when the user leaves, the SPA renders its own route into #root.
(function () {
  var $ = function (id) { return document.getElementById(id); };

  // ---------- styles (scoped under #mv; chrome comes from the SPA) ----------
  var STYLE = [
    '#mv{padding-top:8px;font-family:Helvetica,Arial,sans-serif}',
    '#mv .mv-controls{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin:18px 0 24px}',
    '#mv button.mv-btn,#mv .mv-toggle{font:inherit;font-size:.85rem;color:#efefef;background:transparent;border:1px solid #707070;border-radius:4px;padding:8px 14px;cursor:pointer;user-select:none}',
    '#mv button.mv-btn:hover,#mv .mv-toggle:hover{border-color:#efefef}',
    '#mv .mv-toggle.on{background:#fa6831;border-color:#fa6831;color:#0a0a0a;font-weight:700}',
    '#mv button.mv-level{color:#fa6831;border-color:#fa6831}',
    '#mv button.mv-level:hover{background:rgba(250,104,49,.12)}',
    '#mv button.mv-level.arm{background:#fa6831;border-color:#fa6831;color:#0a0a0a;font-weight:700}',
    '#mv button.mv-level:disabled{color:#707070;border-color:#313131;cursor:default}',
    '#mv .mv-sep{width:1px;align-self:stretch;background:#313131;margin:2px}',
    '#mv .mv-cols{display:flex;gap:40px;align-items:flex-start}',
    '#mv .mv-plot{flex:1 1 auto;min-width:0;max-width:620px}',
    '#mv .mv-stats{width:250px;flex:none}',
    '#mv .mv-sec{color:#707070;font-size:1.05rem;padding-bottom:8px;border-bottom:1px solid #313131;margin:0 0 16px}',
    '#mv .mv-stage{display:flex;gap:14px;position:relative}',
    '#mv .mv-canvas{flex:1 1 auto;min-width:0}',
    '#mv #mv-bed{width:100%;height:auto;display:block;background:#161616;cursor:crosshair}',
    '#mv .mv-xaxis{display:flex;justify-content:space-between;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:11px;color:#707070;margin-top:6px}',
    '#mv .mv-bar{display:flex;width:54px;flex:none}',
    '#mv #mv-colorbar{width:16px;display:block}',
    '#mv .mv-bticks{display:flex;flex-direction:column;justify-content:space-between;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:10px;color:#707070;margin-left:6px}',
    '#mv .mv-stat{display:flex;justify-content:space-between;align-items:baseline;padding:9px 0;border-bottom:1px solid #313131}',
    '#mv .mv-stat:last-child{border-bottom:0}',
    '#mv .mv-stat .k{font-size:.8rem;color:#707070}',
    '#mv .mv-stat .v{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:1rem;color:#efefef}',
    '#mv .mv-stat.range .v{color:#fa6831}',
    '#mv .mv-stat .v .u{font-size:.7rem;color:#707070;margin-left:3px}',
    '#mv .mv-legend{margin-top:26px}',
    '#mv .mv-legrow{display:flex;align-items:center;gap:9px;font-size:.8rem;color:#707070;padding:5px 0}',
    '#mv .mv-legrow .m{width:16px;height:16px;flex:none;display:grid;place-items:center}',
    '#mv .mv-ch{width:12px;height:12px;position:relative}',
    '#mv .mv-ch::before,#mv .mv-ch::after{content:"";position:absolute;background:#efefef}',
    '#mv .mv-ch::before{left:5px;top:0;width:1px;height:12px}',
    '#mv .mv-ch::after{top:5px;left:0;height:1px;width:12px}',
    '#mv .mv-interp{width:13px;height:13px;background:linear-gradient(135deg,#1e5f9e,#f2efe6,#c2402a);opacity:.75}',
    '#mv .mv-iso{width:14px;border-top:1px solid #bababa}',
    '#mv .mv-probe{position:absolute;pointer-events:none;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:11px;background:rgba(0,0,0,.9);border:1px solid #707070;border-radius:3px;padding:6px 8px;color:#efefef;white-space:nowrap;opacity:0;transition:opacity .1s;z-index:6;line-height:1.5}',
    '#mv .mv-probe b{color:#fa6831}',
    '#mv .mv-probe .l{color:#707070}',
    '#mv .mv-overlay{position:absolute;inset:0;display:none;flex-direction:column;align-items:center;justify-content:center;gap:10px;background:rgba(10,10,10,.95);text-align:center;padding:30px}',
    '#mv .mv-overlay.show{display:flex}',
    '#mv .mv-overlay .big{font-size:1rem;color:#efefef}',
    '#mv .mv-overlay .small{font-size:.85rem;color:#707070;max-width:340px}',
    '#mv .mv-overlay a{color:#fa6831;text-decoration:none}',
    '@media (max-width:980px){#mv .mv-cols{flex-direction:column}#mv .mv-stats{width:100%}#mv .mv-plot{max-width:none}}',
  ].join("");

  // ---------- markup injected into #root ----------
  var HTML =
    '<div id="title"><p class="txt-lg txt-grey">Bed Mesh</p>' +
    '<p class="title-printer txt-grey">Printer <span class="txt-orange txt-bold" id="mv-name">—</span></p></div>' +
    '<div id="mv">' +
    '<div class="mv-controls">' +
    '<button class="mv-btn" id="mv-refresh">↻ Refresh</button>' +
    '<button class="mv-btn mv-level" id="mv-level">⬡ Run bed leveling</button>' +
    '<span class="mv-sep"></span>' +
    '<span class="mv-toggle on" id="mv-tMode" role="button" tabindex="0" aria-pressed="true">deviation</span>' +
    '<span class="mv-toggle on" id="mv-tContour" role="button" tabindex="0" aria-pressed="true">contours</span>' +
    '<span class="mv-toggle on" id="mv-tProbe" role="button" tabindex="0" aria-pressed="true">probe points</span>' +
    '</div>' +
    '<div class="mv-cols">' +
    '<div class="mv-plot">' +
    '<div class="mv-sec">Bed surface · Y ↑ / X →</div>' +
    '<div class="mv-stage">' +
    '<div class="mv-canvas"><canvas id="mv-bed" width="600" height="600"></canvas>' +
    '<div class="mv-xaxis"><span id="mv-xMin">—</span><span>X (mm)</span><span id="mv-xMax">—</span></div></div>' +
    '<div class="mv-bar"><canvas id="mv-colorbar" width="16" height="300"></canvas>' +
    '<div class="mv-bticks"><span id="mv-barHi">—</span><span id="mv-barMid">0</span><span id="mv-barLo">—</span></div></div>' +
    '<div class="mv-probe" id="mv-probe"></div>' +
    '<div class="mv-overlay" id="mv-overlay"><div class="big" id="mv-ovBig">No mesh on record</div>' +
    '<div class="small" id="mv-ovSmall">Run a bed level (First Layer Calibration / <code>G29</code>), then refresh.</div></div>' +
    '</div></div>' +
    '<div class="mv-stats">' +
    '<div class="mv-sec">Deviation</div>' +
    '<div class="mv-stat range"><span class="k">Range (peak-peak)</span><span class="v" id="mv-sRange">—</span></div>' +
    '<div class="mv-stat"><span class="k">Std deviation</span><span class="v" id="mv-sStd">—</span></div>' +
    '<div class="mv-stat"><span class="k">Highest point</span><span class="v" id="mv-sMax">—</span></div>' +
    '<div class="mv-stat"><span class="k">Lowest point</span><span class="v" id="mv-sMin">—</span></div>' +
    '<div class="mv-stat"><span class="k">Mean height</span><span class="v" id="mv-sMean">—</span></div>' +
    '<div class="mv-legend"><div class="mv-sec">Legend</div>' +
    '<div class="mv-legrow"><span class="m"><span class="mv-ch"></span></span><span>measured probe point</span></div>' +
    '<div class="mv-legrow"><span class="m"><span class="mv-interp"></span></span><span>interpolated surface</span></div>' +
    '<div class="mv-legrow"><span class="m"><span class="mv-iso"></span></span><span>iso-deviation line</span></div>' +
    '</div></div>' +
    '</div></div>';

  // ---------- rendering core (ported, scoped to mv- elements) ----------
  var STOPS = [[0, [30, 95, 158]], [0.25, [111, 176, 216]], [0.5, [232, 233, 224]], [0.75, [220, 130, 52]], [1, [194, 64, 42]]];
  function colorAt(t) {
    t = Math.max(0, Math.min(1, t));
    for (var i = 1; i < STOPS.length; i++) {
      if (t <= STOPS[i][0]) {
        var t0 = STOPS[i - 1][0], c0 = STOPS[i - 1][1], t1 = STOPS[i][0], c1 = STOPS[i][1], f = (t - t0) / (t1 - t0);
        return [Math.round(c0[0] + (c1[0] - c0[0]) * f), Math.round(c0[1] + (c1[1] - c0[1]) * f), Math.round(c0[2] + (c1[2] - c0[2]) * f)];
      }
    }
    return STOPS[STOPS.length - 1][1];
  }

  var M = null, opts = { mode: "deviation", contour: true, probe: true };
  var bed, bctx, cbar, cbctx, probeEl;
  var probing = false, armTimer = null;
  var BUSY = { PRINTING: 1, BUSY: 1, PAUSED: 1, ATTENTION: 1 }, DONE = { IDLE: 1, READY: 1, FINISHED: 1, STOPPED: 1 };

  function isProbed(g, i, j) {
    var b = g.border, s = Math.max(1, g.major_step);
    return i >= b && i < g.x_points - b && (i - b) % s === 0 && j >= b && j < g.y_points - b && (j - b) % s === 0;
  }
  function nearestProbed(g, gx, gy) {
    var b = g.border, s = Math.max(1, g.major_step);
    var lastX = b + Math.max(0, g.x_major - 1) * s, lastY = b + Math.max(0, g.y_major - 1) * s;
    var i = Math.max(b, Math.min(lastX, b + Math.round((gx - b) / s) * s));
    var j = Math.max(b, Math.min(lastY, b + Math.round((gy - b) / s) * s));
    return { i: i, j: j };
  }
  function norm(v) {
    if (opts.mode === "deviation") { var span = M.maxAbsDev || 1e-6; return (v - M.mean) / (2 * span) + 0.5; }
    var r = (M.max - M.min) || 1e-6; return (v - M.min) / r;
  }
  function fmt(v, d) { return (v >= 0 ? "+" : "") + v.toFixed(d === undefined ? 3 : d); }
  function fmtAbs(v) { return v.toFixed(3); }

  function sample(gx, gy) {
    var d = M.data, X = M.x_points, Y = M.y_points;
    gx = Math.max(0, Math.min(X - 1, gx)); gy = Math.max(0, Math.min(Y - 1, gy));
    var x0 = Math.floor(gx), y0 = Math.floor(gy), x1 = Math.min(X - 1, x0 + 1), y1 = Math.min(Y - 1, y0 + 1);
    var fx = gx - x0, fy = gy - y0, a = d[y0][x0], b = d[y0][x1], c = d[y1][x0], e = d[y1][x1];
    if ([a, b, c, e].some(function (z) { return z === null || Number.isNaN(z); })) return NaN;
    return a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + e * fx * fy;
  }

  function render() {
    if (!M) return;
    var X = M.x_points, Y = M.y_points;
    var cssW = bed.clientWidth || 560;
    var aspect = ((X - 1) * M.x_step) / Math.max(1e-6, (Y - 1) * M.y_step) || 1;
    var cssH = Math.round(cssW / aspect);
    var dpr = Math.min(2, window.devicePixelRatio || 1);
    bed.width = Math.round(cssW * dpr); bed.height = Math.round(cssH * dpr);
    bed.style.height = cssH + "px";
    var W = bed.width, H = bed.height;
    bctx.setTransform(1, 0, 0, 1, 0, 0); bctx.clearRect(0, 0, W, H);

    var off = document.createElement("canvas"); off.width = X; off.height = Y;
    var octx = off.getContext("2d"), img = octx.createImageData(X, Y);
    for (var py = 0; py < Y; py++) {
      var gy = Y - 1 - py;
      for (var px = 0; px < X; px++) {
        var v = M.data[gy][px], o = (py * X + px) * 4;
        if (v === null || Number.isNaN(v)) { img.data[o] = 22; img.data[o + 1] = 22; img.data[o + 2] = 22; img.data[o + 3] = 255; }
        else { var c = colorAt(norm(v)); img.data[o] = c[0]; img.data[o + 1] = c[1]; img.data[o + 2] = c[2]; img.data[o + 3] = 255; }
      }
    }
    octx.putImageData(img, 0, 0);
    bctx.imageSmoothingEnabled = true; bctx.imageSmoothingQuality = "high";
    bctx.drawImage(off, 0, 0, X, Y, 0, 0, W, H);
    bctx.strokeStyle = "rgba(112,112,112,.5)"; bctx.lineWidth = dpr;
    bctx.strokeRect(0.5 * dpr, 0.5 * dpr, W - dpr, H - dpr);

    if (opts.contour) drawContours(W, H, dpr);
    if (opts.probe) {
      for (var j = 0; j < Y; j++) for (var i = 0; i < X; i++) {
        if (!isProbed(M, i, j)) continue;
        var pxc = (i / (X - 1)) * W, pyc = (1 - j / (Y - 1)) * H, r = 4 * dpr;
        bctx.strokeStyle = "rgba(10,10,10,.55)"; bctx.lineWidth = 3 * dpr; cross(pxc, pyc, r);
        bctx.strokeStyle = "rgba(239,239,239,.92)"; bctx.lineWidth = dpr; cross(pxc, pyc, r);
      }
    }
  }
  function cross(px, py, r) { bctx.beginPath(); bctx.moveTo(px - r, py); bctx.lineTo(px + r, py); bctx.moveTo(px, py - r); bctx.lineTo(px, py + r); bctx.stroke(); }
  function niceStep(span) { var raw = span / 6, mag = Math.pow(10, Math.floor(Math.log10(raw))), n = raw / mag; return (n >= 5 ? 5 : n >= 2 ? 2 : 1) * mag; }
  function drawContours(W, H, dpr) {
    var X = M.x_points, Y = M.y_points, d = M.data, mean = M.mean, step = niceStep(2 * (M.maxAbsDev || 0.01));
    if (!isFinite(step) || step <= 0) return;
    var gx = function (i) { return (i / (X - 1)) * W; }, gy = function (j) { return (1 - j / (Y - 1)) * H; };
    for (var L = -8; L <= 8; L++) {
      var level = L * step, isZero = L === 0;
      bctx.strokeStyle = isZero ? "rgba(239,239,239,.5)" : "rgba(186,186,186,.22)";
      bctx.lineWidth = (isZero ? 1.4 : 0.8) * dpr; bctx.beginPath();
      for (var j = 0; j < Y - 1; j++) for (var i = 0; i < X - 1; i++) {
        var vv = [d[j][i], d[j][i + 1], d[j + 1][i + 1], d[j + 1][i]];
        if (vv.some(function (z) { return z === null || Number.isNaN(z); })) continue;
        var dev = vv.map(function (z) { return z - mean - level; });
        var cx = [i, i + 1, i + 1, i], cy = [j, j, j + 1, j + 1], pts = [];
        for (var k = 0; k < 4; k++) {
          var k2 = (k + 1) % 4, a = dev[k], b = dev[k2];
          if ((a < 0) !== (b < 0)) { var t = a / (a - b); pts.push([gx(cx[k] + (cx[k2] - cx[k]) * t), gy(cy[k] + (cy[k2] - cy[k]) * t)]); }
        }
        for (var p = 0; p + 1 < pts.length; p += 2) { bctx.moveTo(pts[p][0], pts[p][1]); bctx.lineTo(pts[p + 1][0], pts[p + 1][1]); }
      }
      bctx.stroke();
    }
  }
  function drawColorbar() {
    var h = cbar.height, w = cbar.width;
    for (var y = 0; y < h; y++) { var c = colorAt(1 - y / (h - 1)); cbctx.fillStyle = "rgb(" + c[0] + "," + c[1] + "," + c[2] + ")"; cbctx.fillRect(0, y, w, 1); }
    if (opts.mode === "deviation") { var s = M.maxAbsDev || 0; $("mv-barHi").textContent = fmt(s, 3); $("mv-barMid").textContent = "0.000"; $("mv-barLo").textContent = fmt(-s, 3); }
    else { $("mv-barHi").textContent = fmtAbs(M.max); $("mv-barMid").textContent = fmtAbs((M.max + M.min) / 2); $("mv-barLo").textContent = fmtAbs(M.min); }
  }
  function setStats() {
    var u = '<span class="u">mm</span>';
    $("mv-sRange").innerHTML = fmtAbs(M.max - M.min) + u;
    $("mv-sStd").innerHTML = fmtAbs(M.std) + u;
    $("mv-sMax").innerHTML = fmt(M.max - M.mean) + u;
    $("mv-sMin").innerHTML = fmt(M.min - M.mean) + u;
    $("mv-sMean").innerHTML = fmtAbs(M.mean) + u;
    $("mv-xMin").textContent = M.x_min.toFixed(0);
    $("mv-xMax").textContent = (M.x_min + (M.x_points - 1) * M.x_step).toFixed(0);
  }
  function buildModel(j) {
    var vals = [];
    for (var r = 0; r < j.data.length; r++) for (var c = 0; c < j.data[r].length; c++) { var z = j.data[r][c]; if (z !== null && !Number.isNaN(z)) vals.push(z); }
    var m = Object.assign({}, j);
    if (!vals.length) { m.empty = true; return m; }
    m.min = Math.min.apply(null, vals); m.max = Math.max.apply(null, vals);
    m.mean = vals.reduce(function (a, b) { return a + b; }, 0) / vals.length;
    m.std = Math.sqrt(vals.reduce(function (a, b) { return a + (b - m.mean) * (b - m.mean); }, 0) / vals.length);
    m.maxAbsDev = Math.max(m.max - m.mean, m.mean - m.min) || 1e-6; m.empty = false;
    return m;
  }
  function showOverlay(big, small) { $("mv-ovBig").textContent = big; if (small !== undefined) $("mv-ovSmall").innerHTML = small; $("mv-overlay").classList.add("show"); }
  function hideOverlay() { $("mv-overlay").classList.remove("show"); }

  function load() {
    fetch("/api/v1/mesh", { credentials: "same-origin", headers: { Accept: "application/json" } })
      .then(function (res) {
        if (res.status === 401) { showOverlay("Sign in required", 'Open <a href="/">PrusaLink</a>, log in, then reload.'); return null; }
        if (!res.ok) throw new Error("HTTP " + res.status);
        return res.json();
      })
      .then(function (j) {
        if (!j) return;
        M = buildModel(j); setStats();
        if (M.empty || !j.valid) { showOverlay("No mesh on record", "Run a bed level (First Layer Calibration / <code>G29</code>), then refresh."); return; }
        hideOverlay(); drawColorbar(); render();
      })
      .catch(function (err) { showOverlay("Couldn’t reach the printer", "The mesh endpoint didn’t respond (" + err.message + "). Check the connection and refresh."); });
  }

  function fetchName() {
    fetch("/api/v1/info", { credentials: "same-origin", headers: { Accept: "application/json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) { if (j && j.hostname) $("mv-name").textContent = j.hostname; })
      .catch(function () {});
  }

  // ---------- bed leveling trigger ----------
  function resetLevelBtn() { var b = $("mv-level"); b.disabled = false; b.classList.remove("arm"); b.textContent = "⬡ Run bed leveling"; }
  function startLeveling() {
    probing = true; var b = $("mv-level"); b.classList.remove("arm"); b.disabled = true; b.textContent = "Starting…"; hideOverlay();
    fetch("/api/v1/mesh", { method: "POST", credentials: "same-origin" })
      .then(function (res) {
        if (res.status === 409) { probing = false; resetLevelBtn(); showOverlay("Printer busy", "Bed leveling can only start when the printer is idle."); return; }
        if (res.status === 401) { probing = false; resetLevelBtn(); showOverlay("Sign in required", 'Open <a href="/">PrusaLink</a>, log in, then reload.'); return; }
        if (res.status !== 202 && !res.ok) throw new Error("HTTP " + res.status);
        pollLeveling(Date.now(), false);
      })
      .catch(function (err) { probing = false; resetLevelBtn(); showOverlay("Couldn’t start leveling", err.message); });
  }
  function pollLeveling(t0, sawBusy) {
    if (!probing) return;
    var elapsed = Math.round((Date.now() - t0) / 1000);
    $("mv-level").textContent = "Probing bed… " + elapsed + "s";
    if (elapsed > 360) { probing = false; resetLevelBtn(); showOverlay("Leveling timed out", "The printer didn’t report back. Check it and refresh."); return; }
    fetch("/api/v1/status", { credentials: "same-origin", headers: { Accept: "application/json" } })
      .then(function (r) { return r.ok ? r.json() : {}; })
      .catch(function () { return {}; })
      .then(function (j) {
        if (!probing) return;
        var state = (j.printer || {}).state || "";
        if (state === "ERROR") { probing = false; resetLevelBtn(); showOverlay("Printer error", "An error occurred during leveling."); return; }
        if (BUSY[state]) sawBusy = true;
        if (DONE[state] && (sawBusy || elapsed > 10)) { probing = false; resetLevelBtn(); load(); return; }
        setTimeout(function () { pollLeveling(t0, sawBusy); }, 2500);
      });
  }

  // ---------- wiring ----------
  function wire() {
    $("mv-refresh").addEventListener("click", load);
    var lvl = $("mv-level");
    lvl.addEventListener("click", function () {
      if (probing) return;
      if (!lvl.classList.contains("arm")) {
        lvl.classList.add("arm"); lvl.textContent = "Probe now? Moves the toolhead — click to confirm";
        clearTimeout(armTimer); armTimer = setTimeout(resetLevelBtn, 4000); return;
      }
      clearTimeout(armTimer); startLeveling();
    });
    function toggle(id, key) {
      var t = $(id);
      t.addEventListener("click", function () { opts[key] = !opts[key]; t.classList.toggle("on", opts[key]); t.setAttribute("aria-pressed", String(opts[key])); if (M && !M.empty) render(); });
      t.addEventListener("keydown", function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); t.click(); } });
    }
    var tm = $("mv-tMode");
    tm.addEventListener("click", function () { opts.mode = opts.mode === "deviation" ? "absolute" : "deviation"; tm.textContent = opts.mode; if (M && !M.empty) { drawColorbar(); render(); } });
    tm.addEventListener("keydown", function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); tm.click(); } });
    toggle("mv-tContour", "contour");
    toggle("mv-tProbe", "probe");

    bed.addEventListener("mousemove", function (ev) {
      if (!M || M.empty) { probeEl.style.opacity = 0; return; }
      var rect = bed.getBoundingClientRect();
      var gx = ((ev.clientX - rect.left) / rect.width) * (M.x_points - 1);
      var gy = (1 - (ev.clientY - rect.top) / rect.height) * (M.y_points - 1);
      var np = nearestProbed(M, gx, gy);
      var dpx = Math.hypot(((np.i - gx) / (M.x_points - 1)) * rect.width, ((np.j - gy) / (M.y_points - 1)) * rect.height);
      var html;
      if (dpx <= 18 && isProbed(M, np.i, np.j)) {
        var z = M.data[np.j][np.i];
        if (z === null || Number.isNaN(z)) { probeEl.style.opacity = 0; return; }
        html = '<span class="l">measured</span> X ' + (M.x_min + np.i * M.x_step).toFixed(1) + " Y " + (M.y_min + np.j * M.y_step).toFixed(1) + " mm<br>"
          + '<span class="l">Z</span> <b>' + fmtAbs(z) + ' mm</b> <span class="l">(' + fmt(z - M.mean) + " vs mean)</span>";
      } else {
        var zs = sample(gx, gy);
        if (Number.isNaN(zs)) { probeEl.style.opacity = 0; return; }
        html = '<span class="l">interp.</span> X ' + (M.x_min + gx * M.x_step).toFixed(1) + " Y " + (M.y_min + gy * M.y_step).toFixed(1) + " mm<br>"
          + '<span class="l">Z</span> <b>' + fmtAbs(zs) + ' mm</b> <span class="l">(' + fmt(zs - M.mean) + " vs mean)</span>";
      }
      probeEl.innerHTML = html; probeEl.style.opacity = 1;
      var stage = probeEl.parentElement.getBoundingClientRect(), pw = probeEl.offsetWidth, ph = probeEl.offsetHeight;
      var lx = ev.clientX - stage.left + 14, ly = ev.clientY - stage.top + 14;
      if (lx + pw > stage.width) lx = ev.clientX - stage.left - pw - 14;
      if (ly + ph > stage.height) ly = ev.clientY - stage.top - ph - 14;
      probeEl.style.left = Math.max(0, lx) + "px"; probeEl.style.top = Math.max(0, ly) + "px";
    });
    bed.addEventListener("mouseleave", function () { probeEl.style.opacity = 0; });
  }

  // ---------- route activation ----------
  function injectStyleOnce() { if (!$("mv-style")) { var s = document.createElement("style"); s.id = "mv-style"; s.textContent = STYLE; document.head.appendChild(s); } }
  function setNavActive() {
    var nav = $("navbar");
    if (nav) {
      Array.prototype.forEach.call(nav.querySelectorAll("li"), function (li) { li.classList.remove("active"); });
      var a = document.querySelector('#navbar a[href="#mesh"]');
      if (a && a.parentNode) a.parentNode.className = "active";
    }
    document.title = "Bed Mesh";
  }
  function activate() {
    var root = $("root"); if (!root) return;
    injectStyleOnce();
    root.innerHTML = HTML;
    bed = $("mv-bed"); bctx = bed.getContext("2d"); cbar = $("mv-colorbar"); cbctx = cbar.getContext("2d"); probeEl = $("mv-probe");
    M = null; probing = false;
    setNavActive(); wire(); fetchName(); load();
  }
  function onRoute() { if (window.location.hash === "#mesh") activate(); }

  window.addEventListener("hashchange", onRoute);
  window.addEventListener("load", onRoute);
  window.addEventListener("resize", function () { if (window.location.hash === "#mesh" && M && !M.empty) render(); });
  if (document.readyState === "complete") onRoute();
})();
