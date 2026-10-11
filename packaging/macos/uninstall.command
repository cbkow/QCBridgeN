#!/bin/bash
# Remove QCBridgeN and its registration with QCView. Double-click in Finder.
echo "This removes /Applications/QCBridgeN.app and its QCView registration."
pkill -x QCBridgeN 2>/dev/null || true
sudo rm -rf "/Applications/QCBridgeN.app"
sudo rm -f "/Library/Application Support/QCView/Outputs/ndi.json"
sudo pkgutil --forget com.qcview-app.QCBridgeN 2>/dev/null || true
echo "Done. QCView's settings for the output are left as they were."
