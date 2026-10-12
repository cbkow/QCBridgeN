# Windows: the sink got every other frame (2026-10-11, evening)

Reported from the box with the installed QCView 2.5.4 (MSIX) and the
installed QCBridgeN 0.0.1: "very choppy", also in NDI Studio Monitor on
the same machine, so not the network. The P4 run that afternoon had
measured 25.00 fps at the receiver — from `qcvo-probe`-style playback of
a 25p clip, which turned out not to show it (see below).

## Measured before the fix

QCView's own log (`output publish (N so far)` once a second) from the
reported sessions, two 23.976p clips off a network volume:

| session | publishes | span | rate |
|---|---|---|---|
| 1 | 507 | 52 s | ~9.7/s |
| 2 | 181 | 18 s | ~10/s |

Controlled on the box, the installed 2.5.4 playing the 1080p DNxHR
23.976p test clip, `ndi-recv` on the same machine: **187 frames in 15 s
= 12.47 fps**, +12/+13 every second, steady. The publish counter in
QCView's log agreed (180 in 15 s). So the sink forwarded everything it
was given; the *producer* published every other frame.

## Cause

`d3d11_player_renderer.cpp`, the output pass: one staging texture per
output. Frame N is drawn and copied into it; at the start of the next
draw it is mapped with `DO_NOT_WAIT`; a copy still on the GPU made the
pass *skip* frame N+1 ("lossy by design, O5"). With vsync and the
swapchain's frame queue the GPU runs two or three vblanks behind the
render thread, so at 24 fps the copy of N was routinely unfinished when
N+1 was drawn: N+1 skipped, N published at N+2, N+2 copied… half rate,
regular, exactly what a receiver shows as a 12 fps judder. The Metal
pass commits from the command buffer's completion handler and never had
the problem (P3: 25 of 25).

Why P4 measured 25 of 25 is not established: it was a 25p clip for 12 s,
and nothing in that run looked at the draw cadence. The controlled run
above is the recipe from now on: a 23.976p clip, `QCV_FPS_LOG=1` for
QCView's own rate, `ndi-recv` on the same machine, 15 s or more.

## Fix (QCView 646917ed + f0b00d96)

A three-deep ring of staging textures per output. Each frame is copied
into the next free one; at the start of every draw the oldest copies are
mapped without waiting and published **in draw order**, stopping at the
first the GPU has not finished. A frame is skipped only when all three
are still on the GPU. The trace line now counts both: `output publish
(N so far, W waits, S skipped)`.

## Measured after the fix

Same box, same clip, the dev build at f0b00d96 with `QCV_FPS_LOG=1`:

| | QCView new frames/s | published | received (`ndi-recv`, same box) | waits | skipped |
|---|---|---|---|---|---|
| output on | 24.4 | 352 in 14.4 s | **362 in 15.0 s = 24.13 fps**, +24/+25 every second | 22 (≈1.5/s) | 0 |
| output off | 24.4 | — | — | — | — |

The waits confirm the GPU really was behind on about one draw in sixteen;
the ring absorbs them. QCView's own rate is the same with the output on
and off.

## Two things learned about the packaged QCView

- **The MSIX registry is virtualized.** Writes the packaged app makes to
  `HKCU\Software\QCView` land in the package's private hive
  (`…\Packages\<family>\SystemAppData\Helium\User.dat`) and shadow the
  real key, so a test that flips `output\enabled\ndi` in the real HKCU
  does nothing to the installed app — it kept the state the user left in
  Settings. The dev build reads the real key. Drive the installed app
  through its UI, or test with the dev build.
- **Autoplay needs a key.** Opened from the command line the clip loads
  paused; the test sends a Space to QCView's own window after activating
  it by process id.
