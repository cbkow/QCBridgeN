# QCBridgeN

QCView's output bus and its first sink: what QCView shows and plays, out
over NDI®. A separate process beside QCView that reads QCView's output
rings (shared memory) and sends them with the NDI SDK.

The plan is `PLAN.md`. What exists: `vendor/qcbae/` (the wire contract,
MIT, re-copied from QCBridgeAE), `tools/qcvo-probe`, which plays either
end of the bus so each real side can be built and measured alone,
`src/sink/` — the sink itself (built when the NDI SDK is installed; see
the end of `CMakeLists.txt`), and `tools/ndi-recv`, a receiver that
reports what arrives over NDI:

    cmake -S . -B build && cmake --build build
    build/qcvo-probe produce --id probe --fps 25 --seconds 10
    build/qcvo-probe consume --id probe --seconds 5 --dump last.ppm
    build/QCBridgeN --id ndi --name QCView          # the sink (needs the NDI runtime)
    build/ndi-recv --name QCView --seconds 10        # what a receiver sees

QCView starts the sink itself when the output is enabled in Settings ▸
Output, from a manifest in Application Support/QCView/Outputs (or the
directory `QCV_OUTPUT_MANIFEST_DIR` names, for development):

    { "id": "ndi", "name": "QCBridgeN (NDI)", "protocol": 1,
      "exec": "/Applications/QCBridgeN.app/Contents/MacOS/QCBridgeN",
      "args": ["--id", "ndi", "--name", "QCView"] }

NDI® is a registered trademark of Vizrt NDI AB. QCBridgeN is compatible
with NDI products; it is not an NDI product. The NDI SDK is not part of
this repository; the build locates it where its installer put it.
