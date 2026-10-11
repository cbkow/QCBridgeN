# P2 — the bus inside QCView, macOS (2026-10-11)

QCView main `5f5ea6f1`. The sink was `qcvo-probe consume`, registered as
output `probe` through a manifest (the dev manifest directory,
`QCV_OUTPUT_MANIFEST_DIR`), so QCView itself started it, answered it and
fed it, with no NDI anywhere. Everything below is what the probe read
from the rings or what QCView logged about the sink.

## What landed

- **Renderer (Metal).** After the corrected canvas exists, one extra pass
  per enabled sink at the sink's raster: the canvas compositor (single
  flow) or the dual compositor (dual flow, drawn once more with
  `consume=false`), the SDR-capture OCIO chain in its own instances, a
  BGRA8 target with the letterbox black and the guides/strokes the sink
  asked for, then a blit into the ring slot wrapped once as a no-copy
  MTLBuffer. The ring commit happens when the command buffer has
  completed: checked at the start of every frame, waited for before the
  same sink's next frame, and waited for synchronously when the target is
  removed (`setOutputTargets` under the renderer's source mutex).
- **Publish on change.** A stamp of everything the picture depends on
  (new frame counter, the source textures, layout, viewer aids, the OCIO
  snapshot, the guide's revision, a hash of the strokes, geometry, the
  request generation); a frame is published only when it differs.
- **Bus (WindowManager-owned).** Manifests from Application Support/
  QCView/Outputs (system, then user, then the dev directory), rings per
  enabled output, the plugin process (O13: QCView starts it, retires the
  rings to stop it, restarts a crash three times a minute then reports),
  request → reply at 4 Hz, presence by heartbeat and pid, the sink's status
  line, feed mute, included layers; settings persisted; Settings ▸ Output
  in the left rail.
- **Audio.** A tap in AudioPlayer and DualAudioMixer before Listen, mute
  and volume; the bus splits to planes and pushes 48 kHz stereo into every
  enabled audio ring, zeros with the muted flag when the master mute or
  the feed mute is on.

## Measured

1. **Handshake.** Manifest found → rings created → probe started by
   QCView → `opened /qcview-out-probe ... launched-by-producer yes` →
   request 1920x1080 BGRA8 SDR → `accepted "1920x1080 BGRA8 SDR from
   QCView"` → `sink attached` → `output pass 1920x1080 BGRA8`. All within
   the first poll ticks (`qcview-and-probe-lines.txt`).
2. **Cadence.** Paused: two frames (the first picture, then the first
   decoded frame) and nothing more for six seconds. Playing: one publish
   per source frame — sequence 100 carries source index 99 — at the
   clip's 25 fps (`output publish (27, 52, 78, 103 ...)` once a second).
   The first attempt published twice per frame; the trace line that was
   added to find it named the cause (the stored-strokes list is re-pushed
   on every frame change, and the stamp's memcmp read padding), both
   fixed before this run.
3. **Pixels.** `sink-read-frame.png` is the last frame the probe read,
   dumped from the ring: the 720p clip letterboxed into 1080p (16:9 into
   16:9, so edge to edge), bars, timecode burn-in, the moving line.
4. **Audio.** 1299 packets over the run, 0 gaps, 0 dropped, 0 muted; the
   session is 48000 Hz stereo. While paused the player stops its device,
   so no packets flow (open item below).
5. **Lifecycle.** Killing QCView with SIGTERM: the probe stopped with
   "producer is dead (pid gone)" — the destructor's retire does not run
   on a signal, and the pid path covered it. A clean quit retires.
6. **CPU on the QCView side**, 8 s of steady 1080p25 playback, CPU
   seconds from `ps`: output off 1.92 s (24 % of a core), on 1.79 s
   (22 %). No measurable cost; the pass is GPU work and the probe is its
   own process.
7. **Zero cost when off.** With the output disabled the manifest is read
   and nothing else happens: no rings, no pass, no process (log).
8. **Windows.** The same commit builds on the box (`build-qcview18.cmd`);
   its renderer has no output pass yet, so an enabled output there gets
   audio only — tracker item in QCBridgeAE `lab/TRACKING-windows.md`.

## Not done, by design or yet

- Guides and strokes in the output are implemented but this run had no
  guide loaded and no note, so `layers 0x0` on every frame; owed a run
  with a guide and a stroke.
- Timecode (stamp flag never set), the Native fit (refused), RGBA16F and
  the HDR transfers (refused), SideB while the window is in dual (the
  dual compositor has no B-only layout; the bus accepts it and the
  renderer skips), the per-output display/view override.
- Audio cadence while paused: the device stops with the transport, so
  the feed has no packets to carry zeros in. A sink that needs a
  continuous clock (SDI) will need the device kept running, or its own
  silence; NDI does not care.
- A GPU-time figure for the pass. CPU says nothing; a Metal GPU
  timestamp pair around the pass is the honest number.
- A Qt lesson for the vendored ring code, now in its header: a
  namespace named `slots` does not survive a Qt translation unit.
