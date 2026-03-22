#!/bin/bash
# Build, sign, notarize, and install Guitar Vocoder (AU + VST3 + AAX)
set -e
cd "$(dirname "$0")"

AU_NAME="Guitar Vocoder.component"
VST3_NAME="Guitar Vocoder.vst3"
AAX_NAME="Guitar Vocoder.aaxplugin"
AU_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AAX_INSTALL_DIR="/Library/Application Support/Avid/Audio/Plug-Ins"

# --- Build ---
echo "Building..."
cmake --build build --config Release

# --- Locate built plugins ---
# Path varies by generator: Makefile puts it in AU/, Xcode puts it in Release/AU/
if [ -d "build/GuitarVocoder_artefacts/Release/AU/$AU_NAME" ]; then
    BASE="build/GuitarVocoder_artefacts/Release"
elif [ -d "build/GuitarVocoder_artefacts/AU/$AU_NAME" ]; then
    BASE="build/GuitarVocoder_artefacts"
else
    echo "Error: Could not find built plugins."
    exit 1
fi

AU_PATH="$BASE/AU/$AU_NAME"
VST3_PATH="$BASE/VST3/$VST3_NAME"

# AAX may not be built if SDK is not present
AAX_BUILT=false
if [ -d "$BASE/AAX/$AAX_NAME" ]; then
    AAX_PATH="$BASE/AAX/$AAX_NAME"
    AAX_BUILT=true
fi

# --- Stage in /tmp for clean signing (avoids iCloud/Finder extended attributes) ---
STAGING="/tmp/GuitarVocoder-sign"
rm -rf "$STAGING"
mkdir -p "$STAGING"

cp -R "$AU_PATH" "$STAGING/"
cp -R "$VST3_PATH" "$STAGING/"
if $AAX_BUILT; then
    cp -R "$AAX_PATH" "$STAGING/"
fi

xattr -cr "$STAGING/"*
find "$STAGING" -name '._*' -delete 2>/dev/null || true
find "$STAGING" -name '.DS_Store' -delete 2>/dev/null || true

# --- Sign ---
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

if $AAX_BUILT; then
    echo "Signing AAX..."
    codesign --force --deep --options runtime \
      --sign "Developer ID Application" \
      "$STAGING/$AAX_NAME"
    codesign --verify --deep --strict "$STAGING/$AAX_NAME"
    echo "✓ AAX signed (Apple codesign only — PACE signing required for distribution)"
fi

# --- Install locally ---
cp -r "$STAGING/$AU_NAME" "$AU_INSTALL_DIR/"
echo "✓ Installed AU to $AU_INSTALL_DIR"

mkdir -p "$VST3_INSTALL_DIR"
cp -r "$STAGING/$VST3_NAME" "$VST3_INSTALL_DIR/"
echo "✓ Installed VST3 to $VST3_INSTALL_DIR"

if $AAX_BUILT; then
    sudo mkdir -p "$AAX_INSTALL_DIR"
    sudo cp -r "$STAGING/$AAX_NAME" "$AAX_INSTALL_DIR/"
    echo "✓ Installed AAX to $AAX_INSTALL_DIR"
fi

# --- Notarize (optional, pass --notarize flag) ---
if [[ "$1" == "--notarize" ]]; then
    echo "Notarizing (this takes 1-5 minutes per format)..."

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

    # Submit AAX
    if $AAX_BUILT; then
        AAX_ZIP="/tmp/GuitarVocoder-aax-notarize.zip"
        ditto -c -k --keepParent "$STAGING/$AAX_NAME" "$AAX_ZIP"
        echo "Notarizing AAX..."
        xcrun notarytool submit "$AAX_ZIP" \
          --keychain-profile "GuitarVocoder-Notarize" \
          --wait
        sudo xcrun stapler staple "$AAX_INSTALL_DIR/$AAX_NAME"
        echo "✓ AAX notarized (still needs PACE signing for regular Pro Tools)"
    fi

    # Create distributable zips on Desktop
    AU_DIST="$HOME/Desktop/GuitarVocoder-AU.zip"
    VST3_DIST="$HOME/Desktop/GuitarVocoder-VST3.zip"
    ditto -c -k --keepParent "$AU_INSTALL_DIR/$AU_NAME" "$AU_DIST"
    ditto -c -k --keepParent "$VST3_INSTALL_DIR/$VST3_NAME" "$VST3_DIST"
    echo "✓ Distributable AU: $AU_DIST"
    echo "✓ Distributable VST3: $VST3_DIST"

    if $AAX_BUILT; then
        AAX_DIST="$HOME/Desktop/GuitarVocoder-AAX.zip"
        ditto -c -k --keepParent "$AAX_INSTALL_DIR/$AAX_NAME" "$AAX_DIST"
        echo "✓ Distributable AAX: $AAX_DIST"
    fi

    rm -f "$AU_ZIP" "$VST3_ZIP" ${AAX_BUILT:+"$AAX_ZIP"}
fi

echo "✓ Done"
