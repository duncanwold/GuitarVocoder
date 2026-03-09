#pragma once
#include <cmath>
#include <algorithm>

class VoiceGate
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;

        // Gate envelope: 0.5ms attack
        attackCoeff  = 1.0f - std::exp(-1.0f / (float)(sr * 0.0005));
        releaseCoeff = 1.0f - std::exp(-1.0f / (float)(sr * 0.05));  // default, overridden by setRelease
        gateGain = 0.0f;

        // RMS follower: ~10ms window
        rmsAlpha = 1.0f - std::exp(-1.0f / (float)(sr * 0.01));
        rmsSq = 0.0f;

        // Noise floor estimator: very slow (~2s window)
        noiseAlpha = 1.0f - std::exp(-1.0f / (float)(sr * 2.0));
        noiseFloorSq = 0.0f;
        silenceCounter = 0;
        silenceThresholdSamples = (int)(sr * 0.5);  // 500ms of silence

    }

    // Set gate release time (driven by Response macro)
    void setRelease(float ms)
    {
        if (sr > 0.0)
            releaseCoeff = 1.0f - std::exp(-1.0f / (float)(sr * ms * 0.001));
    }

    void setOffsetDb(float db) { offsetDb = db; }

    float getNoiseFloorDb() const
    {
        return 10.0f * std::log10(noiseFloorSq + 1e-10f);
    }

    float getActiveThresholdDb() const
    {
        float nfDb = 10.0f * std::log10(noiseFloorSq + 1e-10f);
        float autoDb = nfDb + offsetDb;
        return std::clamp(autoDb, -55.0f, -20.0f);
    }

    float process(float voiceSample)
    {
        // Track RMS
        rmsSq += rmsAlpha * (voiceSample * voiceSample - rmsSq);
        float rms = std::sqrt(rmsSq);

        // --- Noise floor estimation ---
        // If voice RMS is very low for >500ms, update noise floor estimate
        if (rms < silenceDetectThresh)
        {
            silenceCounter++;
            if (silenceCounter > silenceThresholdSamples)
            {
                // Update noise floor (slow one-pole on the squared RMS)
                noiseFloorSq += noiseAlpha * (rmsSq - noiseFloorSq);
            }
        }
        else
        {
            silenceCounter = 0;
        }

        // --- Determine active threshold ---
        // Auto: noise floor + offset, clamped between -55dB and -20dB
        float noiseFloorDb = 10.0f * std::log10(noiseFloorSq + 1e-10f);
        float autoDb = noiseFloorDb + offsetDb;
        autoDb = std::clamp(autoDb, -55.0f, -20.0f);
        float threshLin = std::pow(10.0f, autoDb / 20.0f);

        // --- Gate envelope ---
        float gateTarget = (rms > threshLin) ? 1.0f : 0.0f;
        float coeff = (gateTarget > gateGain) ? attackCoeff : releaseCoeff;
        gateGain += coeff * (gateTarget - gateGain);

        // Never fully close — keep a tiny trickle flowing to prevent
        // downstream IIR filter cold-start ringing (the "wood block" artifact).
        // -60dB is well below any audible noise floor.
        static constexpr float kGateFloor = 0.001f;
        float effectiveGain = kGateFloor + gateGain * (1.0f - kGateFloor);
        return voiceSample * effectiveGain;
    }

    float getGateGain() const { return gateGain; }

    void reset()
    {
        gateGain = 0.0f;
        rmsSq = 0.0f;
        noiseFloorSq = 0.0f;
        silenceCounter = 0;
    }

private:
    double sr = 44100.0;

    // Gate envelope
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float gateGain = 0.0f;

    // Offset above noise floor (hardcoded +7dB, set in updateDSPFromParams)
    float offsetDb = 7.0f;

    // RMS follower
    float rmsAlpha = 0.0f;
    float rmsSq = 0.0f;

    // Noise floor auto-calibration
    float noiseAlpha = 0.0f;
    float noiseFloorSq = 0.0f;
    int silenceCounter = 0;
    int silenceThresholdSamples = 22050;  // 500ms at 44.1kHz
    static constexpr float silenceDetectThresh = 0.003f;  // ~-50dB — below this is "silence"
};
