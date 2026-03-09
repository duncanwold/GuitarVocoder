#pragma once
#include <cmath>
#include <algorithm>

class BitCrusher
{
public:
    void setAmount(float a)
    {
        amount = a;
        // Precompute — these only change when amount changes, not per sample
        float bits = 32.0f - amount * 30.0f;
        levels = std::max(1.0f, std::pow(2.0f, bits));
        holdInterval = 1 + (int)(amount * 40.0f);
    }

    float process(float sample)
    {
        // Sample rate reduction (sample-and-hold)
        if (++sampleCounter >= holdInterval)
        {
            sampleCounter = 0;
            heldSample = sample;
        }

        // Bit depth reduction
        return std::round(heldSample * levels) / levels;
    }

    void reset()
    {
        heldSample = 0.0f;
        sampleCounter = 0;
    }

private:
    float amount = 0.0f;
    float levels = 4294967296.0f;  // pow(2, 32)
    int holdInterval = 1;
    float heldSample = 0.0f;
    int sampleCounter = 0;
};
