#!/bin/zsh
# build-pkg.sh — the signed macOS installer for QCBridgeN.
#
#   dist/QCBridgeN-<version>-<arch>.pkg
#     └─ QCBridgeN-app.pkg (component, non-relocatable, pre/postinstall)
#          └─ /Applications/QCBridgeN.app
#
# Input: build/QCBridgeN.app (cmake --build build). Everything happens on a
# COPY: macdeployqt adds the Qt frameworks and plugins, the NDI runtime is
# already in Contents/Frameworks from the build, then the bundle is signed
# inside-out with the Developer ID Application identity (hardened runtime,
# timestamp) and the pkg with the Developer ID Installer identity. Same
# shape as QCBridgeAE's and QCView's. Unsigned when no identity is in the
# keychain (local testing only).
#
#   packaging/macos/build-pkg.sh [--notarize]
#
# --notarize submits the signed app to Apple first and staples it (the copy
# that lands in /Applications must carry its own ticket — QCView's release
# notes record the release that went out without one), then builds the pkg
# from the stapled app and notarizes and staples the pkg too. Two
# submissions, 2–15 minutes each. Needs the "QCView" notarytool keychain
# profile (QCBN_NOTARY_PROFILE overrides).
set -euo pipefail
NOTARIZE=0
[ "${1:-}" = "--notarize" ] && NOTARIZE=1
PROFILE="${QCBN_NOTARY_PROFILE:-QCView}"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$REPO/packaging/macos"
BUILT_APP="${QCBN_APP:-$REPO/build/QCBridgeN.app}"
QT_PREFIX="${QT_PREFIX:-$HOME/Qt/6.11.1/macos}"
PKG_ID="com.qcview-app.QCBridgeN"
[ -d "$BUILT_APP" ] || { echo "ERROR: missing $BUILT_APP — cmake --build build first" >&2; exit 1; }
[ -x "$QT_PREFIX/bin/macdeployqt" ] || { echo "ERROR: no macdeployqt at $QT_PREFIX (set QT_PREFIX)" >&2; exit 1; }
VERSION="$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$BUILT_APP/Contents/Info.plist")"
ARCH="$(uname -m)"
mkdir -p "$REPO/dist"
PKG="$REPO/dist/QCBridgeN-$VERSION-$ARCH.pkg"

STAGE="$(mktemp -d -t qcbn-pkg)"
trap 'rm -rf "$STAGE"' EXIT
ROOT="$STAGE/root/Applications"
APP="$ROOT/QCBridgeN.app"
mkdir -p "$ROOT" "$STAGE/pkgs" "$STAGE/scripts" "$STAGE/resources"
echo "[pkg] staging payload (v$VERSION, $ARCH)"
ditto "$BUILT_APP" "$APP"
[ -f "$APP/Contents/Frameworks/libndi.dylib" ] \
    || { echo "ERROR: the NDI runtime is not in the bundle (build with the SDK installed)" >&2; exit 1; }

echo "[pkg] macdeployqt"
"$QT_PREFIX/bin/macdeployqt" "$APP" -verbose=1 > "$STAGE/macdeployqt.log" 2>&1 \
    || { echo "macdeployqt failed — $STAGE/macdeployqt.log" >&2; tail -20 "$STAGE/macdeployqt.log" >&2; exit 1; }
# macdeployqt deploys by what Qt's plugins could want, not by what this app
# links: a menu-bar app came out at 44 MB with Qt Quick and the virtual
# keyboard inside. Keep only the frameworks the main binary and the kept
# plugins reach through their load commands, closed over transitively.
echo "[pkg] prune"
rm -rf "$APP/Contents/PlugIns/sqldrivers" "$APP/Contents/PlugIns/networkinformation" \
       "$APP/Contents/PlugIns/tls" "$APP/Contents/PlugIns/generic" "$APP/Contents/PlugIns/virtualkeyboard" \
       "$APP/Contents/PlugIns/qmltooling" "$APP/Contents/PlugIns/iconengines" "$APP/Contents/PlugIns/platforminputcontexts" "$APP/Contents/Resources/qml" 2>/dev/null || true
# Only the PNG image plugin is ever used (the icons); drop the rest.
find "$APP/Contents/PlugIns/imageformats" -name "*.dylib" ! -name "libqpng*" -delete 2>/dev/null || true
needed=()
queue=("$APP/Contents/MacOS/QCBridgeN")
while IFS= read -r -d '' f; do queue+=("$f"); done < <(find "$APP/Contents/PlugIns" -name "*.dylib" -print0 2>/dev/null)
seen=""
while [ ${#queue[@]} -gt 0 ]; do
    bin="${queue[1]}"; queue=("${queue[@]:1}")
    for fw in $(otool -L "$bin" 2>/dev/null | grep -o '@rpath/Qt[A-Za-z0-9]*\.framework' | sed 's|@rpath/||' | sort -u); do
        case " $seen " in *" $fw "*) continue ;; esac
        seen="$seen $fw"
        needed+=("$fw")
        inner="$APP/Contents/Frameworks/$fw/Versions/A/${fw%.framework}"
        [ -f "$inner" ] && queue+=("$inner")
    done
done
for d in "$APP"/Contents/Frameworks/Qt*.framework; do
    fw="$(basename "$d")"
    case " $seen " in *" $fw "*) ;; *) rm -rf "$d" ;; esac
done
echo "[pkg] frameworks kept:${seen}"
# Smoke test the staged bundle before signing: it must start and find its
# runtime with nothing but what is inside the app.
"$APP/Contents/MacOS/QCBridgeN" --headless --seconds 1 > "$STAGE/smoke.log" 2>&1 \
    || { echo "ERROR: the staged app does not run after pruning — $STAGE/smoke.log" >&2; cat "$STAGE/smoke.log" >&2; exit 1; }
