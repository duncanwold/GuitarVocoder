#pragma once
#include "TiltEQ.h"
#include "DriveContinuum.h"
#include "BitCrusher.h"
#include "UnisonDetune.h"
#include "OctaveGenerator.h"
#include "SpectralSpread.h"

class GuitarEnrichment
{
public:
    void prepare(double sampleRate, int blockSize)
    {
        tiltEQ.prepare(sampleRate);
        driveCont.prepare(sampleRate, blockSize);
        unison.prepare(sampleRate);
        octaves.prepare(sampleRate);
        spread.prepare(sampleRate);

        // ~50ms RMS smoothing window for gain compensation
        rmsAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.05));
        // Seed with a quiet but non-zero value to prevent cold-start ratio instability
        inputRmsSq = 1e-6f;   // ~-60dB
        outputRmsSq = 1e-6f;  // ~-60dB
    }

    // Chain order: Tone → Octave → Spread → Drive → Crush → Unison → Gain Comp
    float process(float sample)
    {
        float s = sample;

        s = tiltEQ.process(s);
        s = octaves.process(s);

        // Track input RMS AFTER octave stage so it's time-aligned with output.
        inputRmsSq += rmsAlpha * (s * s - inputRmsSq);

        if (spreadAmount > 1e-4f) s = spread.process(s);
        s = driveCont.processSample(s);
        if (crushAmount > 1e-4f)  s = crusher.process(s);
        if (unisonAmount > 1e-4f) s = unison.process(s);

        // Gain compensation — cap ratio to prevent onset spikes
        // when RMS trackers are near zero at playback start
        {
            outputRmsSq += rmsAlpha * (s * s - outputRmsSq);
            float inRms = std::sqrt(inputRmsSq);
            float outRms = std::sqrt(outputRmsSq);
            if (outRms > 0.0001f && inRms > 0.0001f)
            {
                float ratio = juce::jlimit(0.25f, 4.0f, inRms / outRms);
                s *= ratio;
            }
        }

        return s;
    }

    void reset()
    {
        tiltEQ.reset();
        driveCont.reset();
        crusher.reset();
        unison.reset();
        octaves.reset();
        spread.reset();
        inputRmsSq = 1e-6f;
        outputRmsSq = 1e-6f;
    }

    // Tone/Tilt EQ (0.5 = flat/bypass)
    void setToneAmount(float a) { tiltEQ.setAmount(a); }

    // Drive continuum (0 = bypassed internally)
    void setDriveAmount(float a) { driveCont.setAmount(a); }

    // Crush (amount 0 = bypassed by check above)
    void setCrushAmount(float a) { crushAmount = a; crusher.setAmount(a); }

    // Unison (amount 0 = bypassed by check above)
    void setUnisonAmount(float a) { unisonAmount = a; unison.setAmount(a); }

    // Octave bipolar knob (0-1 param, 0.5 = center/dry)
    void setOctaveBipolar(float param) { octaves.setFromBipolar(param); }

    // Spread (0 = bypassed by check above)
    void setSpreadAmount(float a) { spreadAmount = a; spread.setAmount(a); }

private:
    TiltEQ tiltEQ;
    DriveContinuum driveCont;
    BitCrusher crusher;
    UnisonDetune unison;
    OctaveGenerator octaves;
    SpectralSpread spread;

    float crushAmount = 0.0f;
    float unisonAmount = 0.0f;
    float spreadAmount = 0.0f;

    // RMS gain compensation (always on)
    float rmsAlpha = 0.0f;
    float inputRmsSq = 0.0f;
    float outputRmsSq = 0.0f;
};
