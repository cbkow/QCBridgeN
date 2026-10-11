# P3 — QCBridgeN, the NDI® sink, macOS (2026-10-11)

QCBridgeN `f422ca3` against QCView `5f5ea6f1`. The first run of the real
sink, measured one hop further than the ring: a second process received
the NDI stream and reported what arrived.

## Setup

- NDI SDK for Apple 6.3.2 installed; headers taken from
  `/Library/NDI SDK for Apple/include` at build time, `libndi.dylib`
  found at run time (the SDK's environment variable first, then
  `/usr/local/lib`, then the SDK's lib folder). Nothing from the SDK is
  in the repository or linked.
- QCView started the sink from a manifest (`exec` = `build/QCBridgeN`,
  args `--id ndi --name QCView`) when the output was enabled; the
  tray item showed while it ran.
- `ndi-recv` (tools/) found the source `NY-SW04.LOCALDOMAIN (QCView)`,
  connected at highest bandwidth with BGRX/BGRA colour, and captured for
  14 s while QCView autoplayed the 25 fps 1080p-from-720p clip.

## Measured (`ndi-recv.txt`, `qcview-and-sink-lines.txt`)

- **Handshake and launch.** `ndi enabled → started … QCBridgeN` →
  `QCBridgeN: attached … started by QCView` → request accepted → `output
  pass 1920x1080` → `NDI 'QCView': 0 receivers` → `1 receiver` when the
  receiver connected → `0 receivers` when it left.
- **Video at the receiver.** 1920x1080 at 25/1, **25.07 fps** over 14 s
  (351 frames, +25 every second, one +26), one connection; the centre
  pixel constant at the pattern's value; `ndi-received-frame.png` is the
  last frame as received over NDI — the clip, timecode burn-in and all.
- **Audio at the receiver.** 48 128 samples a second (48 kHz), stereo,
  1313 audio frames in 14 s; the sink reported 1562 packets, 0 gaps, 0
  dropped for the whole run.
- **Lifecycle.** QCView killed with SIGTERM → the sink logged "QCView is
  gone", "quitting (started by QCView)", and exited (checked: no
  QCBridgeN process afterwards). The NDI source existed for the whole
  sink lifetime.
- **Cost.** The sink used the synchronous send (the SDK copies before
  returning) under the ring claim; at 1080p25 that is well inside a
  frame. Not measured yet in numbers.

## Open

- Real timecode when the stamp carries one (the stamp never does yet, P2).
- The async send with a second claim, if the synchronous copy ever shows
  at UHD.
- The tray is status + Quit; the settings (source name, raster, content,
  layers) are QSettings and command line. A small settings window is the
  next UI step, or the manifest-declared settings rendered by QCView
  (O12's later form).
- An app bundle with LSUIElement (no Dock icon), an icon, an installer
  that registers the manifest (P5).
- Windows: the sink's build on the box (Qt + the Windows NDI SDK) waits
  on QCView's D3D11 output pass (tracker), since without it there are no
  frames to send there.

## By hand (chris, 2026-10-11 evening)

NDI Video Monitor on the same Mac, QCView started the sink from the
registered manifest, the 5.1 test clip playing: picture and audio as in
the viewport, latency "barely noticeable" locally. The first look at the
stream by a person.
