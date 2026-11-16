/*
 * NULL JSFX - Echo One, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/echo_one.jsfx (computed in double, like EEL2): a one-knob delay
 * locked to the tempo. Amount steps the time from 1/16 through 1/8 and dotted 1/8 to 1/4 and
 * raises the feedback, the wet level and the ping-pong spread while the repeats get darker.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullechoone {

struct Params {
    double note, fb, wet, dry, pp, lpc;
};

// the note length (beats) for an Amount from 0 to 100
inline double beatsFor(double v) noexcept { return v < 25.0 ? 0.25 : v < 50.0 ? 0.5 : v < 75.0 ? 0.75 : 1.0; }

inline const char* nameFor(double v) noexcept
{
    return v < 25.0 ? "1/16" : v < 50.0 ? "1/8" : v < 75.0 ? "1/8 DOTTED" : "1/4";
}

// @slider
inline Params paramsFor(double slider, double sr) noexcept
{
    const double a = slider / 100.0;
    Params p;
    p.note = beatsFor(slider);
    p.fb = 0.12 + a * 0.5;
    p.wet = a * 0.5;
    p.dry = 1.0 - a * 0.25;
    p.pp = std::min(1.0, std::max(0.0, (a - 0.25) * 2.0));
    p.lpc = 1.0 - std::exp(-2.0 * M_PI * std::min(7000.0 - a * 4000.0, 0.45 * sr) / sr);
    return p;
}

class EchoOneProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fN = int(std::ceil(sr * 2.0)) + 4;
        fBl.assign(size_t(fN), 0.0);
        fBr.assign(size_t(fN), 0.0);
        fWp = 0;
        fDt = -1.0;
        fGlide = 1.0 - std::exp(-1.0 / (0.08 * sr));
        fHpc = 1.0 - std::exp(-2.0 * M_PI * 150.0 / sr);
        fLpl = fLpr = fHpl = fHpr = 0.0;
        fP = paramsFor(fAmount, sr);
    }

    void setAmount(double v) noexcept
    {
        fAmount = v;
        fP = paramsFor(v, fSr);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo) noexcept
    {
        const Params p = fP;
        const double n = double(fN);
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double in0 = inL[i], in1 = inR[i];
            const double t = std::min(n - 3.0, p.note * 60.0 / (tempo > 0.0 ? tempo : 120.0) * fSr);
            if (fDt < 0.0)
                fDt = t;
            else
                fDt += (t - fDt) * fGlide;
            double rp = double(fWp) - fDt;
            if (rp < 0.0)
                rp += n;
            const int i0 = int(std::floor(rp));
            const double fr = rp - double(i0);
            int i1 = i0 + 1;
            if (i1 >= fN)
                i1 = 0;
            const double yl = fBl[size_t(i0)] + (fBl[size_t(i1)] - fBl[size_t(i0)]) * fr;
            const double yr = fBr[size_t(i0)] + (fBr[size_t(i1)] - fBr[size_t(i0)]) * fr;
            fLpl += (yl - fLpl) * p.lpc;
            fLpr += (yr - fLpr) * p.lpc;
            fHpl += (fLpl - fHpl) * fHpc;
            fHpr += (fLpr - fHpr) * fHpc;
            const double fl = fLpl - fHpl;
            const double fr2 = fLpr - fHpr;
            const double m = (in0 + in1) * 0.5;
            fBl[size_t(fWp)] = in0 * (1.0 - p.pp) + m * p.pp + (fl * (1.0 - p.pp) + fr2 * p.pp) * p.fb;
            fBr[size_t(fWp)] = in1 * (1.0 - p.pp) + (fr2 * (1.0 - p.pp) + fl * p.pp) * p.fb;
            fWp += 1;
            if (fWp >= fN)
                fWp = 0;
            outL[i] = static_cast<T>(in0 * p.dry + fl * p.wet);
            outR[i] = static_cast<T>(in1 * p.dry + fr2 * p.wet);
        }
    }

private:
    double fSr = 44100.0, fAmount = 40.0;
    Params fP = paramsFor(40.0, 44100.0);
    std::vector<double> fBl, fBr;
    int fN = 1, fWp = 0;
    double fDt = -1.0, fGlide = 0.0, fHpc = 0.0;
    double fLpl = 0.0, fLpr = 0.0, fHpl = 0.0, fHpr = 0.0;
};

} // namespace nullechoone
