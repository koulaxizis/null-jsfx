/*
 * NULL JSFX - audio taps the DSP fills and the UI reads (direct access, same process)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Lock-free: the audio thread writes, the UI only reads. A torn read just draws one odd
 * frame, which is fine for meters and scopes. Mirrors ng_tap / ng_meter in null_gfx.jsfx-inc.
 */

#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

namespace nulljsfx {

inline float toDb(float a) noexcept { return a > 1e-7f ? 20.0f * std::log10(a) : -140.0f; }

struct Taps {
    static constexpr uint32_t kScopeLen = 2048;

    // level values in linear amplitude (meters convert with toDb); gr in dB (positive = reduction)
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, gr { 0.0f }, extra { 0.0f };

    // stereo ring buffer of the latest samples
    float scopeL[kScopeLen] = {}, scopeR[kScopeLen] = {};
    std::atomic<uint32_t> scopePos { 0 };

    void push(float l, float r) noexcept
    {
        const uint32_t p = scopePos.load(std::memory_order_relaxed);
        scopeL[p] = l;
        scopeR[p] = r;
        scopePos.store((p + 1) % kScopeLen, std::memory_order_release);
    }
};

// Peak follower with instant attack and exponential release (like the JSFX meters).
struct PeakFollower {
    float value = 0.0f, release = 0.9995f;
    void setup(double sampleRate, double releaseSec = 0.3) noexcept
    {
        release = float(std::exp(-1.0 / (releaseSec * sampleRate)));
    }
    float process(float x) noexcept
    {
        x = std::fabs(x);
        value = x > value ? x : value * release;
        return value;
    }
};

} // namespace nulljsfx
