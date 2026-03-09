# Guitar Vocoder

A vocoder Audio Unit plugin for Logic Pro. Speak or sing into a mic while playing guitar — the plugin applies your voice's spectral shape to the guitar signal, making the guitar "speak."

Built with [JUCE](https://juce.com/) and C++17. Developed by [Yacht Pockets](https://github.com/duncanwold).

![Guitar Vocoder Screenshot](screenshot.png)

## Features

- **10 controls, no menu diving** — two macro knobs (Response + Clarity) control 9 internal parameters. An Advanced panel shows everything under the hood.
- **16-band vocoder** (configurable 8/16/24/32) with 4th-order bandpass filters for clean spectral isolation.
- **Guitar enrichment chain** — Tone, Octave (FFT phase vocoder), Spread, Drive (3-zone saturation → fuzz), Crush, and Unison (chorus-topology detune).
- **Intelligent voice processing** — auto-calibrating noise gate, voiced/unvoiced consonant detection, voice dynamics normalization, and a sibilant de-esser.
- **Real-time diagnostics** — split mirror visualizer, frequency coverage meters, and hint chips that suggest how to improve your guitar tone for better vocoder results.
- **15 factory presets** — from Talk Box and Zapp Bounce to Demon Voice and Broken Radio.

## Requirements

- macOS (Apple Silicon)
- Logic Pro (or any AU-compatible host)
- CMake 3.22+
- JUCE 7+ (installed via CMake)

## Build

First-time setup (installs Xcode CLI tools, Homebrew, CMake, and JUCE):

```bash
chmod +x setup.sh && ./setup.sh
```

Then build and install to Logic:

```bash
./build-install.sh
```

Or build manually:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$HOME/JUCE-installed
cmake --build . --config Release
cp -r "GuitarVocoder_artefacts/AU/Guitar Vocoder.component" ~/Library/Audio/Plug-Ins/Components/
```

## Usage

1. Create an audio track in Logic with your mic as input
2. Add **Audio Units → YachtPockets → Guitar Vocoder** to the track
3. Set the plugin's **Sidechain** (in Logic's plugin header) to your guitar input
4. Play guitar and sing — the Guitar column lights up when sidechain is connected

Start with the **Talk Box** preset and adjust **Clarity** for speech intelligibility and **Response** for envelope speed.

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed design decisions — why the de-esser lives inside the vocoder, why the gate never fully closes, why the octave latency isn't reported to the host, and what experiments were tried and removed.

See [CLAUDE.md](CLAUDE.md) for a technical reference of the signal flow, parameters, macro curves, and UI layout.

## License

MIT License — see [LICENSE](LICENSE).
