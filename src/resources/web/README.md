# Resources for the PrusaLink web server

We have assortment of static "files" served by the PrusaLink web server. This
acts as a client-side application and offloads as much processing there.

The application comes from the <https://github.com/prusa3d/Prusa-Link-Web>.
Updating:

* Clone the above repository.
* Build the `mini` configuration.
* Replace the content of this directory (keep the favicon and the README).
* Update the rules in `CMakeList.txt` one level up.
* The build system will automatically gzip-compress them and embed the results
  in the output.

## Argo fork additions (re-apply after updating the bundle)

The Bed Mesh Viewer is a first-class SPA route (`#mesh`) implemented as an
add-on module — `mesh-view.js` hooks the hash router and renders into the SPA's
own `#root`, reusing the real header/nav/sidebar. Replacing the bundle from
upstream overwrites `index.html`, so re-apply:

* Keep `mesh-view.js` (not part of upstream) and its
  `add_gzip_resource("web/mesh-view.js" ...)` line in `../CMakeLists.txt`.
* In the new `index.html`, load the module — add
  `<script defer="defer" src="mesh-view.js"></script>` right after the main bundle
  `<script ... src="main.*.js">`.
* Add the **Mesh** nav link: inside the static `<ul ... id="navbar">`, after the
  Storage `<li>`, insert `<li><a href="#mesh">Mesh</a></li>`. It is a normal hash
  route like `#dashboard`/`#files` — the SPA ignores unknown hashes
  (`Zt: if(!a) return false`), and `mesh-view.js` takes over `#mesh`.
* Patch the bundle's telemetry-update loop to tolerate the `#mesh` route. The
  SPA's `update` sets the page title via
  `Ut.routes.find((e=>e.path===t)).getTitle()` with **no** null check, so on every
  telemetry tick while the hash is `#mesh` it throws (route not found) → repeated
  "Application error". Guard it:
  `(Ut.routes.find((e=>e.path===t))||{getTitle:function(){return"Mesh"}}).getTitle()`
  (single occurrence in `main.*.js`). Navigation (`Zt`) is already guarded.
* Because that patch edits the bundle in place, **rename the file** so browsers
  don't serve the stale cached copy (JS is served with `max-age=86400`). Upstream
  names the bundle `main.<webpack-hash>.js`; since we patch the prebuilt file
  rather than re-running webpack, re-hash the patched contents yourself and rename
  to match — `shasum -a 256 main.*.js | cut -c1-20` gives a webpack-style 20-char
  token, so the file becomes `main.<new-hash>.js`. Re-hash whenever the bundle
  content changes. Update the `<script src>` in `index.html` and the
  `add_gzip_resource(...)` line in `../CMakeLists.txt` to match. (`index.html` is
  served uncached, so it always points browsers at the current filename.)

  See [`docs/planning/bed-mesh-viewer.md`](../../../docs/planning/bed-mesh-viewer.md).
