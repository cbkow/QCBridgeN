# Releasing QCBridgeN (macOS)

The same shape as QCView's and QCBridgeAE's flows: everything is a script
in `packaging/`, nothing secret is in the repository, the credentials are
in the login keychain.

## What has to be on the machine

| Thing | Check it | If it's missing |
|---|---|---|
| Developer ID Application + Installer certs | `security find-identity -v` shows both | Apple Developer account → Certificates |
| notarytool profile `QCView` | `xcrun notarytool history --keychain-profile QCView` | `xcrun notarytool store-credentials "QCView" --apple-id <id> --team-id 5Z4S9VHV56` |
| codesign unprompted | signing runs without dialogs | "Allow all applications" on the private key in Keychain Access |
| Qt 6.11.1 | `ls ~/Qt/6.11.1/macos/bin/macdeployqt` | Qt online installer |
| NDI SDK for Apple | `ls "/Library/NDI SDK for Apple/include/Processing.NDI.Lib.h"` | ndi.video; the SDK is per machine and never enters the repository |

The app ships at the **macOS 13.0** floor (`LSMinimumSystemVersion`).

## The flow

```bash
# 1. version: CMakeLists.txt  project(QCBridgeN VERSION x.y.z)
#    (Info.plist takes CFBundleShortVersionString from it)

# 2. build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/macos" \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
cmake --build build -j"$(sysctl -n hw.ncpu)"

# 3. the check that matters: QCView (2.5.4+) with this build registered,
#    Settings ▸ Output on, a receiver on the network showing the viewport
#    with audio; the sink quits with QCView. tools/ndi-recv is the receiver
#    that prints what it got.

# 4. installer: macdeployqt, prune, smoke test, sign, notarize + staple the
#    app, pkg, notarize + staple the pkg (two submissions)
packaging/macos/build-pkg.sh --notarize
# → dist/QCBridgeN-<version>-arm64.pkg, and its SHA-256 on the last line

# 5. tag + GitHub release v<version>, attach the pkg; the README links the
#    latest release
git tag -a v<version> -m "QCBridgeN <version>" && git push origin main v<version>
```

`packaging/macos/notarize.sh <file>` notarizes and staples one file on its
own, for a pkg built without `--notarize`.

## Things worth knowing

- **The app carries its own ticket.** The pkg's ticket covers the install;
  the copy in /Applications is what Gatekeeper assesses when QCView starts
  it, so the app is notarized and stapled before it goes into the pkg.
- **macdeployqt is generous.** The platform input-context plugin links the
  virtual keyboard, which links Qt Quick: 44 MB. `build-pkg.sh` prunes to
  what the binary and the kept plugins link, transitively, then runs the
  staged app headless to prove it still starts and finds its NDI runtime.
- **Resources are not compiled by accident.** `qt_standard_project_setup`
  turns on AUTOMOC and AUTOUIC, not AUTORCC: a `.qrc` listed among the
  sources is silently ignored, the icons come back null, and the menu-bar
  item is an invisible blank — which is what the first installed 0.0.1
  did. The icons go through `qt_add_resources`, and the app warns at
  start if the tray icon resource is missing.
- **Frameworks holds code only.** The NDI licence text lives in Resources;
  codesign refuses a plain file under Frameworks.
- **The NDI runtime inside the app** is the SDK's `libndi.dylib`, allowed
  by the SDK documentation ("you can include the NDI runtime within your
  application folder") and re-signed with our identity. An installed NDI
  runtime (`/usr/local/lib`) is preferred when present, so a user's newer
  runtime wins.

## Checking a build after the fact

```bash
codesign --verify --deep --strict --verbose=2 /Applications/QCBridgeN.app
xcrun stapler validate /Applications/QCBridgeN.app
xcrun stapler validate dist/QCBridgeN-<version>-arm64.pkg
spctl --assess --type install -vv dist/QCBridgeN-<version>-arm64.pkg
cat "/Library/Application Support/QCView/Outputs/ndi.json"
```

## Windows

On the Windows box, from a Release build (`cmake -S . -B build-release -G
Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\msvc2022_64
"-DNDI_SDK_DIR=C:\Program Files\NDI\NDI 6 SDK"` under vcvars64):

    packaging\windows\build-setup.cmd
    → dist\QCBridgeN-<version>-Setup-x64.exe

It stages the exe with `windeployqt` (Core, Gui, Widgets, platforms,
styles only), the NDI runtime DLL and its licence file from the SDK's
`Bin\x64`, and `vc_redist.x64.exe`, then runs Inno Setup 6 on
`qcbridgen.iss`. The installer writes the registration manifest to
`%ProgramData%\QCView\Outputs\ndi.json` and removes it on uninstall. The
Setup is not code-signed; signing is a certificate away (`signtool` on the
staged exe and the Setup), the way QCView's sideload MSIX is signed.
