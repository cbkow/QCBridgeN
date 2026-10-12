# Windows: four frames behind, then two (2026-10-12, morning)

After the every-other-frame fix (`2026-10-11-windows-cadence`) the
Windows feed was smooth but reported as noticeably later than the Mac's
— "it feels 300 ms slower". Measured, then fixed the part that was ours.

## Method

The timecode burn-in clip (`probe-51-720p25.mov`, 25p, frame number in
the top-left corner), playing in QCView with the NDI output on, NDI
Studio Monitor on the **same machine** showing the source, both windows
on screen. One desktop grab (`Graphics.CopyFromScreen` of the virtual
screen, DPI-aware) captures both at the same instant; the difference
between the two burn-ins is the glass-to-glass latency in frames, 40 ms
each. Read the burn-in *in QCView's viewport*, not QCView's "Frame N /
M" readout: the readout is a GUI label and sat one to two frames behind
the viewport in every grab.

Studio Monitor at its defaults (`lowest_latency` off in its settings
file; the Low Latency option was not tried).

## Before (QCView f0b00d96, the 2.5.4 Windows build)

One grab while playing: viewport 93, Studio Monitor 89 — **4 frames,
160 ms**. The person at the machine saw the same four.

## Cause of the part that was ours

The D3D11 output pass copies the wire picture to a staging texture at
the end of a draw and maps it at the *start of the next draw*. QCView
draws on demand, so at 25p the next draw is the next source frame: 40 ms
of latency for nothing, 80 when that map still found the copy in flight
(it did in about a quarter of them). Metal has none of this: the publish
runs from the command buffer's completion handler.

## Fix (QCView 46d2a862)

The render thread already ticks between draws (scope readbacks); the
outputs are settled there too, and the tick is 3 ms instead of 16 while
a copy is pending. A copy is now published within a few milliseconds of
the GPU finishing it.

## After (dev build at 46d2a862, same method, eight grabs 0.7 s apart)

| grab | QCView viewport | Studio Monitor | behind |
|---|---|---|---|
| 1 | 126 | 124 | 2 |
| 2 | 145 | 143 | 2 |
| 3 | 164 | 163 | 1 |
| 4 | 183 | 181 | 2 |
| 5 | 202 | 200 | 2 |
| 6 | 221 | 218 | 3 |
| 7 | 239 | 238 | 1 |
| 8 | 258 | 256 | 2 |

**Two frames, 80 ms**, half of before. QCView's log over the run: 258
publishes, 27 waits, 0 skipped.

## What the remaining two frames are

Not the ring (the sink polls at 1 ms and hands the slot straight to the
SDK, no pacing: `clock_video` is off). They are the NDI hop itself —
the SDK's encode and receive on loopback — and Studio Monitor's own
display path, which are the same on both platforms. Studio Monitor's Low
Latency option may take one back; an `ndi-recv` that prints the age of
each frame against the SDK's send timestamp would put a number on the
hop without a receiver's display in the way. Both are open.
