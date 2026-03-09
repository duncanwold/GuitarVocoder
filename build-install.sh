#!/bin/bash
# Build and install the AU plugin to Logic Pro's Components folder
set -e
cd "$(dirname "$0")"
cmake --build build --config Release
cp -r "build/GuitarVocoder_artefacts/AU/Guitar Vocoder.component" ~/Library/Audio/Plug-Ins/Components/
echo "✓ Built and installed Guitar Vocoder AU"
