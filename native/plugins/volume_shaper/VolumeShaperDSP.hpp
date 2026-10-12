/*
 * NULL JSFX - Volume Shaper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/volume_shaper.jsfx (computed in double, like EEL2): a
 * preset gain shape (breakpoints) repeated once per Rate note at the host tempo. While the
 * transport plays, the cycle position is taken from the host's beat position at the start of
 * each block (the JSFX's @block); within the block, and while stopped, it advances at the
 * tempo. Smooth is a one-pole lowpass on the gain; Mix blends with the dry signal.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullvolshaper {

static constexpr int kNumShapes = 10;
static constexpr int kHalfTime = 8; // the shape that spans two Rate notes

// one shape: points (x, y, curve of the segment to the next point), x from 0 to 1
struct Shape {
    double x[16] = {}, y[16] = {};
    int c[16] = {};
    int n = 0;
    void pt(double px, double py, int pc) noexcept
    {
        x[n] = px; y[n] = py; c[n] = pc;
        ++n;
    }
};

// the preset shapes, built as in the JSFX's @init
struct Shapes {
    Shape s[kNumShapes];
    Shapes() noexcept
    {
        // Pump, Pump Hard, Saw Down, Saw Up
        s[0].pt(0, 0, 3); s[0].pt(0.5, 1, 0); s[0].pt(0.97, 1, 0); s[0].pt(1, 0, 0);
        s[1].pt(0, 0, 0); s[1].pt(0.1, 0, 1); s[1].pt(0.45, 1, 0); s[1].pt(0.97, 1, 0); s[1].pt(1, 0, 0);
        s[2].pt(0, 1, 0); s[2].pt(0.97, 0, 0); s[2].pt(1, 1, 0);
        s[3].pt(0, 0, 0); s[3].pt(0.97, 1, 0); s[3].pt(1, 0, 0);
        // Gate 1/16
        for (int k = 0; k < 4; ++k)
        {
            const double kk = double(k);
            s[4].pt(kk / 4, 1, 0); s[4].pt(kk / 4 + 0.125, 1, 0);
            s[4].pt(kk / 4 + 0.125, 0, 0); s[4].pt(kk / 4 + 0.25, 0, 0);
        }
        // Trance 1/8
        s[5].pt(0, 1, 0); s[5].pt(0.28, 1, 3); s[5].pt(0.46, 0, 0); s[5].pt(0.5, 0, 0);
        s[5].pt(0.5, 1, 0); s[5].pt(0.78, 1, 3); s[5].pt(0.96, 0, 0); s[5].pt(1, 0, 0);
        // Sine, Triangle, Half-time Pump
        s[6].pt(0, 0, 4); s[6].pt(0.5, 1, 4); s[6].pt(1, 0, 0);
        s[7].pt(0, 0, 0); s[7].pt(0.5, 1, 0); s[7].pt(1, 0, 0);
        s[8].pt(0, 0, 3); s[8].pt(0.5, 1, 0); s[8].pt(0.97, 1, 0); s[8].pt(1, 0, 0);
        // Stutter
        s[9].pt(0, 1, 0);
        for (int k = 0; k < 4; ++k)
        {
            const double kk = double(k);
            s[9].pt(0.5 + kk / 8 + 0.0625, 1, 0); s[9].pt(0.5 + kk / 8 + 0.0625, 0, 0);
            s[9].pt(0.5 + kk / 8 + 0.125, 0, 0);
            if (k < 3)
                s[9].pt(0.5 + kk / 8 + 0.125, 1, 0);
        }
    }

    // the level of shape i at cycle position p (vs_eval in the JSFX)
    double eval(int i, double p) const noexcept
    {
        const Shape& b = s[i < 0 ? 0 : (i >= kNumShapes ? kNumShapes - 1 : i)];
        int k = 0;
        while (k < b.n - 2 && p >= b.x[k + 1])
            ++k;
        const double x0 = b.x[k], y0 = b.y[k], x1 = b.x[k + 1], y1 = b.y[k + 1];
        const int c = b.c[k];
        double t = x1 > x0 ? (p - x0) / (x1 - x0) : 1.0;
        const double u = 1.0 - t;
        t = c == 1 ? t * t : c == 2 ? 1.0 - u * u : c == 3 ? 1.0 - u * u * u
          : c == 4 ? 0.5 - 0.5 * std::cos(3.141592653589793 * t) : t;
        return y0 + (y1 - y0) * t;
    }
};

inline const Shapes& shapes() noexcept
{
    static const Shapes kShapes;
    return kShapes;
}

// the cycle in beats for a Rate index (1/1 .. 1/8T) and shape
inline double cycleBeats(int rate, int shape) noexcept
{
    double cyc = rate == 0 ? 4.0 : rate == 1 ? 2.0 : rate == 2 ? 1.0 : rate == 3 ? 0.5 : rate == 4 ? 0.25
               : rate == 5 ? 2.0 / 3.0 : 1.0 / 3.0;
    if (shape == kHalfTime)
        cyc *= 2.0;
    return cyc;
}

struct Params {
    int shape = 0, rate = 2;
    double depthPct = 80.0, smoothMs = 3.0, phasePct = 0.0, mixPct = 100.0;
};

class VolumeShaperProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; }

    void reset() noexcept // @init
    {
        fPos = 0.0;
        fG = 1.0;
    }

    void setParams(const Params& p) noexcept // @slider
    {
        fShape = p.shape;
        fCyc = cycleBeats(p.rate, p.shape);
        fDepth = p.depthPct / 100.0;
        fSm = p.smoothMs > 0.0 ? std::exp(-1.0 / (fSampleRate * p.smoothMs * 0.001)) : 0.0;
        fPhase = p.phasePct / 100.0;
        fMix = p.mixPct / 100.0;
    }

    double position() const noexcept { return fPos; }
    // the shape's smoothed gain now (before Mix)
    double gain() const noexcept { return fG; }

    // playing: host transport running; beatPos: beat position at the block start; tempo in BPM
    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, bool playing, double beatPos,
                 double tempo) noexcept
    {
        const Shapes& sh = shapes();
        const double bpm = tempo > 0.0 ? tempo : 120.0;
        const double inc = bpm / 60.0 / fCyc / fSampleRate;
        if (playing)
            fPos = beatPos / fCyc - std::floor(beatPos / fCyc);
        double pos = fPos, g = fG;
        for (uint32_t i = 0; i < frames; ++i)
        {
            pos += inc;
            if (pos >= 1.0)
                pos -= 1.0;
            double p = pos + fPhase;
            if (p >= 1.0)
                p -= 1.0;
            const double target = 1.0 - fDepth * (1.0 - sh.eval(fShape, p));
            g = target + fSm * (g - target);
            const double m = 1.0 - fMix + fMix * g;
            const double l = inL[i], r = inR[i];
            outL[i] = static_cast<T>(l * m);
            outR[i] = static_cast<T>(r * m);
        }
        fPos = pos;
        fG = g;
    }

private:
    double fSampleRate = 48000.0;
    int fShape = 0;
    double fCyc = 1.0, fDepth = 0.8, fSm = 0.0, fPhase = 0.0, fMix = 1.0;
    double fPos = 0.0, fG = 1.0;
};

} // namespace nullvolshaper
