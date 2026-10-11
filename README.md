# QCBridgeN

QCView's output bus and its first sink: what QCView shows and plays, out
over NDI®. A separate process beside QCView that reads QCView's output
rings (shared memory) and sends them with the NDI SDK.

The plan is `PLAN.md`. What exists: `vendor/qcbae/` (the wire contract,
MIT, re-copied from QCBridgeAE) and `tools/qcvo-probe`, which plays
either end of the bus so each real side can be built and measured alone:

    cmake -S . -B build && cmake --build build
    build/qcvo-probe produce --id probe --fps 25 --seconds 10
    build/qcvo-probe consume --id probe --seconds 5 --dump last.ppm

NDI® is a registered trademark of Vizrt NDI AB. QCBridgeN is compatible
with NDI products; it is not an NDI product. The NDI SDK is not part of
this repository; the build locates it where its installer put it.
