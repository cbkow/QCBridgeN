# P5 — the macOS installer (2026-10-11)

`packaging/macos/build-pkg.sh` on `build/QCBridgeN.app` →
`dist/QCBridgeN-0.0.1-arm64.pkg`, 23 MB, signed with the Developer ID
Application and Installer identities in this Mac's keychain (not yet
notarized — `notarize.sh` is the next step and the owner's).

- The bundle: `LSUIElement` (menu-bar only), `QCBridgeN.icns` generated
  from the SVG mark through `tools/svg2png` (the OS thumbnailers flatten
  SVG alpha onto white, which is why the tool exists), the NDI runtime in
  `Contents/Frameworks/libndi.dylib` with its licence text in Resources
  (codesign refuses non-code files under Frameworks: the first build
  failed exactly there).
- Qt: macdeployqt, then a prune by what the binary and the kept plugins
  actually link, closed transitively. Before the prune, Qt Quick and the
  virtual keyboard rode in through the platform input-context plugin (44
  MB); after it, QtCore, QtGui, QtWidgets, QtDBus and the cocoa platform
  plugin plus the PNG image plugin (`payload.txt`). The staged app is run
  headless before signing and must report the NDI runtime.
- Embedded runtime proved to load: with `NDI_RUNTIME_DIR_V6` pointing at
  the bundle's Frameworks the app reported that path; by default the
  installed SDK copy wins, as `ndi_lib.cpp` orders it.
- Scripts: preinstall quits a running copy and removes the old app;
  postinstall writes `/Library/Application Support/QCView/Outputs/ndi.json`
  pointing at `/Applications/QCBridgeN.app`; `uninstall.command` removes
  both.
- Not done here: installing it (admin), notarizing (owner's profile), a
  look at the menu-bar item on a light and a dark menu bar, Windows.
