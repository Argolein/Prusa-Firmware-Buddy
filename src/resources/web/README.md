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

  See [`docs/planning/bed-mesh-viewer.md`](../../../docs/planning/bed-mesh-viewer.md).
