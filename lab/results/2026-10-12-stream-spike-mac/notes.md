# Stream output spike, macOS: VideoToolbox → MPEG-TS over SRT (2026-10-12, afternoon)

The first step of the stream-output plan (A0): QCView encodes its
corrected viewport in-process and sends it over SRT, no NDI, no OBS. On
the Mac, behind an environment flag, video only, measured against a local
listener. The Louper room measurement is the next run (it needs a room's
URL and key).

## What was built (QCView f33b3b74)

- A `qcv_output` library: a paced sink (clock at the source rate; the
  newest picture or the last one repeated; a bounded access-unit queue; a
  mux thread), a `VideoEncoder` face with the VideoToolbox H.264
  implementation (hardware session, Baseline or Main, no reordering,
  keyframe every second, data-rate limit 2x over 1 s, 709 tags, Annex-B
  with SPS/PPS in band on keyframes), and a libavformat muxer (mpegts over
  `srt://`, caller mode, latency/streamid/passphrase as protocol options,
  interruptible connect).
- The output pass gained a GPU-sink kind of target: steps 1–3 as for a
  ring target, then a compute kernel converts the wire texture to NV12
  (BT.709 limited range, 2x2 chroma average) straight into a CVPixelBuffer
  from the sink's pool, handed over from the command buffer's completion
  handler. No CPU copy. Ring targets (the NDI sink) are untouched.
- `QCV_STREAM_SPIKE=srt://host:port` starts it with the renderer; size,
  kbps, fps, latency, streamid, passphrase and profile via
  `QCV_STREAM_SPIKE_*`. No UI yet.

## Method

Listener on the same machine, recording what arrives:

    ffmpeg -i "srt://127.0.0.1:9000?mode=listener" -c copy -t 40 spike.ts

QCView launched with the flag, 1280x720 at 4000 kbps, the 25p burn-in
clip (`probe-51-720p25.mov`), playback started by `QCV_AUTOPLAY_SECONDS`.
The recording was then read with `ffprobe -show_frames` and one frame
decoded back to a PNG.

A second run streamed a paused frame while `QCV_CAPTURE_PNG` saved
QCView's own capture of the same frame; the stream frame was decoded with
an explicit 709 matrix (`scale=in_color_matrix=bt709:in_range=tv`) and
compared pixel for pixel.

## Results

Run 1, playing (QCView's once-a-second line and the listener):

| item | value |
|---|---|
| clock | locked to the source: 25 fps (bus-reported rate at the first frame) |
| received | 1000 frames in 40 s, pts delta 3600 ticks on every frame |
| encoder | `com.apple.videotoolbox.videoencoder.ave.avc` (hardware) |
| GOP | exactly 25 frames between I-frames, 40 I / 960 P, no B-frames |
| profile / tags | Baseline, level 3.1; VUI bt709 / bt709 / bt709, tv range |
| rate | 3.9–4.2 Mbps while playing, ~340 kbps repeating a paused frame |
| pacing | 1 "skipped" per ~3 s while playing (two source frames inside one clock period — independent clocks), 0 queue drops, 0–1 surface in flight |
| after the clip ended | the last frame repeated at 25/s |
| quit while streaming | `StreamSink: stopped`, no surface left in flight |

Run 2, paused frame vs QCView's capture (1280x720, 921,600 pixels):

| max channel difference | pixels |
|---|---|
| 0 | 30.7 % |
| ≤ 1 | 90.9 % |
| ≤ 2 | 94.1 % |
| > 16 | ~1 % |

Flat regions: the grey cross is exact, the red bar is within 1 code
value, green and blue bars mean 0.2–0.4. Everything above that sits on
the bar edges (4:2:0 chroma) and inside the noisy checkerboard patch at
4 Mbps. The conversion matrix is right.

A side note on measuring: decoding the *source* clip with ffmpeg to
compare against uses swscale's BT.601 default for an untagged file, and
makes the (correct) stream look desaturated (green 255 → 216). QCView
reads untagged HD as Rec.709, as does the stream; compare against
QCView's capture, or force the matrix on the decode.

## What is not in this spike

Audio (A1), a settings row and start/stop (A1), Windows / Media
Foundation (A2), RTMP and reconnect (A3). After the listener closed the
muxer logged a write error per frame and kept going; reconnect is A3's.
The latency number against a Louper viewer is the next measurement.

## Addendum: A1 on the Mac (same day, evening; QCView dd4f562d)

No Louper room exists yet (Chris: "build it blind and use ffplay for
testing"), so the Louper measurement waits. A1 added the product
plumbing and was measured against the same local listener.

What changed: AAC audio from the players' tap (1024-sample frames,
silence inserted while the tap is idle so the stream's audio never
stops), a Settings ▸ Output "Stream" block (Start/Stop, URL, key, video
bitrate, resolution, frame rate, profile, audio bitrate, SRT latency,
counters while running), QSettings-backed, sessions start and stop from
the panel and are torn down before the renderer on quit.

| check | result |
|---|---|
| audio continuity | 1407 AAC packets in 30 s, every pts delta 1920 ticks (1024 samples), none missing |
| A/V alignment, before | tone onset 3.351 s vs picture motion 3.40 s: audio one frame early |
| A/V alignment, after | audio stamped one video period later: 3.446 s vs 3.44 s, 6 ms apart |
| source switching | `--switch-test` 30 s (single/dual/clear/activate, random): clock steady at 25/s, no stream errors, 751 frames received |
| stop/start cycles | 16 sessions back to back at 3 s each, every one connected and stopped cleanly; RSS 268,224 KB at cycle 16 → 268,320 KB at cycle 19 |
| stop during connect | ends the connect at once ("Immediate exit requested"); the user's Stop clears the message |
| quit while streaming | clean, texture pools empty |

Why the audio offset: a new picture waits up to one clock period before
the paced thread encodes it; audio frames are cut from the tap as they
arrive. One period of audio delay puts the two within a few ms at 25p
(the tone's -40 dB onset vs the first frame whose luma differs from the
paused one).

Still to come: the Louper room run and its latency number (needs a
room), Windows / Media Foundation (A2), RTMP and reconnect (A3; after a
listener drops the muxer logs a write error per packet and keeps going).
