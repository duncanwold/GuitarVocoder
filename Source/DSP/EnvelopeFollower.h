#pragma once
#include <cmath>

class EnvelopeFollower
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        updateCoeffs();
        envelope = 0.0f;
    }

    void setAttack(float ms)
    {
        attackMs = ms;
        updateCoeffs();
    }

    void setRelease(float ms)
    {
        releaseMs = ms;
        updateCoeffs();
    }

    float process(float sample)
    {
        float rectified = std::abs(sample);
        float coeff = (rectified > envelope) ? attackCoeff : releaseCoeff;
        envelope = envelope + coeff * (rectified - envelope);
        return envelope;
    }

    void reset() { envelope = 0.0f; }

private:
    void updateCoeffs()
    {
        if (sr <= 0.0) return;
        attackCoeff = 1.0f - std::exp(-1.0f / (float)(attackMs * 0.001 * sr));
        releaseCoeff = 1.0f - std::exp(-1.0f / (float)(releaseMs * 0.001 * sr));
    }

    double sr = 44100.0;
    float attackMs = 5.0f;
    float releaseMs = 50.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
};
