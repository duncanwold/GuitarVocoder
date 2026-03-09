#pragma once
#include "VoicePreEQ.h"
#include "VoicedUnvoicedDetector.h"

class ClarityEngine
{
public:
    void prepare(double sampleRate, int blockSize)
    {
        preEQ.prepare(sampleRate);
        detector.prepare(sampleRate, blockSize);
    }

    // Process voice through pre-EQ and update voiced/unvoiced detector.
    // Returns the pre-EQ'd voice signal (for vocoder envelope detection).
    float processVoice(float voiceSample)
    {
        float eqd = preEQ.process(voiceSample);
        detector.processSample(eqd);
        return eqd;
    }

    float getUnvoicedAmount() const { return detector.getUnvoicedAmount(); }

    void setLowCutFreq(float hz)    { preEQ.setLowCutFreq(hz); }
    void setHighBoost(float dB)     { preEQ.setHighBoost(dB); }
    void setSensitivity(float s)    { detector.setSensitivity(s); }
    void reset()
    {
        preEQ.reset();
        detector.reset();
    }

private:
    VoicePreEQ preEQ;
    VoicedUnvoicedDetector detector;
};
