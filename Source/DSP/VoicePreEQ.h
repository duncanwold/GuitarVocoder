#pragma once
#include <JuceHeader.h>

class VoicePreEQ
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        updateFilters();
        hpFilter.reset();
        shelfFilter.reset();
    }

    void setLowCutFreq(float hz)
    {
        if (std::abs(hz - hpFreq) > 0.1f)
        {
            hpFreq = hz;
            updateFilters();
        }
    }

    void setHighBoost(float dB)
    {
        if (std::abs(dB - boostDb) > 0.01f)
        {
            boostDb = dB;
            updateFilters();
        }
    }

    float process(float sample)
    {
        float out = hpFilter.processSample(sample);
        out = shelfFilter.processSample(out);
        return out;
    }

    void reset()
    {
        hpFilter.reset();
        shelfFilter.reset();
    }

private:
    void updateFilters()
    {
        if (sr <= 0.0) return;

        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpFreq, 0.707f);
        *hpFilter.coefficients = *hpCoeffs;

        float gainFactor = std::pow(10.0f, boostDb / 20.0f);
        auto shelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 3000.0f, 0.707f, gainFactor);
        *shelfFilter.coefficients = *shelfCoeffs;
    }

    double sr = 44100.0;
    float hpFreq = 80.0f;
    float boostDb = 3.0f;
    juce::dsp::IIR::Filter<float> hpFilter;
    juce::dsp::IIR::Filter<float> shelfFilter;
};
