/*
 * NULL JSFX - Comb Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/comb_filter.jsfx, computed in double like EEL2: a copy of the
 * signal delayed by one period of Frequency (linear interpolation) is added (Feed-Forward) or
 * recirculated (Feed-Backward), then level-compensated by the comb's peak gain and Gain.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullcomb {

static constexpr double kPi = 3.141592653589793;

struct Coeffs {
    double delay = 1.0, fb = 0.5, gain = 1.0;
    int mode = 0;
};

// the JSFX's @slider for a buffer of bufSize samples
inline Coeffs coeffs(double sr, int bufSize, double freq, double fb, int mode, double gainDb) noexcept
{
    Coeffs c;
    c.fb = fb;
    c.mode = mode;
    c.delay = std::max(1.0, std::min(double(bufSize - 2), sr / freq));
    const double norm = mode == 0 ? 1.0 / (1.0 + fb) : (1.0 - fb);
    c.gain = std::pow(10.0, gainDb / 20.0) * norm;
    return c;
}

inline int bufferSize(double sr) noexcept { return int(std::ceil(sr / 20.0)) + 4; }

// display: response in dB at f (the comb without the interpolation), as cf_db in the GFX
inline double responseDb(const Coeffs& c, double f, double sr) noexcept
{
    const double th = 2.0 * kPi * f / sr * c.delay;
    const double m = c.mode == 0 ? 1.0 + 2.0 * c.fb * std::cos(th) + c.fb * c.fb
                                 : 1.0 / (1.0 - 2.0 * c.fb * std::cos(th) + c.fb * c.fb);
    return 10.0 * std::log10(std::max(m, 1e-10)) + 20.0 * std::log10(std::max(c.gain, 1e-5));
}

class CombProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = bufferSize(sr);
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
    }

    // @slider
    void setParams(double freq, double fb, int mode, double gainDb)
    {
        fC = coeffs(fSr, fSize, freq, fb, mode, gainDb);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            const double dl = tap(fL), dr = tap(fR);
            const double ol = xl + fC.fb * dl;
            const double orr = xr + fC.fb * dr;
            fL[size_t(fW)] = fC.mode == 0 ? xl : ol;
            fR[size_t(fW)] = fC.mode == 0 ? xr : orr;
            if (++fW >= fSize)
                fW = 0;
            outL[n] = static_cast<T>(ol * fC.gain);
            outR[n] = static_cast<T>(orr * fC.gain);
        }
    }

private:
    double tap(const std::vector<double>& buf) const
    {
        double rp = double(fW) - fC.delay;
        if (rp < 0.0)
            rp += fSize;
        const double fi = std::floor(rp);
        const double fr = rp - fi;
        const int i = int(fi);
        int i2 = i + 1;
        if (i2 >= fSize)
            i2 = 0;
        return buf[size_t(i)] + (buf[size_t(i2)] - buf[size_t(i)]) * fr;
    }

    double fSr = 48000.0;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0;
    Coeffs fC;
};

} // namespace nullcomb
