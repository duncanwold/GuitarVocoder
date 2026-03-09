#pragma once
#include <cmath>

class TiltEQ
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        updateCoeffs();
    }

    // amount: 0.0 = full dark (-9dB tilt), 0.5 = flat, 1.0 = full bright (+9dB tilt)
    void setAmount(float amount)
    {
        float tiltDb = (amount - 0.5f) * 18.0f;  // -9dB to +9dB
        if (std::abs(tiltDb - currentTiltDb) > 0.01f)
        {
            currentTiltDb = tiltDb;
            updateCoeffs();
        }
    }

    float process(float sample)
    {
        // Low shelf
        float xL = sample;
        float yL = loB0 * xL + loB1 * loX1 + loB2 * loX2 - loA1 * loY1 - loA2 * loY2;
        loX2 = loX1; loX1 = xL;
        loY2 = loY1; loY1 = yL;

        // High shelf
        float xH = yL;
        float yH = hiB0 * xH + hiB1 * hiX1 + hiB2 * hiX2 - hiA1 * hiY1 - hiA2 * hiY2;
        hiX2 = hiX1; hiX1 = xH;
        hiY2 = hiY1; hiY1 = yH;

        return yH;
    }

    void reset()
    {
        loX1 = loX2 = loY1 = loY2 = 0.0f;
        hiX1 = hiX2 = hiY1 = hiY2 = 0.0f;
    }

private:
    void updateCoeffs()
    {
        // When tilting bright: cut lows, boost highs
        // When tilting dark: boost lows, cut highs
        float loGainDb = -currentTiltDb;  // opposite of tilt direction
        float hiGainDb = currentTiltDb;   // same as tilt direction

        computeShelf(loGainDb, 400.0f, 0.7f, loB0, loB1, loB2, loA1, loA2);
        computeShelf(hiGainDb, 2000.0f, 0.7f, hiB0, hiB1, hiB2, hiA1, hiA2);
    }

    // Low/high shelf using RBJ cookbook formulas
    void computeShelf(float gainDb, float freqHz, float Q,
                      float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        float A = std::pow(10.0f, gainDb / 40.0f);  // sqrt of linear gain
        float w0 = 2.0f * 3.14159265f * freqHz / (float)sr;
        float cosw = std::cos(w0);
        float sinw = std::sin(w0);
        float alpha = sinw / (2.0f * Q);
        float sqrtA = std::sqrt(A);

        if (freqHz < 1000.0f)
        {
            // Low shelf
            float a0 = (A + 1) + (A - 1) * cosw + 2 * sqrtA * alpha;
            b0 = (A * ((A + 1) - (A - 1) * cosw + 2 * sqrtA * alpha)) / a0;
            b1 = (2 * A * ((A - 1) - (A + 1) * cosw)) / a0;
            b2 = (A * ((A + 1) - (A - 1) * cosw - 2 * sqrtA * alpha)) / a0;
            a1 = (-2 * ((A - 1) + (A + 1) * cosw)) / a0;
            a2 = ((A + 1) + (A - 1) * cosw - 2 * sqrtA * alpha) / a0;
        }
        else
        {
            // High shelf
            float a0 = (A + 1) - (A - 1) * cosw + 2 * sqrtA * alpha;
            b0 = (A * ((A + 1) + (A - 1) * cosw + 2 * sqrtA * alpha)) / a0;
            b1 = (-2 * A * ((A - 1) + (A + 1) * cosw)) / a0;
            b2 = (A * ((A + 1) + (A - 1) * cosw - 2 * sqrtA * alpha)) / a0;
            a1 = (2 * ((A - 1) - (A + 1) * cosw)) / a0;
            a2 = ((A + 1) - (A - 1) * cosw - 2 * sqrtA * alpha) / a0;
        }
    }

    double sr = 44100.0;
    float currentTiltDb = 0.0f;

    // Low shelf state
    float loB0 = 1, loB1 = 0, loB2 = 0, loA1 = 0, loA2 = 0;
    float loX1 = 0, loX2 = 0, loY1 = 0, loY2 = 0;

    // High shelf state
    float hiB0 = 1, hiB1 = 0, hiB2 = 0, hiA1 = 0, hiA2 = 0;
    float hiX1 = 0, hiX2 = 0, hiY1 = 0, hiY2 = 0;
};
