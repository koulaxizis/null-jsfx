// Deterministic stereo test signal shared by clap_render and ysfx_render:
// a 3-sine chord plus LCG white noise, different per channel.
#pragma once
#include <cmath>
#include <cstdint>

struct TestSignal {
    uint32_t seed = 12345;
    uint64_t n = 0;
    double sr = 48000.0;
    float noise()
    {
        seed = seed * 1664525u + 1013904223u;
        return (float)((int32_t)seed) * (1.0f / 2147483648.0f);
    }
    void next(float &l, float &r)
    {
        double t = (double)n++ / sr;
        double s = 0.25 * std::sin(2 * M_PI * 110 * t) + 0.15 * std::sin(2 * M_PI * 1250 * t) + 0.1 * std::sin(2 * M_PI * 7000 * t);
        l = (float)s + 0.1f * noise();
        r = (float)(0.8 * s) + 0.1f * noise();
    }
};
