# Guitar Vocoder — JUCE Audio Unit Plugin

## What It Is
A vocoder plugin for Logic Pro. Voice (modulator) on the main input, guitar (carrier) via sidechain. The voice's spectral shape is applied to the guitar signal, making the guitar "speak."

## Build
```bash
cd ~/Desktop/GuitarVocoder
cmake --build build --config Release
# Or use the helper script which also installs to Logic:
./build-install.sh
```

## Signal Flow

```
VOICE (main in) → Voice Gate (auto, +7dB above noise floor)
                    → ClarityEngine: Pre-EQ (80Hz HPF + hi shelf) + Voiced/Unvoiced Detector
                        → Vocoder: voice dynamics normalization → band filtering → envelope followers
                                                                                       ↓
GUITAR (sidechain) → GuitarEnrichment: Tone → Octave → Spread → Drive → Crush → Unison → Gain Comp
                        → Vocoder: band filtering → multiply carrier bands × voice envelopes
                                                                                       ↓
                                                    Noise injection (global + per-band unvoiced) with de-ess
                                                                                       ↓
                                                    Sum bands → gain comp → output leveler → onset ramp
                                                                                       ↓
                                                                              Dry/Wet mix → OUTPUT
```

## Source Files

```
Source/
├── PluginProcessor.cpp/.h      — Audio processing, parameter registration, macro wiring
├── PluginEditor.cpp/.h         — UI (Computer Love theme), visualizer, diagnostics, Advanced panel
├── PresetManager.cpp/.h        — Factory + user presets
├── MacroMapping.h              — Response and Clarity macro curves
└── DSP/
    ├── Vocoder.cpp/.h          — Core vocoder: filter bank, envelope followers, de-ess, noise injection
    ├── EnvelopeFollower.h      — Per-band peak amplitude tracker
    ├── ClarityEngine.h         — Voice pre-EQ + voiced/unvoiced detector
    ├── VoicePreEQ.h            — 80Hz HPF + high shelf (driven by Clarity)
    ├── VoicedUnvoicedDetector.h — Zero-crossing rate classifier
    ├── VoiceGate.h             — Auto-calibrating noise gate with -60dB floor
    ├── GuitarEnrichment.h      — Enrichment chain manager
    ├── TiltEQ.h                — Bipolar tilt EQ (Tone control)
    ├── OctaveGenerator.h       — FFT phase vocoder for sub/high octaves
    ├── SpectralSpread.h        — Stereo spread
    ├── DriveContinuum.h        — 3-zone distortion: saturate → drive → fuzz
    ├── BitCrusher.h            — Bit/sample rate reduction
    ├── UnisonDetune.h          — Detuned copies
    └── SustainCompressor.h     — (unused, kept on disk)
```

## Parameters

### User-Facing (10)
| ID | Name | Type | Range | Default | UI |
|----|------|------|-------|---------|-----|
| numBands | Bands | Choice | 8/16/24/32 | 16 | Segmented buttons |
| response | Response | Float | 0–1 | 0.50 | Vocoder slider |
| clarity | Clarity | Float | 0–1 | 1.00 | Vocoder slider |
| tone | Tone | Float | 0–1 | 0.50 | Bipolar slider (dark↔bright) |
| drive | Drive | Float | 0–1 | 0.15 | Rotary knob |
| octave | Octave | Float | 0–1 | 0.50 | Bipolar slider (sub↔high) |
| crush | Crush | Float | 0–1 | 0.00 | Rotary knob |
| unison | Unison | Float | 0–1 | 0.00 | Rotary knob |
| spread | Spread | Float | 0–1 | 0.00 | Rotary knob |
| dryWet | Dry/Wet | Float | 0–1 | 0.85 | Footer slider |

### System (2)
| ID | Type | Default |
|----|------|---------|
| bypass | Bool | off |
| monitorGuitar | Bool | off |

### Internal — Macro-Driven (9)
| ID | Range | Driven By |
|----|-------|-----------|
| loAttack | 0.5–50 ms | Response |
| loRelease | 15–400 ms | Response |
| hiAttack | 0.5–20 ms | Response |
| hiRelease | 5–120 ms | Response |
| noise | 0–0.20 | Clarity |
| unvoicedSens | 0.1–0.65 | Clarity |
| voiceDynamics | 0–1 | Clarity |
| voiceHighBoost | 0–10 dB | Clarity |
| deEss | 0–1 | Clarity |

### Hardcoded
Voice Lo Cut: 80 Hz, Gate Offset: +7 dB, Unvoiced Bypass Bands: 4, Split Envelope: always on, Output Leveler: always on, Chain Gain Comp: always on.

## Macro Curves (MacroMapping.h)

**Response** (0→1): Lo Attack 50→1ms, Lo Release 400→15ms, Hi Attack 20→0.5ms, Hi Release 120→5ms. All exponential.

**Clarity** (0→1): Noise Blend 0→20%, UV Sensitivity 10→65%, Voice Dynamics sigmoid 0→100%, Hi Boost 0→10dB (quadratic), De-Ess 0→75% (linear).

## UI Layout (850px wide)
- Header: icon, title, preset selector (factory only, no save in v1.0), input meters (V/G), MONITOR button
- Visualizer: split mirror bar chart (voice up, guitar down, purple overlap)
- Diagnostics bar: Voice/Guitar LEDs, Match %, Lo/Mid/Hi coverage bars, 5 hint chips
- Controls: two-column — Vocoder (Bands, Response, Clarity) | Guitar (Tone, Octave, 4 knobs)
- Footer: Dry/Wet slider (dim→purple gradient)
- Advanced panel: collapsible, three columns (Response / Clarity / Global)

## Conventions
- DSP classes: `prepare(sampleRate, blockSize)`, `process(sample)`, `reset()`
- Audio↔UI data via `std::atomic<float>` — never allocate in process()
- All params via APVTS for Logic automation
- Enrichment knobs: 0% = off, no toggles
- Theme: Computer Love (Zapp & Roger electro-funk). Font: Menlo. Colors in AGI namespace.

## Known Issues / Pending
- Factory presets may need retuning after Clarity/De-Ess changes
- OctaveGenerator latency (~46ms) not reported to host — accepted tradeoff, see ARCHITECTURE.md
- UI screenshot in docs/ is outdated

## Deferred (documented, not addressed)
- Band count change causes brief dropout (rebuildFilterBank zeros filters) — rare user gesture, crossfade would add significant complexity
- Fast automation sweeps on Drive/Tone can produce stepped filter coefficients — inaudible in practice testing, would need per-sample parameter smoothing
