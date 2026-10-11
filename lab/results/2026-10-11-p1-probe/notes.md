# P1 — the output ring and the probe, both ends without QCView (2026-10-11)

What exists after today: the output ring in QCBridgeAE (`5849977`,
`src/common/surface/output_ring.*`, `protocol/output_desc.h`, on the shared
mapping and slot protocol every ring now uses) and `qcvo-probe` here, which
plays either side. Everything below is what one process read from the
mapping the other wrote.

## Unit test (QCBridgeAE `output_ring_unit`)

Names and the id rules; create/open; attach, heartbeat and presence with a
2 s stale beat; request → reply with generations (a second request is
generation 2, a refusal carries its reason and the generation it answers);
the status line, cut at 127 bytes; five frames and latest-wins with the
descriptor and the first and last pixel byte; the page-aligned slot; the
producer stepping over the held slot; the muted flag on the audio ring
beside it; and the O13 transitions: a consumer that closes clears its pid,
a dead pid with any beat is not present, a retire is read by a consumer
holding the old mapping, and a forked producer that publishes one frame
and `_exit`s leaves a ring whose producer reads dead and whose last frame
is still readable, which a new producer then takes over. 9/9 QCBridgeAE
tests on the Mac (the two Transmit tearing tests and the Metal zero-copy
test among them, so the refactor moved no bytes); **7/7 on the Windows
box** (`build-ae.cmd`, VS 18), `output_ring_unit` included (its forked
case is POSIX-only).

## Probe runs (`probe-runs.txt`, this folder)

1. **Normal.** Producer at 25 fps, consumer asks 1920x1080 BGRA8 SDR
   letterbox with guides and annotations. Attach, request, accept and the
   status line all seen by the producer within the same 0.3 s tick. The
   consumer took 101 frames in 3.96 s = 25.2 fps, 187 audio packets, 0
   gaps, 0 dropped; the producer reports the consumer's seen sequence
   tracking its own. Frame 100's centre pixel is the magenta bar it should
   be; `consumer-last-frame.png` is the last frame as the consumer read it
   (colour bars, the moving white bar, downscaled 4x).
2. **Producer retires at 2 s.** Consumer stops with "producer retired the
   ring" at once; 39 frames in 1.48 s.
3. **Producer dies at 2 s** (`_Exit`, no cleanup). Consumer stops with
   "producer is dead (pid gone)"; the orphaned ring was then taken over by
   the next `produce` without complaint (the create path).
4. **Consumer dies after 30 frames** (no close). Producer logged "consumer
   gone" 1.1 s later (the 1 s heartbeat age, no pid clear to help it) and
   published nothing further: frames stayed at 30 for the remaining 3 s
   while audio kept its cadence.
5. **Refusal, muted audio, launched flag.** An 8K request is answered
   "too large: above the 3840x2160 canvas cap"; the consumer reads the
   launched-by-producer flag; 94 audio packets arrive, all flagged muted,
   none dropped — the audio clock runs on through a refused request, which
   is what a sink's SDI embedding would need.

## What this settles for P2 and P3

- The handshake and the lifecycle table are implemented and exercised on
  both OSes before QCView or the SDK touch them; P2's QCView side and P3's
  sink can each be built against the probe.
- A clean consumer leaves instantly; a crashed one costs the producer one
  heartbeat age (1 s) of rendering into a dead ring. Acceptable; the age is
  a producer-side constant.
- Producer-side cost of the handshake: one atomic read per tick for the
  request generation, one for the heartbeat, a `process_alive` only when
  the pid is set.

## Not covered here

- Two consumers on one ring (the design says one ring per sink; the
  header has one consumer block, which is the enforcement).
- The ring re-create-larger path when a request exceeds the slot (the
  probe's producer sizes every slot for UHD RGBA16F, 66 MB, so it never
  needs it; QCView will size for the request and may re-create).
