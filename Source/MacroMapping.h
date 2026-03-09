#pragma once
#include <cmath>

namespace MacroMapping
{

// === Response macro (0-1) → four timing values ===

// Lo band attack: 50ms → 1ms
static inline float responseLoAtk(float pct)
{
    return 50.0f * std::pow(1.0f / 50.0f, pct);
}

// Lo band release: 400ms → 15ms
static inline float responseLoRel(float pct)
{
    return 400.0f * std::pow(15.0f / 400.0f, pct);
}

// Hi band attack: 20ms → 0.5ms
static inline float responseHiAtk(float pct)
{
    return 20.0f * std::pow(0.5f / 20.0f, pct);
}

// Hi band release: 120ms → 5ms
static inline float responseHiRel(float pct)
{
    return 120.0f * std::pow(5.0f / 120.0f, pct);
}

// === Clarity macro (0-1) → four values ===

// Noise blend: gentle ramp, 0 → 0.20
// Reduced from 0.45 max — the de-esser now handles sibilant-specific control,
// so the global noise floor can be much lower.
static inline float clarityNoise(float pct)
{
    return pct * pct * 0.12f + pct * 0.08f;
}

// Unvoiced sensitivity: linear 0.1 → 0.65
// Reduced max from 0.9 → 0.65 to prevent false triggering on bright vowels.
// Only strong sibilants (s, sh, f) should trigger the unvoiced detector.
static inline float clarityUvSens(float pct)
{
    return 0.10f + pct * 0.55f;
}

// Voice dynamics: sigmoid 0 → 1
static inline float clarityDynamics(float pct)
{
    return 1.0f / (1.0f + std::exp(-8.0f * (pct - 0.5f)));
}

// Hi boost: quadratic 0dB → 10dB
static inline float clarityHiBoost(float pct)
{
    return pct * pct * 10.0f;
}

// De-ess amount: 0 → 0.75
// Linear ramp — at full clarity, 75% de-essing.
static inline float clarityDeEss(float pct)
{
    return pct * 0.75f;
}

} // namespace MacroMapping
