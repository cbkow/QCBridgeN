# P4 — Windows: the D3D11 output pass and the sink on the box (2026-10-11)

QCView `f4f43840`, QCBridgeN `cdd077c`, both built on the Windows box
(VS 18, Qt 6.11.1 msvc2022_64, NDI 6 SDK at its default path). Driven
from the Mac seat; the box's desktop ran QCView, the sink and the receiver.

## The D3D11 pass

The Metal pass's twin, in `d3d11_player_renderer.cpp`: at each sink's
raster the compositor (single) or the dual compositor (prepared sources
kept with `consume=false`, `releasePrepared()` after) into an RGBA16F
target, the SDR-capture OCIO instances (`outputOcio`, `outputOcioB`, the
combine for per-side chains), a BGRA8 target with black letterbox and the
requested guides and strokes, then `CopyResource` to a staging texture.
At the start of the next frame the staging is mapped with
`D3D11_MAP_FLAG_DO_NOT_WAIT`; when the GPU is done, the slot is opened,
the rows memcpy'd (the one CPU copy Windows owes, O9) and the frame
committed. A still-pending copy makes the next changed frame skip (lossy
by design). Same change stamp as Metal.

Two build slips, both caught by the box and fixed in minutes: the inputs
struct defined after its first use (the same slip as on the Mac) and the
`output_target.h` include needing its `render/` path from qcv_window.

## Measured

- QCView started the staged `QCBridgeN.exe` from a dev manifest
  directory; request accepted; `output pass 1920x1080 BGRA8`; `output
  publish (…): changed frames` once per source frame at 25/s.
- `ndi-recv` on the same box: **1920x1080 at 25/1, 25.00 fps, 300
  frames in 12 s, 48 kHz stereo (48 480 samples a second, 1201 audio
  frames), one connection**; `ndi-received-frame-windows.png` is the last
  frame as received: the clip.
- QCView killed → QCBridgeN gone (checked: no process).
- WASAPI buffer on this box: 1056 frames (22 ms), as before.

## Packaging

`packaging/windows/build-setup.cmd`: `windeployqt` trimmed to Core, Gui,
Widgets, the platform and style plugins (`--no-svg --no-network --no-pdf`,
no image-format or icon-engine plugins: PNG is built into QtGui); the NDI
runtime DLL and its licence file beside the exe (the SDK allows the
runtime in the application folder; an installed runtime through
`NDI_RUNTIME_DIR_V6` wins at run time); `vc_redist.x64.exe` run quietly by
the installer. `dist/QCBridgeN-0.0.1-Setup-x64.exe`, 30 MB, the NDI
runtime being 29 of them. The Setup is **not code-signed** (no Windows
signing certificate on the box) and was not installed there (UAC); the
test ran the staged exe directly.

## Open

- A look at the tray icon on Windows and at the Settings dialog there.
- Code signing for the Setup (a certificate, or the Store route QCView
  takes).
- The QCView side on Windows is the dev build; the 2.5.4 MSIX for the
  Store is QCView's own release step.
