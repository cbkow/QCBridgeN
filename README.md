# QCBridgeN

What [QCView](https://qcview.app) shows and plays, out to the network as an
NDI® source.

QCBridgeN is a small menu-bar application that runs beside QCView. Switch it
on in QCView's **Settings ▸ Output** and the viewport — single or dual view,
splits, wipes and difference, the OCIO or minColor display transform,
safety guides and note strokes — plus the mixed audio go out as an NDI
source named **QCView**, to any NDI receiver on the network. Nothing is
sent until the switch is on; switch it off, or quit QCView, and the sink
stops.

## Install (macOS)

1. Download the latest `QCBridgeN-<version>-arm64.pkg` from
   [Releases](../../releases/latest) and run it. It installs
   `/Applications/QCBridgeN.app` and registers it with QCView.
2. In QCView 2.5.4 or later, open **Settings ▸ Output** and switch
   **QCBridgeN (NDI)** on. QCView starts QCBridgeN; its mark appears in the
   menu bar, lit while frames flow.
3. On a receiver (NDI Video Monitor, a switcher, OBS…), pick the source
   **QCView** under this machine's name.

The menu-bar item has **Settings…** for the source name, the raster
(1080p by default, up to 3840x2160), the picture (the viewport, side A or
side B of a dual view) and which layers to include. The feed mute and the
layers QCView allows are in QCView's own Output settings.

The NDI runtime is inside the application; an installed NDI runtime is used
instead when present. Requires macOS 13 or later. Windows comes with a later
release.

## How it works

QCView publishes its corrected canvas and its mixed audio into shared
memory, one ring pair per enabled output, and starts the sink registered
for that output. The sink asks for a raster and format, keeps a heartbeat,
takes each published frame straight from shared memory and hands it to the
NDI SDK, and reports the receiver count back to QCView's Settings. The
design, decisions and measurements are in `PLAN.md` and `lab/results/`.

The sink is a separate process on purpose: QCView is GPL and the NDI SDK's
licence does not allow linking the two into one program. The wire contract
between them (`vendor/qcbae/`) is MIT, copied from
[QCBridgeAE](https://github.com/cbkow/QCBridgeAE).

## Building

Needs CMake 3.24+, a C++20 compiler, Qt 6.11 and the NDI SDK installed
(`/Library/NDI SDK for Apple`, or `-DNDI_SDK_DIR=`). The SDK's headers are
confidential and are never in this repository; without the SDK only the
probe builds.

    cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/macos"
    cmake --build build
    build/qcvo-probe produce --id probe --fps 25 --seconds 10          # fakes QCView
    build/qcvo-probe consume --id probe --seconds 5 --dump last.ppm   # fakes a sink
    build/QCBridgeN.app/Contents/MacOS/QCBridgeN --id ndi --name QCView
    build/ndi-recv --name QCView --seconds 10                         # what a receiver sees

`RELEASE.md` is the release flow. For development QCView reads manifests
from the directory `QCV_OUTPUT_MANIFEST_DIR` names as well as from
Application Support/QCView/Outputs:

    { "id": "ndi", "name": "QCBridgeN (NDI)", "protocol": 1,
      "exec": "/Applications/QCBridgeN.app/Contents/MacOS/QCBridgeN",
      "args": ["--id", "ndi"] }

## Privacy

QCBridgeN sends video and audio to NDI receivers on the local network and
answers NDI discovery; it makes no other network connection and collects
nothing.

## Licence

MIT (`LICENSE`). NDI® is a registered trademark of Vizrt NDI AB. QCBridgeN
is compatible with NDI products; it is not an NDI product, and the NDI SDK
is not part of this repository.
