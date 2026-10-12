# Changelog

## 0.0.1 — 2026-10-11

First release, macOS and Windows.

- **The sink.** A menu-bar application QCView starts when its Output is
  switched on: the viewport (single or dual view, splits, wipes, difference,
  the display transform, safety guides, note strokes) and the mixed audio
  as an NDI® source named "QCView". Publish-on-change from QCView, so a
  paused viewer costs the network nothing; 1080p BGRA by default, the
  raster, picture and layers in **Settings…** from the menu bar. The
  receiver count is reported back into QCView's Settings.
- **Lifecycle.** Started by QCView, it quits with QCView or when the output
  is switched off; started by hand, it waits for QCView and keeps the NDI
  source up between sessions.
- **Installers.** macOS: a notarized pkg that installs the app and
  registers it with QCView. Windows: a Setup that installs under Program
  Files and registers the output (not code-signed yet). The NDI runtime is
  inside the app on both; an installed runtime is preferred when present.
- **Tools.** `qcvo-probe`, which plays either end of QCView's output bus,
  and `ndi-recv`, a receiver that prints what arrives.

Requires QCView 2.5.4 or later; macOS 13 or later, or Windows 10 1809 or later.
