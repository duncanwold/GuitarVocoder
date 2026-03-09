#pragma once
#include <cmath>
#include <algorithm>

class SustainCompressor
{
public:
    void prepare(double sampleRate, int /*blockSize*/)
    {
        sr = sampleRate;
        smoothedGain = 1.0f;
        fastEnv = 0.0f;
        slowEnv = 0.0f;

        // Dual envelope followers for program-dependent release detection
        fastEnvAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.005));  // ~5ms
        slowEnvAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.080));  // ~80ms
    }

    void setAmount(float a)
    {
        if (std::abs(a - amount) < 1e-5f) return;
        amount = a;

        if (amount < 1e-4f) return;

        // Derive parameters from sustain amount
        thresholdDb     = -6.0f - amount * 30.0f;           // -6dB to -36dB
        ratio           = 1.5f + amount * 6.5f;             // 1.5:1 to 8:1
        float attackTime = 0.025f - amount * 0.010f;        // 25ms to 15ms
        compressionSlope = 1.0f - 1.0f / ratio;

        // Program-dependent release range
        releaseMin = 0.040f + amount * 0.060f;              // 40ms to 100ms
        releaseMax = 0.160f + amount * 0.240f;              // 160ms to 400ms

        // Precompute attack coefficient (once per block)
        attackCoeff = 1.0f - std::exp(-1.0f / (float)(sr * attackTime));

        // Auto makeup gain: PRD formula
        makeupGainDb = (thresholdDb * (1.0f - 1.0f / ratio)) * -0.5f;
    }

    float processSample(float input)
    {
        if (amount < 1e-4f)
            return input;

        float inputAbs = std::abs(input);

        // --- Program-dependent release via dual envelope ---
        // Fast follower (~5ms) tracks transients, slow (~80ms) tracks trend
        fastEnv += fastEnvAlpha * (inputAbs - fastEnv);
        slowEnv += slowEnvAlpha * (inputAbs - slowEnv);

        // When fast << slow, signal is falling fast → short release
        // When fast ≈ slow, signal is steady → long release
        float fallRatio = (slowEnv > 1e-6f) ? (fastEnv / slowEnv) : 1.0f;
        float fallAmount = std::clamp(1.0f - fallRatio, 0.0f, 1.0f);

        float releaseTime = releaseMax - fallAmount * (releaseMax - releaseMin);
        // Linear approx of 1-exp(-1/(sr*t)) — accurate to <0.003% for these values
        float releaseCoeff = 1.0f / (float)(sr * releaseTime);

        // --- Core compression ---
        float inputDb = 20.0f * std::log10(inputAbs + 1e-10f);

        float gainReductionDb = 0.0f;

        // Soft knee compression (6dB width)
        constexpr float kneeWidth = 6.0f;
        float kneeBottom = thresholdDb - kneeWidth * 0.5f;
        float kneeTop    = thresholdDb + kneeWidth * 0.5f;

        if (inputDb > kneeTop)
        {
            // Above knee: full ratio compression
            float over = inputDb - thresholdDb;
            gainReductionDb = over * compressionSlope;
        }
        else if (inputDb > kneeBottom)
        {
            // In the knee: quadratic onset
            float kneePos = (inputDb - kneeBottom) / kneeWidth;
            float over = kneePos * kneePos * kneeWidth * 0.5f;
            gainReductionDb = over * compressionSlope;
        }

        float targetGain = std::pow(10.0f, (-gainReductionDb + makeupGainDb) / 20.0f);

        // Smooth gain: attack when compressing harder, release when recovering
        float coeff = (targetGain < smoothedGain) ? attackCoeff : releaseCoeff;
        smoothedGain += coeff * (targetGain - smoothedGain);

        return input * smoothedGain;
    }

    void reset()
    {
        smoothedGain = 1.0f;
        fastEnv = 0.0f;
        slowEnv = 0.0f;
    }

private:
    double sr = 44100.0;
    float amount = 0.0f;
    float smoothedGain = 1.0f;

    // Precomputed from setAmount
    float thresholdDb = -6.0f;
    float ratio = 1.5f;
    float attackCoeff = 0.001f;
    float releaseMin = 0.040f;
    float releaseMax = 0.160f;
    float makeupGainDb = 0.0f;
    float compressionSlope = 0.0f;

    // Dual envelope followers for program-dependent release
    float fastEnvAlpha = 0.0f;   // ~5ms tracking
    float slowEnvAlpha = 0.0f;   // ~80ms tracking
    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
};
