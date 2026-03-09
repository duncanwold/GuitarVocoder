# Guitar Vocoder — Architecture & Design Decisions

This document explains *why* things work the way they do. The code tells you *what*; this tells you *why*.

## Why the vocoder uses 4th-order filters (two cascaded biquads)

We tested against the Sennheiser VSM-201 which uses 2nd-order (12 dB/oct). We also tested 6th-order (three cascaded biquads, 36 dB/oct). Result: 2nd-order was too leaky between bands, 6th-order made no audible difference over 4th-order. 4th-order (24 dB/oct) is the sweet spot — good isolation without extra CPU.

## Why the de-esser lives inside the vocoder, not on the voice input

Sibilant sounds ("sss", "shh") get triple-amplified: (1) the global noise injection system detects HF voice energy and dumps pink noise into the carrier, (2) the unvoiced detector replaces the guitar carrier with noise in the top 4 bands, (3) the hi-shelf pre-EQ boosts sibilant frequencies before envelope detection.

We first tried a traditional de-esser on the voice input (narrowband compressor at 5-8kHz). It reduced voice HF by ~1dB but the sibilant character in the output was dominated by the noise injection systems, not the voice HF energy. Moving the de-esser inside Vocoder::process() lets it directly scale down `deEssGain` on both noise systems AND attenuate the high-band envelope values during detected sibilants. The sibilant detection reuses values already computed: `noiseEnv / voiceRms` is a free sibilant-to-broadband ratio.

## Why the voice gate has a -60dB floor (never fully closes)

When the gate goes to exactly 0.0, the vocoder's IIR bandpass filters drain to zero internal state. When voice returns and the gate opens, each filter produces its impulse response — all 16 filters ringing simultaneously at their center frequencies creates a tonal "wood block" click. Keeping a -60dB trickle of voice flowing through the filters maintains their internal state. The trickle is inaudible (-60dB below voice level) but keeps the filters "warm."

## Why filters are primed with pink noise in prepare()

At plugin load / playback start, no `processBlock` has run yet, so the gate floor hasn't had a chance to flow signal through the filters. The first voice samples would hit cold filters and ring. Running 10ms of -60dB pink noise through every filter during `prepare()` fills the IIR delay states before the first real audio arrives.

## Why voice dynamics normalization has a 4x gain cap

The voice dynamics system divides the voice signal by its running RMS to normalize volume. When voice first arrives after silence, the RMS tracker is near zero, producing gains of 50x+ that blast all vocoder band envelopes wide open. Capping at 4x (+12dB) is enough to smooth quiet-vs-loud dynamics during normal singing but prevents the onset spike.

## Why the enrichment gain comp is clamped at 0.25x–4x

Same class of bug: the enrichment chain's gain compensation divides output RMS by input RMS. Both start near zero at playback start, producing wildly unstable ratios. The clamp prevents onset clicks. The RMS trackers are also seeded at -60dB instead of zero.

## Why the enrichment gain comp measures input RMS after the octave stage

The OctaveGenerator is an FFT phase vocoder with ~2048 samples of latency. If input RMS is measured before the octave stage, the gain comp compares *present* input level against *delayed* output level — they're measuring different moments in time, producing huge ratio spikes at playback start. Measuring after the octave stage ensures both RMS values track the same (delayed) signal.

## Why Clarity drives 5 internal params (not 4 separate knobs)

The five Clarity sub-systems (noise blend, unvoiced sensitivity, voice dynamics, hi boost, de-ess) all contribute to consonant intelligibility. A user who wants "more speech-like" wants all five to move together. Exposing them individually would create a 5-dimensional tuning problem. The Clarity macro makes it one knob. The Advanced panel shows all five values for transparency, with a draggable de-ess slider for override.

## Why the OctaveGenerator uses FFT phase vocoding

The PRD originally specified zero-crossing frequency division (simple, low-CPU). But frequency division produces aliasing artifacts on polyphonic input and doesn't handle chords well. The FFT phase vocoder (2048-point, 512 hop, 75% overlap) cleanly shifts each frequency bin independently. The tradeoff is ~46ms of latency and higher CPU. When Octave is at center (bypassed), the FFT is skipped entirely — zero latency, zero CPU.

