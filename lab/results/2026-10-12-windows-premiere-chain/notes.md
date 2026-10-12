# Windows: "frame drops" and "every 4th frame a duplicate" with a Premiere feed (2026-10-12)

Reported from the box: Premiere → QCView (QCBridgeAE Transmit) → NDI →
Studio Monitor, 1080p 23.976, "definitely frame drops", later "every 4th
frame in QCView is a duplicate from Premiere"; the Mac does neither.
Measured stage by stage. Nothing in the chain drops or repeats a frame in
steady state; the duplicate is the 30 fps screen capture of a 23.976
display, and Premiere's own monitor shows it in the same recording.

## Instruments added for this

- QCView's ring reader (`HostBridgeSource`) now counts, per 240 frames:
  sequences the producer published that it never took (`skipped`), frames
  whose pixels fingerprint like the previous one (`duplicate pixels`),
  frames stamped with the previous host time (`same host time`), and pool
  allocations. `QCV_BRIDGE_TRACE=1` logs one line per such event with the
  gap since the last take and the copy time. (QCView f957edc2, 7ddc3e49,
  d548efa9.)
- `ndi-recv` prints each frame's age against the SDK's send stamp (the NDI
  hop on its own) and counts frames whose pixels repeat the previous
  frame's. (QCBridgeN 863e660, 809f0e5, and the fingerprint spread across
  the row: a side-by-side picture's middle is its seam.)

## The chain, measured (dev QCView at 7ddc3e49, QCBridgeN 0.0.1 sink)

Premiere playing the 23.976 sequence from its start, Studio Monitor
attached and showing the source, `ndi-recv` counting beside it.

| stage | rate | distinct? |
|---|---|---|
| plugin push (Premiere → ring) | 23.97–24.00 fps, push 2.2 ms mean | — |
| QCView intake (ring → viewport) | 23.97–24.01 fps, skipped 0 | duplicate pixels 0, same host time 0, per 240 |
| output pass (publish) | 24–25/s, 0 skipped | — |
| receiver (NDI, same box) | 24.00 fps (720 in 30 s; 959 in 40 s) | 0 repeats in steady state |

Premiere's stamps advance 10 594 584 000 ticks a frame at 254 016 000 000
ticks/s: **23.976**, confirmed from the host rather than assumed. CPU was
27 % during the run.

Two things are real and transient: the first eight seconds after play
show intake skips (Premiere's own push ramps from 17 fps; the pool's first
two 16 MB images page-fault at 35–39 ms); and a stopped or looping
sequence pushes its still frame repeatedly, which is what "duplicate
pixels" counts before play and after stop. Neither is a steady-state
drop.

## The recording

`ShareX` capture of Premiere's program monitor beside QCView's viewport:
1278×844, **30 fps**, 8.27 s, 247 frames. Frame-to-frame difference of
each window's timecode burn-in (ffmpeg `tblend=difference` + `signalstats`
on a crop; a repeat is a near-zero difference):

| window | repeats in 247 captured frames | gaps between repeats |
|---|---|---|
| Premiere's program monitor | 53 | mostly 5 |
| QCView's viewport | 58 | mostly 5, some 2–3 and 6–7 |

A 23.976 fps picture captured at 30 fps repeats one captured frame in
five (30/24 = 1.25): "every 4th frame is a duplicate" is the capture's
cadence, and Premiere's own monitor has it too. QCView's gaps are less
regular because its window presents on a 60 Hz vsync (3:2 hold pattern,
phase drifting against the 30 Hz capture). On a 120 Hz display 24p
divides evenly and there is no such pattern, which is one reason the Mac
looks different by eye.

## Found on the way

- QCView told NDI every live feed was **25 fps** (the bus asked the video
  decoder, which has no frame index for a live item; Studio Monitor showed
  "1080/25p"). Fixed: a QCBridge feed reports the host's cadence once
  three consecutive stamp intervals agree, an SRT stream its declared
  rate (QCView 53bfa0d1 + 2c55d139).
- `ndi-recv`'s "connections" is the receiver's own connection count, not
  the number of receivers on the source.

## Dual view through the output on Windows

Two files side by side (`--simulate-user A B`): the receiver's frame dump
is the side-by-side composite, both sides correct. Live Premiere on A with
a file on B: see the addendum below.

### Addendum: live Premiere on A, file on B, transport paused

Before: the receiver got one frame in 12 s while Premiere played — the
D3D11 dual compositor re-uploaded the live side's latest frame every
draw but nothing recorded it as new, and the output stamp in dual view
moved only with the master frame index. So a paused dual view with a
live side went out frozen; it moved only while QCView itself played,
which is how it was reported ("live sources are attached to the
play/pause button, for NDI output").

Fix (QCView e5f15a23, and 5c2bc614 for the Metal pass, same count): the
dual compositor counts each side's newly resolved frame; the renderer
folds a change into `displayedFrames`, which the stamp already reads.

After, same run (QCView paused in dual, Premiere playing from the
start): **288 frames in 12 s = 24.00 fps, 0 repeats** once past the
black leader, declared 24000/1001, hop age 38–41 ms. The frame dump is
the side-by-side composite with the live picture on the left.
