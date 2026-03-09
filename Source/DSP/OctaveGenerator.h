#pragma once
#include <cmath>
#include <algorithm>
#include <cstring>

// FFT Phase Vocoder Octave Generator
// Clean polyphonic pitch shifting — same approach as the EHX POG.
// Each frequency bin is shifted independently, avoiding intermodulation.
class OctaveGenerator
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        buildWindow();
        reset();

        // Prime ALL FFT output buffers regardless of current mix state,
        // so no cold-start windowing transient ("wood block" click) occurs
        // when real audio arrives, at any octave knob position.
        {
            // Save and temporarily force all paths active
            float savedDry   = dryMix,    savedSub1 = subOct1Mix, savedSub2 = subOct2Mix;
            float savedHigh1 = highOct1Mix, savedHigh2 = highOct2Mix;
            dryMix = subOct1Mix = subOct2Mix = highOct1Mix = highOct2Mix = 1.0f;

            uint32_t lcg = 0x12345678u;
            constexpr float kPrimeAmp = 1e-6f;           // ~-120 dBFS
            constexpr int kPrimeSamples = kFFTSize * 4;  // 16 FFT frames
            for (int i = 0; i < kPrimeSamples; ++i)
            {
                lcg = lcg * 1664525u + 1013904223u;
                float noise = (float)(int32_t)lcg * kPrimeAmp * (1.0f / 2147483648.0f);
                process(noise);
            }

            // Restore actual mix state
            dryMix = savedDry;  subOct1Mix = savedSub1;  subOct2Mix = savedSub2;
            highOct1Mix = savedHigh1;  highOct2Mix = savedHigh2;
        }
    }

    // Bipolar octave knob: param 0-1, mapped to bipolar -1..+1
    void setFromBipolar(float param)
    {
        float bipolar = param * 2.0f - 1.0f;

        if (bipolar < 0.0f)
        {
            float subDepth = std::abs(bipolar);
            subOct1Mix  = std::min(1.0f, subDepth * 2.0f);
            subOct2Mix  = std::max(0.0f, subDepth * 2.0f - 1.0f);
            highOct1Mix = 0.0f;
            highOct2Mix = 0.0f;
            dryMix      = 1.0f - subDepth * 0.7f;
        }
        else if (bipolar > 0.0f)
        {
            float highDepth = bipolar;
            highOct1Mix = std::min(1.0f, highDepth * 2.0f);
            highOct2Mix = std::max(0.0f, highDepth * 2.0f - 1.0f);
            subOct1Mix  = 0.0f;
            subOct2Mix  = 0.0f;
            dryMix      = 1.0f - highDepth * 0.7f;
        }
        else
        {
            dryMix      = 1.0f;
            subOct1Mix  = 0.0f;
            subOct2Mix  = 0.0f;
            highOct1Mix = 0.0f;
            highOct2Mix = 0.0f;
        }
    }

    float process(float sample)
    {
        // Write input to circular buffer (always, so data is ready if octaves activate)
        inputBuf[writePos] = sample;

        // When no pitch shifting is active, bypass the entire FFT pipeline.
        // This avoids the 2048-sample (~46ms) latency that causes gain comp
        // misalignment in the enrichment chain.
        if (subOct1Mix < 1e-5f && subOct2Mix < 1e-5f
         && highOct1Mix < 1e-5f && highOct2Mix < 1e-5f)
        {
            writePos = (writePos + 1) & kBufMask;
            if (++hopCount >= kHopSize)
                hopCount = 0;
            return sample;
        }

        // Read delayed dry + octave outputs
        int readIdx = (writePos - kFFTSize + 1 + kBufSize) & kBufMask;
        float dry   = dryBuf[readIdx];
        float sub1  = sub1Buf[readIdx];
        float sub2  = sub2Buf[readIdx];
        float high1 = high1Buf[readIdx];
        float high2 = high2Buf[readIdx];

        // Clear output at read position (consumed)
        dryBuf[readIdx]   = 0.0f;
        sub1Buf[readIdx]  = 0.0f;
        sub2Buf[readIdx]  = 0.0f;
        high1Buf[readIdx] = 0.0f;
        high2Buf[readIdx] = 0.0f;

        writePos = (writePos + 1) & kBufMask;

        // Process FFT frame every hop
        if (++hopCount >= kHopSize)
        {
            hopCount = 0;
            processFrame();
        }

        // Mix
        float mix = dry   * dryMix
                  + sub1  * subOct1Mix
                  + sub2  * subOct2Mix
                  + high1 * highOct1Mix
                  + high2 * highOct2Mix;

        float faderSum = dryMix + subOct1Mix + subOct2Mix + highOct1Mix + highOct2Mix;
        return (faderSum > 1.0f) ? mix / faderSum : mix;
    }

    void reset()
    {
        std::memset(inputBuf, 0, sizeof(inputBuf));
        std::memset(dryBuf, 0, sizeof(dryBuf));
        std::memset(sub1Buf, 0, sizeof(sub1Buf));
        std::memset(sub2Buf, 0, sizeof(sub2Buf));
        std::memset(high1Buf, 0, sizeof(high1Buf));
        std::memset(high2Buf, 0, sizeof(high2Buf));
        std::memset(prevPhase, 0, sizeof(prevPhase));
        std::memset(synthPhaseSub1, 0, sizeof(synthPhaseSub1));
        std::memset(synthPhaseSub2, 0, sizeof(synthPhaseSub2));
        std::memset(synthPhaseHi1, 0, sizeof(synthPhaseHi1));
        std::memset(synthPhaseHi2, 0, sizeof(synthPhaseHi2));
        writePos = 0;
        hopCount = 0;
    }

