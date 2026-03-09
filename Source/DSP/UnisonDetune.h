#pragma once
#include <cmath>
#include <vector>

// Unison detune via modulated delay lines (chorus topology).
// Two voices with slow sinusoidal LFO at 180° phase offset.
// The changing delay time creates a Doppler pitch shift:
//   voice 1 = pitch shifting up while voice 2 shifts down, and vice versa.
// Unlike a pitch-shifter with diverging read pointers, the modulated delay
// oscillates around a fixed center point — no pointer wrapping discontinuities.
class UnisonDetune
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;

        // Buffer large enough for center delay + max modulation depth + safety margin.
        // Center ~200 samples, max depth ~170 samples, so ~400 samples minimum.
        // Use 2048 for power-of-2 masking and plenty of headroom.
        bufferSize = 2048;
        bufferMask = bufferSize - 1;
        buffer.assign((size_t)bufferSize, 0.0f);
        writePos = 0;
        lfoPhase = 0.0;

        // Center delay: ~4.5ms (enough headroom for max modulation depth)
        centerDelay = (float)(sr * 0.0045);

        updateModParams();
    }

    void setAmount(float a)
    {
        if (std::abs(a - amount) < 1e-5f) return;
        amount = a;
        updateModParams();
    }

    float process(float sample)
    {
        // Write input to circular buffer
        buffer[(size_t)writePos] = sample;
        writePos = (writePos + 1) & bufferMask;

        // Advance LFO (slow sine, ~1 Hz)
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;

        // Sine LFO: voice 1 at current phase, voice 2 at 180° offset
        double sinPhase1 = std::sin(lfoPhase * 6.283185307179586);
        double sinPhase2 = std::sin((lfoPhase + 0.5) * 6.283185307179586);

        // Modulated delay times (in samples, relative to write position)
        float delay1 = centerDelay + (float)(sinPhase1 * modDepth);
        float delay2 = centerDelay + (float)(sinPhase2 * modDepth);

        // Read with linear interpolation
        float voice1 = readInterp(delay1);
        float voice2 = readInterp(delay2);

        // Mix: dry + two detuned voices
        // At full amount, wet voices are equal to dry. Normalize to prevent clipping.
        float wet = amount;
        return sample * (1.0f - wet * 0.33f) + (voice1 + voice2) * (wet * 0.33f);
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lfoPhase = 0.0;
    }

private:
    void updateModParams()
    {
        if (sr <= 0.0) return;

        // LFO rate: 0.8 Hz (slow enough to sound like detune, not vibrato)
        lfoRate = 0.8;
        lfoInc = lfoRate / sr;

        // Modulation depth in samples.
        // The perceived detune in cents ≈ 1200 * depth * 2π * lfoRate / (sr * ln(2))
        // For 2 cents (amount=0): depth ≈ 8 samples
        // For 40 cents (amount=1): depth ≈ 162 samples
        float centsTarget = 2.0f + amount * 38.0f;
        // depth = cents * ln(2) * sr / (1200 * 2π * lfoRate)
        modDepth = (double)centsTarget * 0.693147 * sr / (1200.0 * 6.283185 * lfoRate);
    }

    float readInterp(float delaySamples) const
    {
        // Read position relative to write position (which just advanced)
        float readPos = (float)writePos - delaySamples;
        if (readPos < 0.0f) readPos += (float)bufferSize;

        int intPart = (int)readPos;
        float frac = readPos - (float)intPart;
        int idx0 = intPart & bufferMask;
        int idx1 = (intPart + 1) & bufferMask;

        return buffer[(size_t)idx0] * (1.0f - frac) + buffer[(size_t)idx1] * frac;
    }

    double sr = 44100.0;
    float amount = 0.0f;

    // Circular delay buffer
    std::vector<float> buffer;
    int bufferSize = 2048;
    int bufferMask = 2047;
    int writePos = 0;

    // LFO state
    double lfoPhase = 0.0;
    double lfoInc = 0.0;
    double lfoRate = 0.8;

    // Modulation parameters
    double modDepth = 0.0;      // in samples
    float centerDelay = 200.0f; // in samples (~4.5ms)
};
