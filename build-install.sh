#!/bin/bash
# Build, sign, notarize, and install the AU plugin to Logic Pro's Components folder
set -e
cd "$(dirname "$0")"

COMPONENT="Guitar Vocoder.component"
INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/Components"

# --- Build ---
echo "Building..."
cmake --build build --config Release

# --- Locate built component ---
# Path varies by generator: Makefile puts it in AU/, Xcode puts it in Release/AU/
if [ -d "build/GuitarVocoder_artefacts/Release/AU/$COMPONENT" ]; then
    AU_PATH="build/GuitarVocoder_artefacts/Release/AU/$COMPONENT"
elif [ -d "build/GuitarVocoder_artefacts/AU/$COMPONENT" ]; then
    AU_PATH="build/GuitarVocoder_artefacts/AU/$COMPONENT"
else
    echo "Error: Could not find built component."
    exit 1
fi

# --- Stage in /tmp for clean signing (avoids iCloud/Finder extended attributes) ---
STAGING="/tmp/GuitarVocoder-sign"
rm -rf "$STAGING"
mkdir -p "$STAGING"
cp -R "$AU_PATH" "$STAGING/"
xattr -cr "$STAGING/$COMPONENT"
find "$STAGING" -name '._*' -delete 2>/dev/null || true
find "$STAGING" -name '.DS_Store' -delete 2>/dev/null || true

# --- Sign ---
echo "Signing..."
codesign --force --deep --options runtime \
  --sign "Developer ID Application" \
  "$STAGING/$COMPONENT"

# Verify signature
codesign --verify --deep --strict "$STAGING/$COMPONENT"
echo "✓ Signed and verified"

# --- Install locally ---
cp -r "$STAGING/$COMPONENT" "$INSTALL_DIR/"
echo "✓ Installed to Logic"

# --- Notarize (optional, pass --notarize flag) ---
if [[ "$1" == "--notarize" ]]; then
    echo "Notarizing (this takes 1-5 minutes)..."
    ZIP_PATH="/tmp/GuitarVocoder-notarize.zip"
    ditto -c -k --keepParent "$STAGING/$COMPONENT" "$ZIP_PATH"

    xcrun notarytool submit "$ZIP_PATH" \
      --keychain-profile "GuitarVocoder-Notarize" \
      --wait

    xcrun stapler staple "$INSTALL_DIR/$COMPONENT"
    echo "✓ Notarized and stapled"

    # Create distributable zip on Desktop
    DIST_ZIP="$HOME/Desktop/GuitarVocoder.zip"
    ditto -c -k --keepParent "$INSTALL_DIR/$COMPONENT" "$DIST_ZIP"
    echo "✓ Distributable zip: $DIST_ZIP"

    rm "$ZIP_PATH"
fi

echo "✓ Done"
