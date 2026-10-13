# Stream output, A3: RTMP / FLV and reconnect (2026-10-13)

Plan step A3 hardens the stream output: a second wire format (FLV over
RTMP or RTMPS, the ingest most services besides Louper's SRT offer) and
a session that survives a dropped connection. Measured on the Mac against
ffmpeg acting as the server.

## What was built (QCView, the A3 commit)

- **FLV over RTMP(S).** `StreamMuxer` chooses `flv` for `rtmp://` and
  `rtmps://`, `mpegts` for `srt://`. FLV wants the H.264 parameter sets in
  its header: given up front (a reconnect has them from the last keyframe)
  they go in on open; otherwise the connect happens at once and the header
  is written from the SPS/PPS inside the first keyframe, with the units
  before it dropped. libavformat's flv muxer converts the Annex-B units to
  length-prefixed ones itself when the extradata is not avcC, so no
  packetiser of our own. The stream key becomes the RTMP stream name
  (`rtmp://host/app/<key>`); for SRT it stays the stream id.
- **Reconnect with backoff.** The first connect and every reconnect retry
  at 1, 2, 4, 8 and then 10 s until the server answers or the user stops.
  A failed write is taken as a lost connection: the muxer closes, the
  queue is cleared, the paced thread keys the next frame, and the mux
  thread drops everything until that keyframe so the receiver starts
  clean. The encoder never stops; the clock keeps its pts.
- **Counters.** `reconnects` in the sink's status, the Output row and the
  once-a-second log line.
- **Docs.** `docs/output.md`: the Output section, NDI, every stream
  setting, the Louper recipe, what happens on a drop.

## Method

RTMP: `ffmpeg -listen 1 -i rtmp://127.0.0.1:1935/live/test -c copy -t 24
out.flv` as the server; QCView streams the 25p burn-in clip through
`QCV_STREAM_SPIKE` at 1280x720 / 6000 kbps for 24 s. SRT reconnect: a
listener on port 9001 records for 8 s and exits; six seconds later a new
listener records for 10 s; QCView streams for 32 s across the gap.

## Results

RTMP / FLV:

| item | value |
|---|---|
| header | "connected; the FLV header waits for the first keyframe" → written 17 ms later from 26 bytes of SPS/PPS |
| streams | h264 Constrained Baseline 1280x720 25/1 + aac LC |
| timing | 600 frames, pts delta 40 ms on every frame (FLV's millisecond clock) |
| GOP | 25 frames between keyframes, 24 keyframes |
| rate | 5.8–6.1 Mbps on the log line; 16.9 MB in 24 s |
| when the server closed | "send failed (Broken pipe) — reconnecting", then a clean stop on quit |

SRT reconnect:

| time | event |
|---|---|
| 0 s | streaming; listener 1 records 200 frames (8 s) |
| 8.4 s | listener 1 ends: "send failed (write audio: Input/output error) — reconnecting (1)" |
| 9.5 s | reconnect attempt: refused (no listener yet) — "retrying in 1 s" |
| 15.7 s | attempt 2: connected, "reconnected (1)", streaming resumes at a keyframe |
| 15.7–25.7 s | listener 2 records 248 frames (10 s) |
| 25.7 s | listener 2 ends: "reconnecting (2)" until quit |

427 units were dropped from the queue during the first gap, by design
(they would arrive late and before the keyframe).

## Known

- The spike aid starts the session before the media is open, so the clock
  takes the 30 fps default; the Output section's Start, after loading
  media, reads the source rate (as on the Windows A2 run).
- RTMPS was not exercised (no TLS ingest here); the build has `tls` and
  `rtmps`, and the code path is the same.
- Not built: a software (libx264) fallback. Both target machines have a
  hardware encoder; the Stream block says "no hardware encoder" where
  there is none.
