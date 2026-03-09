#pragma once
#include <cmath>
#include <algorithm>

class SpectralSpread
{
public:
    static constexpr int kNumBands = 4;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        updateCrossovers();

        float envAlpha = 1.0f - std::exp(-1.0f / (float)(sr * 0.008));
        for (int b = 0; b < kNumBands; ++b)
        {
            bandRms[b] = 0.0f;
            bandRmsAlpha[b] = envAlpha;
        }
        reset();
    }

    void setAmount(float a) { amount = std::clamp(a, 0.0f, 1.0f); }

    float process(float sample)
    {
        if (amount < 1e-4f)
            return sample;

        // Split at 1200 Hz (crossover index 1)
        float lo12  = applyLP(sample, 1, 0);
        lo12        = applyLP(lo12, 1, 1);
        float hi12  = applyHP(sample, 1, 0);
        hi12        = applyHP(hi12, 1, 1);

        // Split lo12 at 300 Hz (crossover index 0)
        float band0 = applyLP(lo12, 0, 0);
        band0       = applyLP(band0, 0, 1);
        float band1 = applyHP(lo12, 0, 0);
        band1       = applyHP(band1, 0, 1);

        // Split hi12 at 4500 Hz (crossover index 2)
        float band2 = applyLP(hi12, 2, 0);
        band2       = applyLP(band2, 2, 1);
        float band3 = applyHP(hi12, 2, 0);
        band3       = applyHP(band3, 2, 1);

        float bands[kNumBands] = {band0, band1, band2, band3};

        // Per-band RMS tracking
        for (int b = 0; b < kNumBands; ++b)
            bandRms[b] += bandRmsAlpha[b] * (bands[b] * bands[b] - bandRms[b]);

        // Target: average RMS across active bands
        float avgRms = 0.0f;
        int activeBands = 0;
        for (int b = 0; b < kNumBands; ++b)
        {
            float rms = std::sqrt(bandRms[b]);
            if (rms > 1e-6f) { avgRms += rms; activeBands++; }
        }
        avgRms = activeBands > 0 ? avgRms / (float)activeBands : 0.0f;

        // Compress each band toward avgRms
        float output = 0.0f;
        for (int b = 0; b < kNumBands; ++b)
        {
            float rms = std::sqrt(bandRms[b]);
            float gain = 1.0f;
            if (rms > 1e-6f && avgRms > 1e-6f)
            {
                float ratio = std::clamp(avgRms / rms, 0.0625f, 16.0f);
                gain = 1.0f + amount * (ratio - 1.0f);
            }
            smoothGain[b] += gainSmoothCoeff * (gain - smoothGain[b]);
            output += bands[b] * smoothGain[b];
        }

        return sample * (1.0f - amount) + output * amount;
    }

    void reset()
    {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2; ++j)
            {
                lpState[i][j] = {};
                hpState[i][j] = {};
            }
        for (int b = 0; b < kNumBands; ++b)
        {
            bandRms[b] = 0.0f;
            smoothGain[b] = 1.0f;
        }
    }

private:
    struct BiquadState { float x1=0, x2=0, y1=0, y2=0; };
    struct BiquadCoeffs { float b0=1, b1=0, b2=0, a1=0, a2=0; };

    float processFilter(float x, BiquadState& s, const BiquadCoeffs& c)
    {
        float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    float applyLP(float x, int idx, int stage) { return processFilter(x, lpState[idx][stage], lpC[idx]); }
    float applyHP(float x, int idx, int stage) { return processFilter(x, hpState[idx][stage], hpC[idx]); }

    BiquadCoeffs lpC[3], hpC[3];
    BiquadState lpState[3][2], hpState[3][2];

    void updateCrossovers()
    {
        float freqs[3] = {300.0f, 1200.0f, 4500.0f};
        for (int i = 0; i < 3; ++i)
        {
            computeButterLP(freqs[i], lpC[i]);
            computeButterHP(freqs[i], hpC[i]);
        }
        gainSmoothCoeff = 1.0f - std::exp(-1.0f / (float)(sr * 0.002));
    }

    void computeButterLP(float freq, BiquadCoeffs& c)
    {
        float w0 = 2.0f * 3.14159265f * freq / (float)sr;
        float cosw = std::cos(w0), sinw = std::sin(w0);
        float alpha = sinw / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;
        c.b0 = ((1.0f - cosw) * 0.5f) / a0;
        c.b1 = (1.0f - cosw) / a0;
        c.b2 = c.b0;
        c.a1 = (-2.0f * cosw) / a0;
        c.a2 = (1.0f - alpha) / a0;
    }

    void computeButterHP(float freq, BiquadCoeffs& c)
    {
        float w0 = 2.0f * 3.14159265f * freq / (float)sr;
        float cosw = std::cos(w0), sinw = std::sin(w0);
        float alpha = sinw / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;
        c.b0 = ((1.0f + cosw) * 0.5f) / a0;
        c.b1 = -(1.0f + cosw) / a0;
        c.b2 = c.b0;
        c.a1 = (-2.0f * cosw) / a0;
        c.a2 = (1.0f - alpha) / a0;
    }

    double sr = 44100.0;
    float amount = 0.0f;
    float bandRms[kNumBands] = {};
    float bandRmsAlpha[kNumBands] = {};
    float smoothGain[kNumBands] = {1, 1, 1, 1};
    float gainSmoothCoeff = 0.0f;
};