## Why the OctaveGenerator latency is not reported to the host (accepted tradeoff)

When Octave is engaged, the guitar carrier is delayed by 2048 samples (~46ms) relative to the voice. This means voice envelopes run ~46ms ahead of the guitar they're modulating. In theory this degrades intelligibility. In practice, the envelope followers' own smoothing (release times 15-400ms at typical settings) creates an envelope window much wider than the 46ms offset — the envelope is still "open" when the delayed guitar arrives. At extreme Response settings (90%+, release 5ms), the desync could be audible, but this is uncommon in normal use.

The alternative — always adding 46ms of latency to both voice and guitar — penalizes all users (including those who never touch Octave) with audible monitoring delay. Since the desync is not perceptible in practice at typical settings, we accept the tradeoff. If this becomes a problem in the future, the fix is: always report 2048 samples via setLatencySamples(), add a matching delay line to the voice path, and keep the guitar delay line active even when octave is bypassed.

## Why the output leveler targets dry voice RMS

The output leveler adjusts vocoder output volume to approximately match the dry voice level. This means the Dry/Wet slider crossfades without volume jumps — at 50% wet, the dry voice and vocoded signal are at similar levels. Without this, turning up Wet would often mean a volume change.

## Why the gate offset is +7dB (not +10dB)

Originally +10dB above noise floor. Testing showed this was too conservative — quiet speech and breathy consonants got gated out. +7dB still rejects room noise (typical home studio has 15-20dB headroom between noise floor and quiet speech) but catches more of the quiet vocal content.

## Why the Dry/Wet fader uses dim→purple (not pink→cyan)

Throughout the UI, pink=voice and cyan=guitar as signal identifiers. On the fader, "dry" means unprocessed voice and "wet" means vocoded output (which is voice+guitar combined). Using pink→cyan would imply "voice vs guitar" when it actually means "bypass vs effect." Dim→purple correctly reads as "uncolored → colored by the vocoder," matching how purple already represents voice×guitar overlap in the visualizer.

## Why the enrichment chain gain comp is always on

Without gain comp, engaging Drive at 50% would change the carrier level feeding the vocoder, which changes the output level. The user would need to compensate manually every time they adjust an enrichment knob. The always-on gain comp means enrichment knobs change the *character* of the carrier without changing its *level*.

## Why band count changes don't crossfade (accepted dropout)

Changing the band count (8→16→24→32) rebuilds the entire filter bank: new center frequencies, new Q values, new coefficients for all modulator/carrier/noise filters. The rebuild zeros all filter states and re-primes with 10ms of pink noise. This produces a brief audible dropout.

A proper crossfade would require maintaining two complete filter banks simultaneously (old and new), running both for 10-20ms, and blending the outputs. That's doubling the CPU for the transition period and adding significant code complexity for a momentary state change. Since band count switching is a setup gesture (not something you automate or sweep mid-performance), the brief dropout is acceptable. The user clicks a button, hears a tiny blip, and the new band count is active.

## Why Drive and Tone don't have per-sample parameter smoothing (accepted stepping)

The Drive and Tone parameters are read once per processBlock (~86 times/second at 512 samples/44.1kHz) and applied as stepped values. Fast automation sweeps could theoretically produce audible zipper noise from the stepped filter coefficient and waveshaping changes.

In practice testing, rapidly sweeping both Drive and Tone while audio plays produces no audible artifacts. The Drive's waveshaping (tanh) is inherently smooth across small gain changes, and its filter coefficient updates use the `*coefficients = *newCoeffs` pattern (no state reset). The Tone tilt EQ has a 0.01dB threshold before updating. At 86 updates/second, the steps are small enough that the ear doesn't distinguish them from continuous change. Adding per-sample one-pole smoothers (~3 lines per class) would eliminate the theoretical risk but isn't justified by audible results.

## Why voice dynamics onset overshoot is accepted

When voice arrives after silence, the RMS tracker is near zero, so the normalization gain instantly pegs at the 4x cap (+12dB) for the first ~100ms (two time constants of the 50ms RMS window). This means the vocoder "overreacts" briefly on voice onset — slightly brighter and louder than steady-state.

