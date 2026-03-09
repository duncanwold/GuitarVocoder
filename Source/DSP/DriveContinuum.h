#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

class DriveContinuum
{
public:
    void prepare(double sampleRate, int /*blockSize*/)
    {
        sr = sampleRate;

        // Low shelf at 500Hz for fat fuzz pre-boost
        lastShelfGainDb = -999.0f;
        updateLowShelf(0.0f);

        // Low-pass for post-fuzz tone shaping
        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 8000.0f);
        fuzzLowPass.coefficients = lpCoeffs;
        fuzzLowPass.reset();

        // High-pass at 3kHz for exciter blend
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 3000.0f);
        exciterHighPass.coefficients = hpCoeffs;
        exciterHighPass.reset();

        lowShelfState.reset();
    }

    void setAmount(float a)
    {
        if (std::abs(a - amount) < 1e-5f) return;
        amount = a;

        // Update fuzz-zone filter coefficients once per block (not per sample)
        if (amount > 0.70f && sr > 0.0)
        {
            float t = (amount - 0.70f) / 0.30f;
            // +9dB shelf at full for Big Muff low-mid thickness
            updateLowShelf(9.0f * t);
            // Post-clip tone: 8kHz at zone entrance → 5kHz at full fuzz
            float lpFreq = 8000.0f - t * 3000.0f;
            auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, lpFreq);
            *fuzzLowPass.coefficients = *lpCoeffs;
        }
        else if (sr > 0.0)
        {
            updateLowShelf(0.0f);
        }
    }

    float processSample(float input)
    {
        if (amount < 1e-4f)
            return input;

        // Continuous drive gain across all zones — no boundary jumps
        float drive = 1.0f + amount * 15.0f;

        if (amount <= 0.30f)
        {
            // Pure saturation: symmetric tanh, gain-normalized
            float tanhD = std::tanh(drive);
            return std::tanh(input * drive) / (tanhD > 0.0f ? tanhD : 1.0f);
        }

        if (amount <= 0.70f)
        {
            // Crossfade saturation → asymmetric drive
            float t = (amount - 0.30f) / 0.40f;
            float tanhD = std::tanh(drive);
            float satOut = std::tanh(input * drive) / (tanhD > 0.0f ? tanhD : 1.0f);
            float drvOut = (input > 0.0f)
                ? std::tanh(input * drive * 1.5f)
                : std::tanh(input * drive);
            return satOut * (1.0f - t) + drvOut * t;
        }

        // Crossfade drive → fat fuzz (0.70 – 1.00)
        float t = (amount - 0.70f) / 0.30f;

        // Drive component (continuous with drive zone at t=0)
        float drvOut = (input > 0.0f)
            ? std::tanh(input * drive * 1.5f)
            : std::tanh(input * drive);

        // Fat fuzz: shelf-boosted input → aggressive hard clip
        float boosted = lowShelfState.processSample(input);
        float fuzzGain = drive * (1.0f + t);  // ramps from 1× to 2× drive
        float fuzzOut = std::clamp(boosted * fuzzGain, -1.0f, 1.0f);

        // Post-clip tone filter (coefficients set in setAmount)
        fuzzOut = fuzzLowPass.processSample(fuzzOut);

        // Exciter for presence (25% max blend)
        float highs = exciterHighPass.processSample(fuzzOut);
        float excited = std::tanh(highs * (1.0f + t * 3.0f));
        fuzzOut = fuzzOut + excited * t * 0.25f;

        return drvOut * (1.0f - t) + fuzzOut * t;
    }

    void reset()
    {
        lowShelfState.reset();
        fuzzLowPass.reset();
        exciterHighPass.reset();
    }

private:
    void updateLowShelf(float gainDb)
    {
        if (sr <= 0.0 || std::abs(gainDb - lastShelfGainDb) < 0.01f)
            return;
        lastShelfGainDb = gainDb;
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sr, 500.0f, 0.707f, std::pow(10.0f, gainDb / 20.0f));
        lowShelfState.coefficients = coeffs;
    }

    double sr = 44100.0;
    float amount = 0.0f;
    float lastShelfGainDb = -999.0f;

    juce::dsp::IIR::Filter<float> lowShelfState;   // 500Hz low shelf
    juce::dsp::IIR::Filter<float> fuzzLowPass;     // post-fuzz tone
    juce::dsp::IIR::Filter<float> exciterHighPass;  // 3kHz exciter
};
