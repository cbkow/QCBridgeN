# QCBridgeN — QCView's output bus and its first sink

The plan for getting what QCView shows and plays out of QCView and onto a
wire, written 2026-10-11 from the design discussion. Three repositories
take part: QCBridgeAE (the ring code), QCView-Player (the bus) and this one
(the NDI® sink). Decisions here are the ones the code will cite as
`PLAN On`; the measured results go in `lab/results/<date>-<slug>/` the
way the other repos do it.

## Decisions

- **O1 — A separate process in a separate repository, never a plugin
  loaded into QCView.** QCView is GPLv3 and links the GPL build of FFmpeg
  on both platforms, so no exception can make a binary that also contains
  the NDI SDK distributable. The SDK's own licence (§3d) requires any
  product containing it to ship under terms that forbid modifying or
  reverse engineering the SDK, which the GPL forbids demanding; and §3a
  makes the SDK headers confidential, so they can never sit in a public
  repository. A process boundary with a documented data-only protocol
  settles all three.
- **O2 — QCView publishes a generic output bus; sinks consume it.** The bus
  is a pair of shared-memory rings per enabled output (frames, audio),
  built on the QCBridgeAE ring with the roles reversed: QCView is the
  producer. One process per sink plugin. QCView owns the list of outputs:
  the user enables one in Settings, QCView creates its rings (named by the
  plugin's manifest id) and starts the plugin; the plugin opens its rings
  by name. There is no discovery and no uninvited consumer.
- **O3 — The output is the end of the chain.** The corrected canvas: the
  composite (single, side-by-side, wipe, difference), the OCIO display/view
  for the requested transfer target, safety guides and annotations. It is
  rendered by the renderer's existing capture chain, generalised to a
  requested raster, format and transfer, once per changed frame, straight
  into a ring slot. A content selector lets a sink ask for the composite,
  side A or side B; side A without guides is the clean source. The canvas
  cap (3840x2160) is the output cap, so an 8K source never crosses.
- **O4 — The sink states what it wants; QCView imposes no policy.** The
  consumer writes a request (raster, pixel format, transfer target, fit,
  content, maximum rate, included layers); QCView replies with what it will
  produce or refuses with a reason. No request, no render. "Never above
  1080p" is the NDI plugin's request, not QCView's rule.
- **O5 — Publish on change; sinks own their clocks; lossy by design.**
  QCView writes a frame only when the canvas changed, stamped with its
  presentation time. The NDI sink sends as frames arrive; an SDI sink runs
  the card clock and repeats or drops. The producer never blocks on a
  consumer; a late sink loses frames, not QCView. This bus is a monitor
  feed, not a recording path.
- **O6 — Audio is what you hear, before the listening level.** Tapped
  after the fold, the dual mix and the per-side mutes, before master
  volume. Planar float32 at 48 kHz, packets stamped on the same clock as
  the frames. Master mute (M) mutes the feed too; a separate feed mute in
  the Output section mutes the feed alone. Muted audio is still sent, as
  zeros with a muted flag, so sinks never lose their audio cadence.
- **O7 — RGB now, standard packings in the shader when a sink needs
  them.** BGRA8 and RGBA16F first. P216 and v210 are well-defined formats
  the output shader can write nearly for free, so they join the request's
  format list when a sink asks; CPU packing at UHD would cost
  milliseconds a frame. Card-specific work (audio embedding, cadence
  patterns, genlock, connectors) stays in the plugin.
- **O8 — Timecode is passed through, never invented.** The frame stamp
  carries source timecode when the file or host has one, plus frame
  index, fps and presentation time. In dual the stamp carries side A's.
  Sinks synthesise if their wire wants one and nothing came.
- **O9 — The copy budget is zero on the Mac and one on Windows.** The ring
  slot is page-aligned mapped memory; Metal wraps it as a buffer and the
  output pass blits into it. D3D11 cannot wrap foreign memory: staging
  texture, Map, one memcpy. The plugin hands the slot pointer to the SDK.
  Cross-process GPU sharing only relocates the readback, because every
  sink in view wants CPU memory.
- **O10 — Name, licence, trademark.** The repository is QCBridgeN, MIT.
  The NDI headers are never committed; the build finds the SDK where the
  installer put it, as QCBridgeAE finds Adobe's. The NDI mark appears only
  in a compatibility statement with the ® and the owner notice, never in a
  product name.
- **O11 — The ring code is dual-licensed.** `src/common/surface/` in
  QCBridgeAE (frame ring, audio ring, process-alive) gets an MIT licence
  from its copyright holder, with SPDX headers, so it can be vendored into
  this repository. QCView's vendored copy already claims MIT in its header
  comment; after this step that claim is true.
- **O12 — Sinks are full applications.** `QCBridgeN.app` in `/Applications`
  and `C:\Program Files\QCBridgeN\` on Windows, with their own installers,
  icons and menu-bar or tray presence. A user can start one from the Dock
  or Start menu, or QCView can. The manifest (id, name, executable,
  protocol version) lives inside the bundle; the installer registers a
  pointer to it where QCView looks (`/Library/Application
  Support/QCView/Outputs/<id>.json` on macOS, `HKLM\Software\QCView\Outputs\<id>`
  on Windows). Registration, not scanning: QCView lists what installers
  told it about.
- **O13 — Lifecycle: whoever started it owns it.** The rings are QCView's
  and exist while the output is enabled. A sink, however it was started,
  opens its rings by name when they exist and waits when they don't,
  showing "waiting for QCView" and, as an option, keeping its wire source
  alive with a slate so downstream routing holds. Enabling an output in
  QCView creates the rings and starts the sink if it is not already
  running; QCView marks that launch (a flag in the header the sink reads
  at attach). Disabling retires the rings: a sink QCView started quits; a
  sink the user started goes back to waiting. QCView restarts a sink it
  started that exits with the rings still live, with backoff and a cap
  (three in a minute, then it reports and stops); a user-started sink is
  reported only. QCView quitting or crashing retires the rings or leaves a
  dead producer pid; the sink notices either, returns to waiting, and
  reattaches when the rings come back. Sleep and wake are the same case
  seen through heartbeats. Protocol mismatch is a refusal with a message
  on whichever side is older.
- **O14 — Included layers default on.** The NDI request asks for the
  composite with safety guides and annotations included; the Output
  section can turn either off per output. QCView's guides are the guides.

## The protocol, on one page

Rings `/qcview-out-<plugin>` and `/qcview-out-<plugin>-audio` (`Local\`
prefix on Windows, as today). Same slot and seqlock machinery as the
Transmit rings, new magic and version so the Transmit contract never moves.

Header, producer-written: magic, version, producer pid, slot geometry,
reply block. Header, consumer-written: consumer pid, heartbeat (monotonic
ns, refreshed every 250 ms), last seen sequence, request block, status
string (128 bytes, shown verbatim in QCView's Output section).

Request: protocol version, width, height, pixel format (BGRA8, RGBA16F;
later P216, v210), transfer target (SDR Rec.709, PQ Rec.2020, HLG), fit
(letterbox to raster, native size), content (composite, A, B), maximum
rate, included layers (guides, annotations).

Reply: accepted flag, the raster and format QCView will produce, a reason
code and message when refused (too large, unsupported format, no source).

Frame descriptor: width, height, bytes per row, pixel format, transfer,
content, presentation time (ns, QCView's monotonic clock), source frame
index, source fps as a rational, timecode (packed, with a valid flag),
flags (changed layers, A present, B present, non-finite values as today).

Audio: the existing audio ring, session = {48000, 2 channels, flags},
packet time on the same clock as the frames, flag bit for muted.

Rules: no request, no render; render only while the heartbeat is fresh;
publish only on change; a refused request is re-read when it changes;
either side's version mismatch is a refusal with a message, never a
guess.

## Phases

Each phase ends with evidence in `lab/results/`, checked on the consumed
property (the bytes a sink reads), not on internal state.

**P0 — Licence groundwork (QCBridgeAE, QCView). DONE 2026-10-11**
(QCBridgeAE `308536e`, QCView `2d0faab1`). MIT dual-licence for
`src/common/surface/` with a LICENSE file in that folder and SPDX headers;
the README names the split. Re-vendor into QCView so its header comment is
accurate. Exit: the commit exists and the copyright holder has signed off.

**P1 — Protocol, lifecycle and probe (QCBridgeAE ring code, QCBridgeN
probe). DONE 2026-10-11** (QCBridgeAE `5849977`, this repo's
`tools/qcvo-probe`, `lab/results/2026-10-11-p1-probe/`; 9/9 tests on the
Mac, 7/7 on Windows). Make
the ring generic over its descriptor so the output ring is a sibling of
the Transmit ring, not a fork. Add the consumer side of the header
(heartbeat, request, status) and the reply. `qcvo-probe` in this
repository, MIT, plays both parts: as a fake consumer it writes a request,
dumps frames, samples pixels and counts audio continuity; as a fake
producer it feeds a plugin under test without QCView, and it can retire
its rings, die with them live, and come back, so every O13 transition is
a test before either real side exists. Unit tests beside the audio ring
tests. Exit: tests pass on both OSes (tracker items in
`lab/TRACKING-windows.md`).

**P2 — The bus in QCView, macOS first. LANDED 2026-10-11** (QCView
`5f5ea6f1`, measured in `lab/results/2026-10-11-p2-bus/`; the Windows
build of the same commit passes on the box, its D3D11 output pass is
owed — tracker). Still open inside P2: timecode in the stamp, the Native
fit, RGBA16F and the HDR transfers, SideB when the window is in dual, the
per-output display/view override, audio cadence while the transport is
paused (the device stops, so no packets flow), and a cost figure for the
pass on the GPU. Output pass on the render
thread: when a request is accepted and the heartbeat is fresh, run the
capture chain per changed frame at the requested raster into the slot,
Metal buffer over the mapped slot, no copy. Change detection as a content
dirty flag distinct from UI redraws. Content selector. Stamps. Audio tap
in AudioPlayer and DualAudioMixer, lock-free into the audio ring from the
device callback. Settings ▸ Output: per-plugin enable (which also starts
the plugin from its manifest), feed mute, included layers, per-output
display/view override, status line from the consumer's string. Exit: the
probe shows frames only when the canvas changed and at source rate in
single and dual at mixed fps; a probed frame's pixels equal a screenshot
of the same frame; audio continuity counter unbroken across play, pause,
seek and mute; measured cost per frame at 1080p and UHD; measured zero
cost with no output enabled.

**P3 — QCBridgeN, the NDI sink, macOS first. LANDED 2026-10-11**
(`f422ca3`, `lab/results/2026-10-11-p3-ndi/`: a second process received
1920x1080 at 25.07 fps and 48 kHz stereo over NDI, the frame is the clip,
the sink quit with QCView). Open: real timecode, the async send, a
settings window or manifest-declared settings, the app bundle and
installer (P5), Windows (after P4's D3D11 pass). Ring reader (vendored),
request 1080p BGRA8 SDR letterbox by default (configurable), NDI send of
video and audio with the stamps, async send on the slot pointer, runtime
loaded dynamically from the SDK's redist location, reconnect when QCView
restarts or retires the ring, menu-bar item with status and the NDI
source name, status string back (receiver count as tally). Exit: NDI
Studio Monitor shows the feed; frame-to-receiver latency measured with a
counter burn-in; receiver audio/video offset measured; CPU cost of the
plugin measured; behaviour across QCView quit and relaunch recorded.

**P4 — Windows twins.** D3D11 output pass (staging + one memcpy), the
same audio tap, the plugin with the NDI Runtime installed. Tracker items
from the day P2 lands, run from the Mac seat.

**P5 — Packaging and release.** Application installers (pkg, Setup)
carrying only the redistributable runtime files the SDK documentation
names, writing the registration from O12; a QCView release that includes
the bus; docs page. The release gate is the same as before: nothing ships until both
platforms are built and measured.

**Later, by design already allowed:** SRT sink (could live in QCView
itself, FFmpeg and libsrt are GPL-compatible); DeckLink sink (proprietary
SDK, plugin); AJA sink (NTV2 is MIT, plugin for uniformity); Syphon/Spout
publish of the same output pass as a bonus for existing setups; HDR
transfer targets and P216.

## Settled with the owner (2026-10-11)

MIT (O10); full applications with installer registration (O12); the
lifecycle rule in O13, which the owner called the hard part and should be
the first thing the probe and the fake producer exercise; guides and
annotations on by default (O14).

## Measurements the plan owes

- Output pass GPU time at 1080p and UHD, single and dual, Metal and D3D11.
- Windows memcpy per frame at UHD BGRA8.
- End-to-end latency QCView canvas → NDI receiver, Mac and Windows.
- Audio/video offset at the receiver with the stamps honoured.
- Change-rate correctness: 24 fps beside 25 fps publishes the union, not
  the display rate.
- Cost with an output enabled and no consumer attached (must be the
  rings' existence only).