Smoothing normGain with a separate 10ms one-pole would soften the onset, but in practice the effect is subtle and arguably desirable — it gives voice onsets a slight "pop" that can improve perceived intelligibility of initial consonants. The onset ramp (5ms fade-in) already catches the worst of the transient. If this becomes a problem with specific vocal styles (e.g., rapid talk-then-silence patterns), the fix is a one-pole smoother on normGain.

## Why sidechain detection uses sample comparison (Logic-specific)

Logic Pro mirrors the main input into the sidechain bus when no sidechain source is assigned. The plugin detects this by comparing the first 32 samples of the sidechain buffer against the main input — if they're bit-identical, the sidechain is treated as disconnected.

This is a heuristic, not a protocol. It works reliably in Logic because Logic copies the buffer exactly. Other hosts (Ableton, Reaper) may handle unassigned sidechains differently — sending silence, sending the last buffer, or not allocating the bus at all. If the plugin is ever ported to other hosts, this detection needs testing per-host. For the current Logic-only target, the heuristic is reliable.

## Why the AdvancedPanel timer calls are slightly redundant

The AdvancedPanel's timerCallback reads parameter values that are already being updated by the audio thread's macro mapping. The timer runs at 10Hz; processBlock runs at ~86Hz. The values are always current by the time the timer fires. A minor optimization would skip the reads when the audio engine is active, but the overhead is negligible (9 atomic float reads at 10Hz) and the simplicity of always-read is preferable to conditional logic that could show stale values when audio stops.

## Why Sustain was removed

The Keeley-style compressor (SustainCompressor.h) was a v10 feature that didn't justify its UI space. In testing, Sustain at typical settings (25-50%) made a subtle difference to guitar carrier dynamics, but the effect was largely masked by the vocoder's own envelope followers reshaping the signal downstream. The enrichment chain's gain compensation further reduced the perceivable impact. The knob was removed to simplify the Guitar column (4 knobs instead of 5). The class file remains on disk for potential future use.

## Why the Gate slider was removed

The voice gate originally had a user-facing "Gate Offset" slider controlling how far above the noise floor the threshold sat. In practice, the auto-calibrating gate at +7dB worked well across different mic setups and room conditions without user intervention. The gate chatter diagnostic chip was also removed — gate transitions didn't produce audible artifacts thanks to the -60dB gate floor. The gate now runs invisibly, which aligns with the design goal of minimal knob-twiddling for good results.

## Experiments tried and removed

Several A/B experiments were implemented, tested, and removed during development:

- **Dense Highs** (55% of bands above 2kHz): Hurt speech intelligibility by starving the formant region (300Hz-2.5kHz) of band resolution. The opposite of what we needed.
- **Steep Filters** (6th-order / 36 dB/oct bandpass): No audible difference over 4th-order. Extra CPU cost for no benefit.
- **Smooth Envelope** (RMS-based instead of peak follower): Guitar decayed faster at word endings, sounded less musical. Cancelled out the Response slider's attack/release controls.
- **Hilbert Envelope** (90° phase-shift envelope extraction): The 25Hz lowpass smoothing made it sound identical to the peak follower. Would need faster smoothing (~60-100Hz) to differentiate, which was a bigger experiment.
- **Shaped Release** (sqrt-curved release): Only slowed release by 20-40% at typical levels — below audible threshold.
- **Carrier Noise** (-30dB pink noise mixed into guitar): Inaudible when guitar playing covers the vocal frequency range. Only matters with single low notes, which is an uncommon use case.
- **Phase Blend** (15% voice signal mixed back into carrier): Sounded muddy rather than more intelligible.
- **Formant Focus** (60% of bands in 200Hz-3kHz): Sounded worse — too many bands in a narrow range produced comb-filtering artifacts.
- **Carrier Density** (hard clip / rectify / saturate guitar for harmonic fill): Clip was no better than existing Drive. Rectification had DC offset bugs. Saturation was a worse Drive. The existing Drive knob serves the harmonic enrichment role adequately.