private:
    static constexpr float kPi    = 3.14159265358979f;
    static constexpr float kTwoPi = 6.28318530717959f;

    // FFT parameters
    static constexpr int kFFTOrder = 11;                   // 2048-point FFT
    static constexpr int kFFTSize  = 1 << kFFTOrder;       // 2048
    static constexpr int kHalfFFT  = kFFTSize / 2;         // 1024
    static constexpr int kHopSize  = kFFTSize / 4;         // 512 (75% overlap)
    static constexpr int kBufSize  = kFFTSize * 2;         // circular buffer size (power of 2)
    static constexpr int kBufMask  = kBufSize - 1;

    // Expected phase advance per bin per hop
    static constexpr float kFreqPerBin = kTwoPi / (float)kFFTSize;
    static constexpr float kExpectedPhaseStep = kTwoPi * (float)kHopSize / (float)kFFTSize;

    // Overlap-add normalization: for Hann window with 75% overlap
    // Sum of squared windows at each sample = 1.5, so divide by that
    static constexpr float kOLANorm = 1.0f / 1.5f;

    double sr = 44100.0;

    // Circular buffers
    float inputBuf[kBufSize] = {};
    float dryBuf[kBufSize]   = {};
    float sub1Buf[kBufSize]  = {};
    float sub2Buf[kBufSize]  = {};
    float high1Buf[kBufSize] = {};
    float high2Buf[kBufSize] = {};
    int writePos = 0;
    int hopCount = 0;

    // Hann window
    float window[kFFTSize] = {};

    // Phase vocoder state (per bin)
    float prevPhase[kHalfFFT + 1]      = {};
    float synthPhaseSub1[kHalfFFT + 1]  = {};
    float synthPhaseSub2[kHalfFFT + 1]  = {};
    float synthPhaseHi1[kHalfFFT + 1]   = {};
    float synthPhaseHi2[kHalfFFT + 1]   = {};

    // Mix levels
    float dryMix = 1.0f;
    float subOct1Mix = 0.0f, subOct2Mix = 0.0f;
    float highOct1Mix = 0.0f, highOct2Mix = 0.0f;

    void buildWindow()
    {
        for (int i = 0; i < kFFTSize; ++i)
            window[i] = 0.5f * (1.0f - std::cos(kTwoPi * (float)i / (float)kFFTSize));
    }

    // ---- In-place radix-2 Cooley-Tukey FFT ----
    // data: interleaved [re, im, re, im, ...] with 2*N floats
    // N must be power of 2. inverse: true for IFFT.
    static void fft(float* data, int N, bool inverse)
    {
        // Bit-reversal permutation
        int j = 0;
        for (int i = 0; i < N; ++i)
        {
            if (i < j)
            {
                std::swap(data[2*i],   data[2*j]);
                std::swap(data[2*i+1], data[2*j+1]);
            }
            int m = N >> 1;
            while (m >= 1 && j >= m) { j -= m; m >>= 1; }
            j += m;
        }

        // Butterfly stages
        float sign = inverse ? 1.0f : -1.0f;
        for (int step = 1; step < N; step <<= 1)
        {
            float ang = sign * kPi / (float)step;
            float wRe = std::cos(ang), wIm = std::sin(ang);
            for (int grp = 0; grp < N; grp += step * 2)
            {
                float tRe = 1.0f, tIm = 0.0f;
                for (int k = 0; k < step; ++k)
                {
                    int a = grp + k;
                    int b = a + step;
                    float bRe = data[2*b]*tRe - data[2*b+1]*tIm;
                    float bIm = data[2*b]*tIm + data[2*b+1]*tRe;
                    data[2*b]   = data[2*a]   - bRe;
                    data[2*b+1] = data[2*a+1] - bIm;
                    data[2*a]   += bRe;
                    data[2*a+1] += bIm;
                    float newTRe = tRe*wRe - tIm*wIm;
                    tIm = tRe*wIm + tIm*wRe;
                    tRe = newTRe;
                }
            }
        }

        if (inverse)
        {
            float norm = 1.0f / (float)N;
            for (int i = 0; i < 2*N; ++i)
                data[i] *= norm;
        }
    }

    static float wrapPhase(float p)
    {
        while (p >  kPi) p -= kTwoPi;
        while (p < -kPi) p += kTwoPi;
        return p;
    }

    // Pitch-shift a spectrum by ratio R, writing into outSpec
    void pitchShift(const float* mag, const float* phase,
                    float* synthPhase, float ratio,
                    float* outSpec)
    {
        // Clear output spectrum
        std::memset(outSpec, 0, sizeof(float) * 2 * kFFTSize);

        for (int k = 0; k <= kHalfFFT; ++k)
        {
            // Compute instantaneous frequency deviation for bin k
            float dPhase = phase[k] - prevPhase[k];
            float expected = kExpectedPhaseStep * (float)k;
            float deviation = wrapPhase(dPhase - expected);
            // True frequency in radians/sample * hopSize
            float trueFreq = expected + deviation;

            // Map to output bin
            int outBin;
            if (ratio >= 1.0f)
                outBin = (int)(k * ratio + 0.5f);
            else
                outBin = (int)(k * ratio + 0.5f);

            if (outBin < 0 || outBin > kHalfFFT)
                continue;

            // Accumulate synthesis phase at the shifted frequency
            synthPhase[outBin] += trueFreq * ratio;

            // Accumulate magnitude (multiple source bins may map to same output)
            float re = mag[k] * std::cos(synthPhase[outBin]);
            float im = mag[k] * std::sin(synthPhase[outBin]);
            outSpec[2 * outBin]     += re;
            outSpec[2 * outBin + 1] += im;

            // Mirror for negative frequencies
            if (outBin > 0 && outBin < kHalfFFT)
            {
                int mirror = kFFTSize - outBin;
                outSpec[2 * mirror]     += re;
                outSpec[2 * mirror + 1] -= im;
            }
        }
    }

    void processFrame()
    {
        // Gather input frame from circular buffer (most recent kFFTSize samples)
        float frame[kFFTSize];
        for (int i = 0; i < kFFTSize; ++i)
        {
            int idx = (writePos - kFFTSize + i + kBufSize) & kBufMask;
            frame[i] = inputBuf[idx] * window[i];
        }

        // Forward FFT (interleaved complex)
        float spec[2 * kFFTSize];
        for (int i = 0; i < kFFTSize; ++i)
        {
            spec[2*i]   = frame[i];
            spec[2*i+1] = 0.0f;
        }
        fft(spec, kFFTSize, false);

        // Extract magnitude and phase
        float mag[kHalfFFT + 1];
        float phase[kHalfFFT + 1];
        for (int k = 0; k <= kHalfFFT; ++k)
        {
            float re = spec[2*k];
            float im = spec[2*k+1];
            mag[k]   = std::sqrt(re*re + im*im);
            phase[k] = std::atan2(im, re);
        }

        // --- Dry path (passthrough with matched latency) ---
        {
            float drySpec[2 * kFFTSize];
            std::memcpy(drySpec, spec, sizeof(drySpec));
            fft(drySpec, kFFTSize, true);
            int outStart = (writePos) & kBufMask;
            for (int i = 0; i < kFFTSize; ++i)
            {
                int idx = (outStart + i) & kBufMask;
                dryBuf[idx] += drySpec[2*i] * window[i] * kOLANorm;
            }
        }

        // --- Octave up +1 (ratio = 2) ---
        if (highOct1Mix > 1e-5f)
        {
            float outSpec[2 * kFFTSize];
            pitchShift(mag, phase, synthPhaseHi1, 2.0f, outSpec);
            fft(outSpec, kFFTSize, true);
            int outStart = (writePos) & kBufMask;
            for (int i = 0; i < kFFTSize; ++i)
            {
                int idx = (outStart + i) & kBufMask;
                high1Buf[idx] += outSpec[2*i] * window[i] * kOLANorm;
            }
        }

        // --- Octave up +2 (ratio = 4) ---
        if (highOct2Mix > 1e-5f)
        {
            float outSpec[2 * kFFTSize];
            pitchShift(mag, phase, synthPhaseHi2, 4.0f, outSpec);
            fft(outSpec, kFFTSize, true);
            int outStart = (writePos) & kBufMask;
            for (int i = 0; i < kFFTSize; ++i)
            {
                int idx = (outStart + i) & kBufMask;
                high2Buf[idx] += outSpec[2*i] * window[i] * kOLANorm;
            }
        }

        // --- Sub octave -1 (ratio = 0.5) ---
        if (subOct1Mix > 1e-5f)
        {
            float outSpec[2 * kFFTSize];
            pitchShift(mag, phase, synthPhaseSub1, 0.5f, outSpec);
            fft(outSpec, kFFTSize, true);
            int outStart = (writePos) & kBufMask;
            for (int i = 0; i < kFFTSize; ++i)
            {
                int idx = (outStart + i) & kBufMask;
                sub1Buf[idx] += outSpec[2*i] * window[i] * kOLANorm;
            }
        }

        // --- Sub octave -2 (ratio = 0.25) ---
        if (subOct2Mix > 1e-5f)
        {
            float outSpec[2 * kFFTSize];
            pitchShift(mag, phase, synthPhaseSub2, 0.25f, outSpec);
            fft(outSpec, kFFTSize, true);
            int outStart = (writePos) & kBufMask;
            for (int i = 0; i < kFFTSize; ++i)
            {
                int idx = (outStart + i) & kBufMask;
                sub2Buf[idx] += outSpec[2*i] * window[i] * kOLANorm;
            }
        }

        // Save analysis phase for next frame
        std::memcpy(prevPhase, phase, sizeof(prevPhase));
    }
};
