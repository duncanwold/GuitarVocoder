#pragma once
#include <cmath>
#include <algorithm>

class VoicedUnvoicedDetector
{
public:
    void prepare(double sampleRate, int blockSize)
    {
        sr = sampleRate;
        analysisBlockSize = std::min(blockSize, 128);
        reset();
    }

    void setSensitivity(float s) { sensitivity = s; }

    // Call per sample. Returns current unvoicedAmount (smoothed 0.0-1.0)
    float processSample(float voiceSample)
    {
        // Count zero crossings
        if ((voiceSample >= 0.0f && prevSample < 0.0f) ||
            (voiceSample < 0.0f && prevSample >= 0.0f))
            zeroCrossings++;

        rmsAccum += voiceSample * voiceSample;
        prevSample = voiceSample;
        sampleCount++;

        if (sampleCount >= analysisBlockSize)
        {
            float zcr = (float)zeroCrossings / (float)analysisBlockSize;
            float rms = std::sqrt(rmsAccum / (float)analysisBlockSize);

            // sensitivity 0% -> threshold 0.40 (only hard sibilants)
            // sensitivity 100% -> threshold 0.10 (aggressive)
            float threshold = 0.40f - sensitivity * 0.30f;
            bool isUnvoiced = (zcr > threshold) && (rms > 0.001f);

            // Smooth: gentle onset, slow release
            if (isUnvoiced)
                unvoicedAmount += (1.0f - unvoicedAmount) * onsetCoeff;
            else
                unvoicedAmount += (0.0f - unvoicedAmount) * 0.05f;

            // Hard ceiling: always keep at least some guitar carrier
            if (unvoicedAmount > unvoicedCeiling)
                unvoicedAmount = unvoicedCeiling;

            // Reset accumulators
            sampleCount = 0;
            zeroCrossings = 0;
            rmsAccum = 0.0f;
        }

        return unvoicedAmount;
    }

    float getUnvoicedAmount() const { return unvoicedAmount; }

    void reset()
    {
        sampleCount = 0;
        zeroCrossings = 0;
        rmsAccum = 0.0f;
        prevSample = 0.0f;
        unvoicedAmount = 0.0f;
    }

private:
    double sr = 44100.0;
    int analysisBlockSize = 128;
    int sampleCount = 0;
    int zeroCrossings = 0;
    float rmsAccum = 0.0f;
    float prevSample = 0.0f;
    float sensitivity = 0.5f;
    float unvoicedAmount = 0.0f;
    float unvoicedCeiling = 0.7f;
    float onsetCoeff = 0.15f;
};
