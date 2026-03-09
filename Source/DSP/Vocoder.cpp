#include "Vocoder.h"

void Vocoder::prepare(double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    bands.resize(kMaxBands);

    for (auto& band : bands)
    {
        band.envelopeFollower.prepare(sampleRate);
        band.modulatorFilter1.reset();
        band.modulatorFilter2.reset();
        band.carrierFilter1.reset();
        band.carrierFilter2.reset();
        band.noiseFilter1.reset();
        band.noiseFilter2.reset();
    }

    // Noise high-pass at 3kHz for consonant detection
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 3000.0f);
    noiseHighPass.coefficients = hpCoeffs;
    noiseHighPass.reset();

    noiseEnvFollower.prepare(sampleRate);
    noiseEnvFollower.setAttack(1.0f);
    noiseEnvFollower.setRelease(10.0f);

    pinkB0 = pinkB1 = pinkB2 = pinkB3 = pinkB4 = pinkB5 = pinkB6 = 0.0f;

    // Voice input normalization: ~50ms window
    voiceRmsAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.05));
    voiceRmsSq = 0.0f;

    // Output leveler: ~300ms slow window
    levelerAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.3));
    levelerGainAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.05));  // ~50ms smoothing
    dryVoiceRmsSq = 0.0f;
    wetRmsSq = 0.0f;
    smoothedLevelerGain = 1.0f;

    needsRebuild = true;
    rebuildFilterBank();

    // Prime all band filters with quiet noise to prevent cold-start ringing.
    // Run ~10ms of -60dB pink noise through every filter path.
    int primeSamples = (int)(sr * 0.010);
    for (int s = 0; s < primeSamples; ++s)
    {
        float noise = generatePinkNoise() * 0.001f;
        for (int i = 0; i < numBands; ++i)
        {
            bands[(size_t)i].modulatorFilter1.processSample(noise);
            bands[(size_t)i].modulatorFilter2.processSample(noise);
            bands[(size_t)i].carrierFilter1.processSample(noise);
            bands[(size_t)i].carrierFilter2.processSample(noise);
            bands[(size_t)i].noiseFilter1.processSample(noise);
            bands[(size_t)i].noiseFilter2.processSample(noise);
        }
    }

    // Seed voice dynamics RMS tracker so it doesn't start at zero
    voiceRmsSq = 1e-5f;  // ~-50dB

    onsetRampSamples = 0;
}

void Vocoder::setNumBands(int n)
{
    if (n != numBands)
    {
        numBands = juce::jlimit(8, kMaxBands, n);
        needsRebuild = true;
    }
}

void Vocoder::setAttack(float ms)  { attackMs = ms; }
void Vocoder::setRelease(float ms) { releaseMs = ms; }
void Vocoder::setNoiseAmount(float amount) { noiseAmount = amount; }
void Vocoder::setDryWet(float mix) { dryWet = mix; }
void Vocoder::setVoiceDynamics(float amount) { voiceDynamics = amount; }

void Vocoder::setHiAttack(float ms)  { hiAttackMs = ms; }
void Vocoder::setHiRelease(float ms) { hiReleaseMs = ms; }

void Vocoder::applyEnvelopeSettings()
{
    for (int i = 0; i < numBands; ++i)
    {
        if (splitEnvOn && i >= bandSplitIndex)
        {
            bands[(size_t)i].envelopeFollower.setAttack(hiAttackMs);
            bands[(size_t)i].envelopeFollower.setRelease(hiReleaseMs);
        }
        else
        {
            bands[(size_t)i].envelopeFollower.setAttack(attackMs);
            bands[(size_t)i].envelopeFollower.setRelease(releaseMs);
        }

    }
}

void Vocoder::rebuildFilterBank()
{
    if (!needsRebuild) return;
    needsRebuild = false;

    activeBandCount.store(numBands);

    float logMin = std::log(kMinFreq);
    float logMax = std::log(kMaxFreq);

    bandSplitIndex = numBands;

    for (int i = 0; i < numBands; ++i)
    {
        float t = (float)i / (float)(numBands - 1);
        float centerFreq = std::exp(logMin + t * (logMax - logMin));

        if (centerFreq >= 1000.0f && bandSplitIndex == numBands)
            bandSplitIndex = i;

        float bandwidth = (logMax - logMin) / (float)numBands;
        float Q = 1.0f / (2.0f * std::sinh(bandwidth * 0.5f));
        Q = juce::jlimit(0.5f, 20.0f, Q);

        auto bpCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, centerFreq, Q);

        bands[(size_t)i].modulatorFilter1.coefficients = bpCoeffs;
        bands[(size_t)i].modulatorFilter2.coefficients = bpCoeffs;
        bands[(size_t)i].carrierFilter1.coefficients = bpCoeffs;
        bands[(size_t)i].carrierFilter2.coefficients = bpCoeffs;
        bands[(size_t)i].noiseFilter1.coefficients = bpCoeffs;
        bands[(size_t)i].noiseFilter2.coefficients = bpCoeffs;
        bands[(size_t)i].modulatorFilter1.reset();
        bands[(size_t)i].modulatorFilter2.reset();
        bands[(size_t)i].carrierFilter1.reset();
        bands[(size_t)i].carrierFilter2.reset();
        bands[(size_t)i].noiseFilter1.reset();
        bands[(size_t)i].noiseFilter2.reset();
        bands[(size_t)i].envelopeFollower.reset();
    }

    // Clear unused band visualizer data
    for (int i = numBands; i < kMaxBands; ++i)
    {
        carrierEnvelopes[(size_t)i].store(0.0f);
        modulatorEnvelopes[(size_t)i].store(0.0f);
        outputEnvelopes[(size_t)i].store(0.0f);
    }

    // Prime rebuilt filters with quiet noise to prevent cold-start ringing
    int primeSamples = (int)(sr * 0.010);
    for (int s = 0; s < primeSamples; ++s)
    {
        float noise = generatePinkNoise() * 0.001f;
        for (int i = 0; i < numBands; ++i)
        {
            bands[(size_t)i].modulatorFilter1.processSample(noise);
            bands[(size_t)i].modulatorFilter2.processSample(noise);
            bands[(size_t)i].carrierFilter1.processSample(noise);
            bands[(size_t)i].carrierFilter2.processSample(noise);
            bands[(size_t)i].noiseFilter1.processSample(noise);
            bands[(size_t)i].noiseFilter2.processSample(noise);
        }
    }
}

