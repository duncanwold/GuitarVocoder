#!/bin/bash
# Build, sign, notarize, and install Guitar Vocoder (AU + VST3)
set -e
cd "$(dirname "$0")"

AU_NAME="Guitar Vocoder.component"
VST3_NAME="Guitar Vocoder.vst3"
AU_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

# --- Build ---
echo "Building..."
cmake --build build --config Release

# --- Locate built plugins ---
# Path varies by generator: Makefile puts it in AU/, Xcode puts it in Release/AU/
if [ -d "build/GuitarVocoder_artefacts/Release/AU/$AU_NAME" ]; then
    AU_PATH="build/GuitarVocoder_artefacts/Release/AU/$AU_NAME"
    VST3_PATH="build/GuitarVocoder_artefacts/Release/VST3/$VST3_NAME"
elif [ -d "build/GuitarVocoder_artefacts/AU/$AU_NAME" ]; then
    AU_PATH="build/GuitarVocoder_artefacts/AU/$AU_NAME"
    VST3_PATH="build/GuitarVocoder_artefacts/VST3/$VST3_NAME"
else
    echo "Error: Could not find built plugins."
    exit 1
fi

# --- Stage in /tmp for clean signing (avoids iCloud/Finder extended attributes) ---
STAGING="/tmp/GuitarVocoder-sign"
rm -rf "$STAGING"
mkdir -p "$STAGING"

cp -R "$AU_PATH" "$STAGING/"
cp -R "$VST3_PATH" "$STAGING/"
xattr -cr "$STAGING/$AU_NAME" "$STAGING/$VST3_NAME"
find "$STAGING" -name '._*' -delete 2>/dev/null || true
find "$STAGING" -name '.DS_Store' -delete 2>/dev/null || true

# --- Sign both ---
echo "Signing AU..."
codesign --force --deep --options runtime \
  --sign "Developer ID Application" \
  "$STAGING/$AU_NAME"
codesign --verify --deep --strict "$STAGING/$AU_NAME"
echo "✓ AU signed"

echo "Signing VST3..."
codesign --force --deep --options runtime \
  --sign "Developer ID Application" \
  "$STAGING/$VST3_NAME"
codesign --verify --deep --strict "$STAGING/$VST3_NAME"
echo "✓ VST3 signed"

# --- Install locally ---
cp -r "$STAGING/$AU_NAME" "$AU_INSTALL_DIR/"
mkdir -p "$VST3_INSTALL_DIR"
cp -r "$STAGING/$VST3_NAME" "$VST3_INSTALL_DIR/"
echo "✓ Installed AU to Logic"
echo "✓ Installed VST3 to $VST3_INSTALL_DIR"

# --- Notarize (optional, pass --notarize flag) ---
if [[ "$1" == "--notarize" ]]; then
    echo "Notarizing (this takes 1-5 minutes)..."

    # Submit AU
    AU_ZIP="/tmp/GuitarVocoder-au-notarize.zip"
    ditto -c -k --keepParent "$STAGING/$AU_NAME" "$AU_ZIP"
    echo "Notarizing AU..."
    xcrun notarytool submit "$AU_ZIP" \
      --keychain-profile "GuitarVocoder-Notarize" \
      --wait
    xcrun stapler staple "$AU_INSTALL_DIR/$AU_NAME"
    echo "✓ AU notarized"

    # Submit VST3
    VST3_ZIP="/tmp/GuitarVocoder-vst3-notarize.zip"
    ditto -c -k --keepParent "$STAGING/$VST3_NAME" "$VST3_ZIP"
    echo "Notarizing VST3..."
    xcrun notarytool submit "$VST3_ZIP" \
      --keychain-profile "GuitarVocoder-Notarize" \
      --wait
    xcrun stapler staple "$VST3_INSTALL_DIR/$VST3_NAME"
    echo "✓ VST3 notarized"

    # Create distributable zips on Desktop
    AU_DIST="$HOME/Desktop/GuitarVocoder-AU.zip"
    VST3_DIST="$HOME/Desktop/GuitarVocoder-VST3.zip"
    ditto -c -k --keepParent "$AU_INSTALL_DIR/$AU_NAME" "$AU_DIST"
    ditto -c -k --keepParent "$VST3_INSTALL_DIR/$VST3_NAME" "$VST3_DIST"
    echo "✓ Distributable AU: $AU_DIST"
    echo "✓ Distributable VST3: $VST3_DIST"

    rm "$AU_ZIP" "$VST3_ZIP"
fi

echo "✓ Done"
