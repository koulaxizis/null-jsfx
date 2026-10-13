/*
 * NULL JSFX - Band Splitter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/band_splitter.jsfx (computed in double, like EEL2).
 * Split: a Linkwitz-Riley 4th-order crossover cascade (each LR4 is two Butterworth TPT
 * state-variable stages) into 2, 3 or 4 bands on channel pairs 1/2, 3/4, 5/6, 7/8; the lower
 * bands go through the allpasses of the crossovers above them so the sum is an allpass.
 * Listen copies one band to 1/2. Merge: the four input pairs summed into 1/2.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullbandsplit {

static const double kPi = 3.141592653589793;

// Butterworth SVF coefficients (svf_coefs in the JSFX)
struct Coefs {
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
    void set(double f, double sr) noexcept
    {
        const double g = std::tan(kPi * f / sr);
        a1 = 1.0 / (1.0 + g * (g + std::sqrt(2.0)));
        a2 = g * a1;
        a3 = g * a2;
    }
};

// one TPT state-variable filter (svf in the JSFX): bp in v1, lp in v2
struct Svf {
    double ic1 = 0.0, ic2 = 0.0, v1 = 0.0, v2 = 0.0;
    void tick(double x, const Coefs& c) noexcept
    {
        const double v3 = x - ic2;
        v1 = c.a1 * ic1 + c.a2 * v3;
        v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
    }
};

// one LR4 crossover (lr4 in the JSFX)
struct Lr4 {
    Svf s, l, h;
    double lo = 0.0, hi = 0.0;
    void tick(double x, const Coefs& c) noexcept
    {
        const double k2 = std::sqrt(2.0);
        s.tick(x, c);
        const double hp = x - k2 * s.v1 - s.v2;
        l.tick(s.v2, c);
        lo = l.v2;
        h.tick(hp, c);
        hi = hp - k2 * h.v1 - h.v2;
    }
};

// second-order allpass with the phase of an LR4 crossover (ap in the JSFX)
struct Allpass {
    Svf s;
    double tick(double x, const Coefs& c) noexcept
    {
        s.tick(x, c);
        return x - 2.0 * std::sqrt(2.0) * s.v1;
    }
};

// one channel (split in the JSFX)
struct Channel {
    Lr4 x1, x2, x3;
    Allpass p2, p3a, p3b;
    double b[4] = {};
    void tick(double x, int nb, const Coefs* c) noexcept
    {
        x1.tick(x, c[0]);
        x2.tick(x1.hi, c[1]);
        x3.tick(x2.hi, c[2]);
        const double a2 = p2.tick(x1.lo, c[1]);
        const double a3a = p3a.tick(a2, c[2]);
        const double a3b = p3b.tick(x2.lo, c[2]);
        if (nb == 2)
        {
            b[0] = x1.lo; b[1] = x1.hi; b[2] = 0.0; b[3] = 0.0;
        }
        else if (nb == 3)
        {
            b[0] = a2; b[1] = x2.lo; b[2] = x2.hi; b[3] = 0.0;
        }
        else
        {
            b[0] = a3a; b[1] = a3b; b[2] = x3.lo; b[3] = x3.hi;
        }
    }
};

struct Params {
    bool merge = false;
    int bands = 3;
    double xover[3] = { 150.0, 2000.0, 7000.0 }; // slider values
    int listen = 0;
};

// the crossovers in use (the @slider limits): each at least 1.5x the one below, the top one
// below 0.45 x the sample rate
inline void effectiveXovers(const double* in, double sr, double* xo) noexcept
{
    xo[0] = std::fmax(30.0, std::fmin(1000.0, in[0]));
    xo[1] = std::fmax(xo[0] * 1.5, std::fmax(200.0, std::fmin(6000.0, in[1])));
    xo[2] = std::fmin(std::fmax(xo[1] * 1.5, std::fmax(1000.0, std::fmin(16000.0, in[2]))), 0.45 * sr);
}

// magnitude (linear) of band b (0..nb-1) at f Hz: products of the LR4 lowpass 1/(1+r^4) and
// highpass r^4/(1+r^4), r the prewarped frequency ratio (the allpasses have no magnitude)
inline double bandMag(int b, int nb, double f, const double* xo, double sr) noexcept
{
    const double w = std::tan(kPi * std::fmin(f, 0.49 * sr) / sr);
    double m = 1.0;
    for (int k = 0; k < nb - 1; ++k)
    {
        const double r = w / std::tan(kPi * xo[k] / sr);
        const double r4 = r * r * r * r;
        if (k == b)
            m *= 1.0 / (1.0 + r4);      // the band below crossover k
        else if (k < b)
            m *= r4 / (1.0 + r4);       // above the crossovers under the band
    }
    return m;
}

class BandSplitterProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; }

    void reset() noexcept // @init
    {
        fL = Channel();
        fR = Channel();
    }

    void setParams(const Params& p) noexcept // @slider
    {
        fMerge = p.merge;
        fBands = p.bands < 2 ? 2 : (p.bands > 4 ? 4 : p.bands);
        double xo[3];
        effectiveXovers(p.xover, fSampleRate, xo);
        for (int k = 0; k < 3; ++k)
            fC[k].set(xo[k], fSampleRate);
        fListen = p.listen;
    }

    // in/out: 8 channels each (they may be the same buffers)
    template <typename T>
    void process(const T* const* in, T* const* out, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double s[8];
            for (int c = 0; c < 8; ++c)
                s[c] = in[c][i];
            if (fMerge)
            {
                s[0] = s[0] + s[2] + s[4] + s[6];
                s[1] = s[1] + s[3] + s[5] + s[7];
                for (int c = 2; c < 8; ++c)
                    s[c] = 0.0;
            }
            else
            {
                fL.tick(s[0], fBands, fC);
                fR.tick(s[1], fBands, fC);
                for (int b = 0; b < 4; ++b)
                {
                    s[2 * b] = fL.b[b];
                    s[2 * b + 1] = fR.b[b];
                }
                if (fListen >= 2 && fListen <= 4)
                {
                    s[0] = s[2 * (fListen - 1)];
                    s[1] = s[2 * (fListen - 1) + 1];
                }
            }
            for (int c = 0; c < 8; ++c)
                out[c][i] = static_cast<T>(s[c]);
        }
    }

private:
    double fSampleRate = 48000.0;
    bool fMerge = false;
    int fBands = 3, fListen = 0;
    Coefs fC[3];
    Channel fL, fR;
};

} // namespace nullbandsplit