float Vocoder::generatePinkNoise()
{
    float white = rng.nextFloat() * 2.0f - 1.0f;

    // Paul Kellet's refined method
    pinkB0 = 0.99886f * pinkB0 + white * 0.0555179f;
    pinkB1 = 0.99332f * pinkB1 + white * 0.0750759f;
    pinkB2 = 0.96900f * pinkB2 + white * 0.1538520f;
    pinkB3 = 0.86650f * pinkB3 + white * 0.3104856f;
    pinkB4 = 0.55000f * pinkB4 + white * 0.5329522f;
    pinkB5 = -0.7616f * pinkB5 - white * 0.0168980f;

    float pink = pinkB0 + pinkB1 + pinkB2 + pinkB3 + pinkB4 + pinkB5 + pinkB6 + white * 0.5362f;
    pinkB6 = white * 0.115926f;

    return pink * 0.11f; // Normalize
}

float Vocoder::process(float voiceSample, float preEQVoice, float guitarSample, float unvoicedAmt)
{
    if (needsRebuild)
        rebuildFilterBank();

    // --- Voice dynamics control ---
    // 0 = raw voice (expressive), 1 = fully normalized (consistent)
    voiceRmsSq += voiceRmsAlpha * (preEQVoice * preEQVoice - voiceRmsSq);
    float voiceRms = std::sqrt(voiceRmsSq);

    float normalizedVoice;
    if (voiceRms > kRmsFloor)
    {
        // Cap normalization gain to prevent onset spikes.
        // Without this cap, voice arriving after silence gets amplified 50x+
        // because the RMS tracker hasn't caught up yet.
        float normGain = juce::jmin(1.0f / voiceRms, kMaxNormGain);
        normalizedVoice = preEQVoice * normGain;
    }
    else
    {
        normalizedVoice = preEQVoice;
    }

    float normVoice = preEQVoice * (1.0f - voiceDynamics) + normalizedVoice * voiceDynamics;

    // Track dry voice RMS for output leveler target
    dryVoiceRmsSq += levelerAlpha * (voiceSample * voiceSample - dryVoiceRmsSq);

    // Consonant noise: high-pass normalized voice, follow envelope, gate pink noise
    float voiceHP = noiseHighPass.processSample(normVoice);
    float noiseEnv = noiseEnvFollower.process(voiceHP);

    // De-ess: when sibilant energy dominates (high HF-to-broadband ratio),
    // reduce noise injection to prevent S sounds from being too prominent.
    // The noiseEnv tracks 3kHz+ energy, voiceRms tracks broadband energy.
    // During S sounds this ratio spikes; we use it to pull down noise gain.
    // De-ess: continuous amount controls threshold and reduction depth.
    // At amount=0, no effect. At amount=1, aggressive sibilant reduction.
    float deEssGain = 1.0f;
    if (deEssAmount > 0.001f && voiceRms > kRmsFloor)
    {
        float hfRatio = noiseEnv / voiceRms;
        // Threshold scales from 0.5 (gentle) to 0.1 (aggressive) with amount
        float threshold = 0.5f - deEssAmount * 0.4f;
        if (hfRatio > threshold)
        {
            float range = 0.45f - deEssAmount * 0.2f;  // narrower range at high amounts
            float excess = (hfRatio - threshold) / juce::jmax(0.05f, range);
            float minGain = 0.5f - deEssAmount * 0.4f;  // floor from 0.5 down to 0.1
            deEssGain = juce::jlimit(minGain, 1.0f, 1.0f - excess * (1.0f - minGain));
        }
    }

    float noise = generatePinkNoise() * noiseEnv * noiseAmount * deEssGain;

    // Mix noise into carrier
    float carrier = guitarSample + noise;

    // Vocoding: apply voice spectral shape to guitar
    float output = 0.0f;
    int firstBypassBand = numBands - unvoicedBands;

    for (int i = 0; i < numBands; ++i)
    {
        // Cascaded 4th-order bandpass: two biquads in series per path
        // Use normalized voice for envelope detection
        float modFiltered = bands[(size_t)i].modulatorFilter1.processSample(normVoice);
        modFiltered = bands[(size_t)i].modulatorFilter2.processSample(modFiltered);

        float env = bands[(size_t)i].envelopeFollower.process(modFiltered);

        // De-ess: attenuate high-band envelopes during sibilants.
        // Only affects bands above the split index (~1kHz+).
        // Lower bands pass through at full level so vowels are unaffected.
        if (deEssAmount > 0.001f && i >= bandSplitIndex)
        {
            float bandT = (float)(i - bandSplitIndex) / (float)juce::jmax(1, numBands - bandSplitIndex - 1);
            float bandDeEss = 1.0f - bandT * (1.0f - deEssGain);
            env *= bandDeEss;
        }

        float carFiltered = bands[(size_t)i].carrierFilter1.processSample(carrier);
        carFiltered = bands[(size_t)i].carrierFilter2.processSample(carFiltered);

        if (unvoicedOn && i >= firstBypassBand && unvoicedAmt > 0.1f)
        {
            // Generate pink noise, filter through this band's dedicated noise BPF
            // so it only fills this band's frequency range
            float rawNoise = generatePinkNoise();
            float bandNoise = bands[(size_t)i].noiseFilter1.processSample(rawNoise);
            bandNoise = bands[(size_t)i].noiseFilter2.processSample(bandNoise);
            // Scale by voice envelope (noise only as loud as voice energy)
            // and 0.5 overall to keep it subtle
            float noiseSample = bandNoise * env * 0.5f * deEssGain;
            float vocoded = carFiltered * env;
            output += vocoded * (1.0f - unvoicedAmt) + noiseSample * unvoicedAmt;
        }
        else
        {
            output += carFiltered * env;
        }

        // Update visualizer — store per-band envelope values for UI
        // Carrier: peak-hold with ~10ms decay so it tracks like a smooth envelope
        float absCar = std::abs(carFiltered);
        float& cDisp = bands[(size_t)i].carrierDisplayEnv;
        if (absCar > cDisp)
            cDisp = absCar;
        else
            cDisp *= 0.9995f;
        carrierEnvelopes[(size_t)i].store(cDisp, std::memory_order_relaxed);
        modulatorEnvelopes[(size_t)i].store(env, std::memory_order_relaxed);
        outputEnvelopes[(size_t)i].store(cDisp * env, std::memory_order_relaxed);
    }

    // Static gain compensation
    float gain = 2.0f * std::sqrt((float)numBands / 16.0f);
    output *= gain;

    // --- Output RMS leveler ---
    // Slow-acting leveler targets dry voice RMS so dry/wet crossfades without volume jumps
    if (outputLevelerOn)
    {
        wetRmsSq += levelerAlpha * (output * output - wetRmsSq);
        float dryRms = std::sqrt(dryVoiceRmsSq);
        float wetRms = std::sqrt(wetRmsSq);
        if (wetRms > kRmsFloor && dryRms > kRmsFloor)
        {
            float targetGain = juce::jlimit(0.1f, 10.0f, dryRms / wetRms);
            smoothedLevelerGain += levelerGainAlpha * (targetGain - smoothedLevelerGain);
        }
        output *= smoothedLevelerGain;
    }

    // Soft onset ramp: fade in vocoder output over first ~5ms
    // to catch any residual cold-start transients
    if (onsetRampSamples < kOnsetRampLength)
    {
        float ramp = (float)onsetRampSamples / (float)kOnsetRampLength;
        output *= ramp;
        onsetRampSamples++;
    }

    // Safety limiter: soft-clip output to prevent compounding gain stages
    // from exceeding ±1.0 (voice dynamics 4x + band gain + leveler 10x)
    output = std::tanh(output);

    // Dry/wet mix
    return voiceSample * (1.0f - dryWet) + output * dryWet;
}

void Vocoder::reset()
{
    for (auto& band : bands)
    {
        band.modulatorFilter1.reset();
        band.modulatorFilter2.reset();
        band.carrierFilter1.reset();
        band.carrierFilter2.reset();
        band.noiseFilter1.reset();
        band.noiseFilter2.reset();
        band.envelopeFollower.reset();
        band.carrierDisplayEnv = 0.0f;
    }
    noiseHighPass.reset();
    noiseEnvFollower.reset();
    pinkB0 = pinkB1 = pinkB2 = pinkB3 = pinkB4 = pinkB5 = pinkB6 = 0.0f;
    voiceRmsSq = 1e-5f;  // match prepare() seeding to prevent onset spike
    dryVoiceRmsSq = 0.0f;
    wetRmsSq = 0.0f;
    smoothedLevelerGain = 1.0f;
    onsetRampSamples = 0;
}