grep -q "NDI runtime" "$STAGE/smoke.log" || { echo "ERROR: the staged app did not load the NDI runtime" >&2; cat "$STAGE/smoke.log" >&2; exit 1; }
echo "[pkg] staged app runs: $(head -1 "$STAGE/smoke.log")"

identity="$(security find-identity -v -p codesigning 2>/dev/null | grep -o '"Developer ID Application: [^"]*"' | head -1 | tr -d '"')"
if [ -n "$identity" ]; then
    echo "[pkg] codesign inside-out with: $identity"
    sign() { codesign --force --options runtime --timestamp --sign "$identity" "$@"; }
    find "$APP/Contents/Frameworks" -name "*.dylib" -print0 2>/dev/null \
        | while IFS= read -r -d '' f; do sign "$f"; done
    find "$APP/Contents/Frameworks" -maxdepth 1 -name "Qt*.framework" -print0 2>/dev/null \
        | while IFS= read -r -d '' f; do sign "$f"; done
    find "$APP/Contents/PlugIns" -type f -name "*.dylib" -print0 2>/dev/null \
        | while IFS= read -r -d '' f; do sign "$f"; done
    sign --entitlements "$SRC/entitlements.plist" "$APP"
    codesign --verify --deep --strict "$APP"
    if [ "$NOTARIZE" = 1 ]; then
        xcrun notarytool history --keychain-profile "$PROFILE" >/dev/null 2>&1 \
            || { echo "ERROR: notarytool cannot use profile \"$PROFILE\"" >&2; exit 1; }
        echo "[pkg] notarizing the app (profile $PROFILE)"
        ditto -c -k --keepParent "$APP" "$STAGE/app.zip"
        xcrun notarytool submit "$STAGE/app.zip" --keychain-profile "$PROFILE" --wait --output-format json > "$STAGE/app-notary.json"
        app_status="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("status",""))' "$STAGE/app-notary.json")"
        app_id="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id",""))' "$STAGE/app-notary.json")"
        if [ "$app_status" != "Accepted" ]; then
            echo "ERROR: app notarization $app_status (id $app_id):" >&2
            xcrun notarytool log "$app_id" --keychain-profile "$PROFILE" >&2 || true
            exit 1
        fi
        xcrun stapler staple "$APP"
        xcrun stapler validate "$APP"
        echo "[pkg] app notarized and stapled (id $app_id)"
    fi
else
    [ "$NOTARIZE" = 1 ] && { echo "ERROR: --notarize needs a Developer ID Application identity" >&2; exit 1; }
    echo "WARN: no Developer ID Application identity — UNSIGNED app (local testing only)" >&2
fi

cp "$SRC/scripts/preinstall"  "$STAGE/scripts/preinstall"
cp "$SRC/scripts/postinstall" "$STAGE/scripts/postinstall"
chmod 755 "$STAGE/scripts/preinstall" "$STAGE/scripts/postinstall"
cp "$SRC/welcome.txt" "$STAGE/resources/welcome.txt"
cp "$REPO/LICENSE" "$STAGE/resources/LICENSE"
pkgbuild --analyze --root "$STAGE/root" "$STAGE/components.plist" >/dev/null
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$STAGE/components.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$STAGE/components.plist" 2>/dev/null \
        || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$STAGE/components.plist"
    i=$((i + 1))
done
pkgbuild --root "$STAGE/root" \
         --component-plist "$STAGE/components.plist" \
         --identifier "$PKG_ID" \
         --version "$VERSION" \
         --install-location / \
         --scripts "$STAGE/scripts" \
         "$STAGE/pkgs/QCBridgeN-app.pkg" >/dev/null
sed "s/@VERSION@/$VERSION/g" "$SRC/distribution.xml" > "$STAGE/distribution.xml"
rm -f "$PKG"
installer_id="$(security find-identity -v 2>/dev/null | grep -o '"Developer ID Installer: [^"]*"' | head -1 | tr -d '"')"
if [ -n "$installer_id" ]; then
    productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                 --resources "$STAGE/resources" --sign "$installer_id" --timestamp "$PKG" >/dev/null
    pkgutil --check-signature "$PKG" | sed 's/^/  /' | head -3
else
    echo "WARN: no Developer ID Installer identity — UNSIGNED pkg (local testing only)" >&2
    productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                 --resources "$STAGE/resources" "$PKG" >/dev/null
fi
if [ "$NOTARIZE" = 1 ]; then
    [ -n "$installer_id" ] || { echo "ERROR: --notarize needs a Developer ID Installer identity" >&2; exit 1; }
    echo "[pkg] notarizing the pkg"
    xcrun notarytool submit "$PKG" --keychain-profile "$PROFILE" --wait --output-format json > "$STAGE/pkg-notary.json"
    pkg_status="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("status",""))' "$STAGE/pkg-notary.json")"
    pkg_id="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id",""))' "$STAGE/pkg-notary.json")"
    if [ "$pkg_status" != "Accepted" ]; then
        echo "ERROR: pkg notarization $pkg_status (id $pkg_id):" >&2
        xcrun notarytool log "$pkg_id" --keychain-profile "$PROFILE" >&2 || true
        exit 1
    fi
    xcrun stapler staple "$PKG"
    xcrun stapler validate "$PKG"
    echo "[pkg] pkg notarized and stapled (id $pkg_id)"
fi
echo "[pkg] done: $PKG ($(du -sh "$PKG" | cut -f1))"
shasum -a 256 "$PKG"
