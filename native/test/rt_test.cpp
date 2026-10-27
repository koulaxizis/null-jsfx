/*
 * NULL JSFX - realtime-safety tests of the DSP cores (no host, no ysfx)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Slicer and Half Speed: reset() (REAPER's @init when the transport starts) only marks the bar
 * buffers as cleared (NullLazyClear.hpp). Each is rendered through transport starts mid-bar,
 * song-position jumps, stops and tempo changes, once with that reset() and once with a full
 * re-initialisation (setSampleRate: the buffers allocated and cleared, as before), and the two
 * outputs must be identical, bit for bit. The longest reset() is printed.
 */

#include "SlicerDSP.hpp"
#include "HalfSpeedDSP.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int gFailures = 0;

static void check(bool ok, const char* what)
{
    std::printf("  %s: %s\n", ok ? "ok" : "FAILED", what);
    if (!ok)
        ++gFailures;
}

// Park-Miller, so the scenario is the same everywhere
struct Rng {
    double s = 4242.0;
    double next()
    {
        s *= 16807.0;
        s -= std::floor(s / 2147483647.0) * 2147483647.0;
        return s / 2147483647.0;
    }
};

// music-like input: a decaying tone per beat plus noise
static void signal(std::vector<float>& l, std::vector<float>& r, double sr, size_t n)
{
    Rng g;
    l.resize(n);
    r.resize(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = double(i) / sr, ph = std::fmod(t, 0.5);
        const double tone = std::sin(2.0 * M_PI * 110.0 * t) * std::exp(-ph * 8.0);
        l[i] = float(0.5 * tone + 0.1 * (g.next() * 2.0 - 1.0));
        r[i] = float(0.5 * tone + 0.1 * (g.next() * 2.0 - 1.0));
    }
}

// one transport scenario: (start sample, playing, tempo, beat position at that sample)
struct Segment {
    size_t start;
    bool playing;
    double tempo, beat;
};

static std::vector<Segment> scenario(double sr)
{
    const size_t s = size_t(sr);
    return {
        { 0, false, 120.0, 0.0 },         // stopped: free-runs, fills the buffers
        { 3 * s, true, 120.0, 2.37 },     // play from the middle of bar 1
        { 7 * s, true, 120.0, 21.0 },     // a jump of the song position
        { 9 * s, false, 90.0, 0.0 },      // stop, slower
        { 11 * s, true, 90.0, 0.0 },      // play from bar 1
        { 14 * s, false, 150.0, 0.0 },    // stop, faster
        { 15 * s, true, 60.0, 5.5 },      // play mid-bar at 60 bpm: longer bars than recorded before
        { 20 * s, true, 60.0, 5.5 },
    };
}

template <typename Dsp, typename Params>
static bool render(double sr, const Params& p, bool lazy, std::vector<float>& outL, std::vector<float>& outR,
                   double& worstResetMs)
{
    const auto segs = scenario(sr);
    const size_t n = segs.back().start;
    std::vector<float> inL, inR;
    signal(inL, inR, sr, n);
    outL.assign(n, 0.0f);
    outR.assign(n, 0.0f);
    Dsp dsp;
    dsp.setSampleRate(sr);
    dsp.setParams(p);
    bool was = false;
    const uint32_t block = 256;
    for (size_t k = 0; k + 1 < segs.size(); ++k)
    {
        const Segment& sg = segs[k];
        if (sg.playing && !was)
        {
            const auto t0 = std::chrono::steady_clock::now();
            if (lazy)
                dsp.reset();
            else
                dsp.setSampleRate(sr); // the old reset(): buffers cleared in full
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            if (lazy && ms > worstResetMs)
                worstResetMs = ms;
        }
        was = sg.playing;
        for (size_t pos = sg.start; pos < segs[k + 1].start; pos += block)
        {
            const uint32_t f = uint32_t(std::min<size_t>(block, segs[k + 1].start - pos));
            const double beat = sg.beat + double(pos - sg.start) / sr * sg.tempo / 60.0;
            dsp.process(inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, f, sg.tempo,
                        sg.playing, sg.playing ? beat : 0.0);
        }
    }
    return true;
}

template <typename Dsp, typename Params>
static void lazyClear(const char* name, const std::vector<Params>& settings)
{
    for (double sr : { 44100.0, 96000.0 })
    {
        double worst = 0.0;
        bool same = true;
        for (const Params& p : settings)
        {
            std::vector<float> aL, aR, bL, bR;
            render<Dsp>(sr, p, true, aL, aR, worst);
            render<Dsp>(sr, p, false, bL, bR, worst);
            same = same && std::memcmp(aL.data(), bL.data(), aL.size() * sizeof(float)) == 0 &&
                   std::memcmp(aR.data(), bR.data(), aR.size() * sizeof(float)) == 0;
        }
        char what[160];
        std::snprintf(what, sizeof(what), "%s @%g: lazy clear = full clear, bit for bit (longest reset %.3f ms)",
                      name, sr, worst);
        check(same, what);
    }
}

int main()
{
    std::printf("rt_test\n");

    std::vector<nullslicer::Params> sl(4);
    sl[1].pattern = 4; // Backspin
    sl[1].variation = 80.0;
    sl[2].pattern = 2; // Reverse Pairs, 32 slices, gated
    sl[2].slices = 2;
    sl[2].gatePct = 60.0;
    sl[3].pattern = 5; // Random
    sl[3].reversePct = 50.0;
    sl[3].mixPct = 70.0;
    lazyClear<nullslicer::SlicerProcessor>("Slicer", sl);

    std::vector<nullhalfspeed::Params> hs(4);
    hs[1].keep = true;
    hs[2].length = 3; // 2 bars at 1/4: a long cycle
    hs[2].speed = 1;
    hs[3].length = 0;
    hs[3].speed = 2;
    hs[3].toneHz = 4000.0;
    hs[3].offset = 1.0;
    lazyClear<nullhalfspeed::HalfSpeedProcessor>("Half Speed", hs);

    std::printf(gFailures ? "rt_test: %d FAILED\n" : "rt_test: all passed\n", gFailures);
    return gFailures ? 1 : 0;
}
