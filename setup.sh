#!/bin/bash
# =============================================================================
# Guitar Vocoder — Development Environment Setup
# =============================================================================
# Run this script once on a fresh Mac to install everything you need.
# Usage: chmod +x setup.sh && ./setup.sh
# =============================================================================

set -e

echo "🎸🎤 Guitar Vocoder — Setup"
echo "=========================="
echo ""

# --- Check for Xcode command line tools ---
echo "1/4 Checking Xcode command line tools..."
if ! xcode-select -p &>/dev/null; then
    echo "    Installing Xcode command line tools..."
    xcode-select --install
    echo "    ⚠️  A dialog should have appeared. Install, then re-run this script."
    exit 1
else
    echo "    ✅ Xcode CLI tools found"
fi

# --- Check for Homebrew ---
echo ""
echo "2/4 Checking Homebrew..."
if ! command -v brew &>/dev/null; then
    echo "    Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    echo "    ✅ Homebrew installed"
else
    echo "    ✅ Homebrew found"
fi

# --- Install CMake ---
echo ""
echo "3/4 Checking CMake..."
if ! command -v cmake &>/dev/null; then
    echo "    Installing CMake via Homebrew..."
    brew install cmake
    echo "    ✅ CMake installed"
else
    echo "    ✅ CMake found ($(cmake --version | head -1))"
fi

# --- Install JUCE ---
echo ""
echo "4/4 Checking JUCE..."
if [ -d "$HOME/JUCE-installed" ]; then
    echo "    ✅ JUCE found at ~/JUCE-installed"
else
    echo "    Cloning and installing JUCE (this takes a few minutes)..."
    
    if [ ! -d "$HOME/JUCE" ]; then
        git clone https://github.com/juce-framework/JUCE.git ~/JUCE
    fi
    
    cd ~/JUCE
    cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/JUCE-installed
    cmake --build build --target install
    cd -
    
    echo "    ✅ JUCE installed to ~/JUCE-installed"
fi

echo ""
echo "=========================="
echo "✅ All prerequisites installed!"
echo ""
echo "Next steps:"
echo "  1. cd $(dirname "$0")"
echo "  2. mkdir build && cd build"
echo "  3. cmake .. -G Xcode -DCMAKE_PREFIX_PATH=\$HOME/JUCE-installed"
echo "  4. cmake --build . --config Release"
echo ""
echo "Or just run 'claude' in this directory and let Claude Code handle it!"
echo ""
