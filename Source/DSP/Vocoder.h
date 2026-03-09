#pragma once
#include <JuceHeader.h>
#include "EnvelopeFollower.h"
#include <array>
#include <atomic>
#include <vector>

class Vocoder
{
public:
    static constexpr int kMaxBands = 32;
    static constexpr float kMinFreq = 80.0f;
    static constexpr float kMaxFreq = 12000.0f;

    void prepare(double sampleRate, int blockSize);
    void setNumBands(int n);
    void setAttack(float ms);
    void setRelease(float ms);
    void setNoiseAmount(float amount);
    void setDryWet(float mix);
    void setVoiceDynamics(float amount);

    // Clarity: split envelope
    void setHiAttack(float ms);
    void setHiRelease(float ms);
    void applyEnvelopeSettings();

    // De-esser (0 = off, 1 = full reduction)
    void setDeEssAmount(float amount) { deEssAmount = amount; }

    // Re-trigger the 5ms onset ramp (call on preset load to prevent clicks)
    void retriggerOnsetRamp() { onsetRampSamples = 0; }

    // Process with pre-EQ'd voice for envelope detection, raw voice for dry/wet mix
    float process(float voiceSample, float preEQVoice, float guitarSample, float unvoicedAmount);
    void reset();

    // For UI visualizer — atomic snapshots
    std::array<std::atomic<float>, kMaxBands> carrierEnvelopes{};
    std::array<std::atomic<float>, kMaxBands> modulatorEnvelopes{};
    std::array<std::atomic<float>, kMaxBands> outputEnvelopes{};
    std::atomic<int> activeBandCount{16};

private:
    void rebuildFilterBank();
    float generatePinkNoise();

    double sr = 44100.0;
    int numBands = 16;
    float noiseAmount = 0.15f;
    float dryWet = 0.8f;

    // Clarity: unvoiced detection (always on)
    static constexpr bool unvoicedOn = true;
    static constexpr int unvoicedBands = 4;

    // Clarity: split envelope
    static constexpr bool splitEnvOn = true;
    float attackMs = 5.0f;
    float releaseMs = 50.0f;
    float hiAttackMs = 2.0f;
    float hiReleaseMs = 20.0f;
    int bandSplitIndex = 0;

    // Per-band filters: modulator (voice), carrier (guitar), noise (unvoiced)
    // Two cascaded biquads per path = 4th-order (24 dB/oct) for proper spectral isolation
    struct Band
    {
        juce::dsp::IIR::Filter<float> modulatorFilter1;
        juce::dsp::IIR::Filter<float> modulatorFilter2;
        juce::dsp::IIR::Filter<float> carrierFilter1;
        juce::dsp::IIR::Filter<float> carrierFilter2;
        juce::dsp::IIR::Filter<float> noiseFilter1;
        juce::dsp::IIR::Filter<float> noiseFilter2;
        EnvelopeFollower envelopeFollower;
        float carrierDisplayEnv = 0.0f; // peak-hold for visualizer
    };

    std::vector<Band> bands;

    // Noise consonant detection
    juce::dsp::IIR::Filter<float> noiseHighPass;
    EnvelopeFollower noiseEnvFollower;

    // Pink noise generator state
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f;
    float pinkB3 = 0.0f, pinkB4 = 0.0f, pinkB5 = 0.0f, pinkB6 = 0.0f;
    juce::Random rng;

    // Voice input normalization (~50ms RMS window)
    float voiceDynamics = 0.5f; // 0 = raw (expressive), 1 = fully normalized (consistent)
    float voiceRmsAlpha = 0.0f;
    float voiceRmsSq = 0.0f;
    static constexpr float kRmsFloor = 0.001f; // -60 dB
    static constexpr float kMaxNormGain = 4.0f;  // max +12dB normalization gain

    // Output RMS leveler (~300ms slow-acting, targets dry voice level)
    static constexpr bool outputLevelerOn = true;
    float levelerAlpha = 0.0f;
    float dryVoiceRmsSq = 0.0f;
    float wetRmsSq = 0.0f;
    float smoothedLevelerGain = 1.0f;
    float levelerGainAlpha = 0.0f;

    int onsetRampSamples = 0;
    static constexpr int kOnsetRampLength = 240;  // ~5ms at 48kHz
    bool needsRebuild = true;

    // De-esser
    float deEssAmount = 0.0f;
};
