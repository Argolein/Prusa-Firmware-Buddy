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

Replacing the bundle from upstream overwrites `index.html`, so the Bed Mesh
Viewer integration must be re-applied:

* Keep `mesh.html` (the Bed Mesh Viewer page; not part of upstream) and its
  `add_gzip_resource("web/mesh.html" ...)` line in `../CMakeLists.txt`.
* Re-add the **Mesh** nav link in the new `index.html`: inside the static
  `<ul ... id="navbar">`, after the Storage `<li>`, insert
  `<li><a href="/mesh.html">Mesh</a></li>` (no `data-label`, so the SPA leaves
  the text literal). See [`docs/planning/bed-mesh-viewer.md`](../../../docs/planning/bed-mesh-viewer.md).
