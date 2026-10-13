# Stream output on Windows: Media Foundation → MPEG-TS over SRT (2026-10-13)

Plan step A2: the Windows twin of the Mac stream output. QCView's D3D11
renderer converts the wire picture to NV12 on the GPU and the vendor's
hardware H.264 encoder behind Media Foundation encodes it on the same
device; the paced sink, AAC and the SRT muxer are the shared code from
A0/A1. Measured on the Windows box (RTX 5090) against a local listener,
driven from the Mac over SSH.

## What was built (QCView 71666829, 44be3223, 2a20437d)

- `MfVideoEncoder` (src/output/mf_video_encoder.cpp): `MFTEnumEx` for a
  hardware H.264 encoder, `MF_TRANSFORM_ASYNC_UNLOCK` + `MF_LOW_LATENCY`,
  `IMFDXGIDeviceManager` over the renderer's device, ICodecAPI CBR / mean
  bit rate / GOP / low latency, output type H.264 Baseline or Main with
  709 primaries, transfer and matrix and limited range, input type NV12.
  The asynchronous MFT is driven by its NeedInput / HaveOutput events on
  a Media Foundation work queue (`IMFAsyncCallback`); input samples wrap
  the NV12 textures (`MFCreateDXGISurfaceBuffer`); frames are FIFO (no
  B-frames), so each output carries the exact 90 kHz pts of its input.
  Output is Annex-B already; SPS/PPS are cached and prepended on a
  keyframe that lacks them.
- Surfaces: a pool of up to 8 NV12 `ID3D11Texture2D`
  (RENDER_TARGET | SHADER_RESOURCE), sized at prepare, created on first
  acquire on the render thread (the bus can start a session before the
  device exists), reference-counted through the sink's
  acquire / retain / release.
- D3D11 output pass: a sink target takes its surface before the passes;
  the wire texture gains an SRV; two fullscreen pixel passes write the Y
  plane (an R8 view of the NV12 texture) and the CbCr plane (an R8G8 view
  at half size, 2x2 average) with the Metal kernel's maths; then Flush
  and submit. Ring targets (the NDI sink) are untouched.
- The device is created with `D3D11_CREATE_DEVICE_VIDEO_SUPPORT` (retry
  without, no stream encoder then) and multithread protection from the
  start, not only once the D3D11VA bridge initialises.

## Method

On the box: an ffmpeg listener records 30 s
(`srt://127.0.0.1:9000?mode=listener`), QCView (dev build, Ninja Release,
VS 18) plays the 25p burn-in clip from `C:\qcb-lab\media`, the stream
started by `QCV_STREAM_SPIKE` at 1280x720 / 6000 kbps, launched on the
desktop through the scheduled-task runner with its settings and log in
a scratch folder. The recording is copied to the Mac and read with
ffprobe; one frame is decoded with an explicit 709 matrix and compared
pixel for pixel against the source clip decoded the same way (static
layout, so the bars match and the moving parts do not).

## Results

| item | value |
|---|---|
| encoder | `NVIDIA H.264 Encoder MFT` through Media Foundation |
| profile / tags | Constrained Baseline, level 3.1; VUI bt709 / bt709 / bt709, tv range |
| timing | 815 frames in 27 s, pts delta 3000 on every frame (the 30 fps clock, see below) |
| GOP | exactly 30 frames between I-frames, 28 I / 787 P, no B |
| rate | 5827 kbps over the moving section (ffprobe), 6052 kbps on QCView's line; I-frames ~49 KB |
| picture vs source | 84.9 % of pixels within 1 code value, 86.6 % within 2; the rest is the moving bar, the checkerboard and the burn-in; red bar (255,24,0) → (254,24,0), cyan (0,230,255) → (1,230,255) |
| encoder counters at stop | 822 submitted, 822 encoded, 0 errors; `StreamSink: stopped` |
| mux | 0 errors, 0 queue drops, 0 surfaces in flight |

Two runs before this one were instructive. The first refused to start:
the bus opened the session before the renderer had created the D3D11
device, and the surface step wanted the device — the textures are now
created on first acquire. The second streamed black at ~190 kbps: the
NV12 pass bound the wire texture as a shader resource while it was still
step 3's render target, which D3D11 refuses silently (the slot stays
null, Y reads 16). Unbinding the render targets first fixed it.

`CODECAPI_AVEncMPVDefaultBPictureCount` is rejected by the NVIDIA MFT
(`E_INVALIDARG`); the stream has no B-frames anyway (Baseline, and the
MFT's low-latency mode).

## Known

- The clock ran at 30 fps for a 25p source: `QCV_STREAM_SPIKE` starts the
  session at renderer-ready, before the media is open, and the sink reads
  the source rate once, when its clock starts. Started from the Output
  section after loading media (the product path) the rate is known. The
  same holds on the Mac; the A0 run happened to see the rate first.
- Not measured here: a Louper room from the box (no room yet), the NDI
  sink and the stream at the same time, HDR swapchain modes.
