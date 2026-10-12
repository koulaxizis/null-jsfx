/*
 * NULL JSFX - Kick Shaper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/kick_shaper.jsfx, computed in double like EEL2: three bells
 * (Thump at 55 Hz, Body at 120 Hz, Click at 4 kHz), an attack shaper driven by how far a
 * fast envelope runs ahead of a slow one, and a decay that fades the tail of every detected
 * hit after 20 ms.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

namespace nullkick {

struct Params {
    double attackDb = 0.0, bodyDb = 0.0, thumpDb = 0.0, clickDb = 0.0, decayMs = 250.0, outDb = 0.0;
};

// RBJ peaking bell, as the JSFX's peak(); runs two channels
struct Bell {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double xl1 = 0, xl2 = 0, yl1 = 0, yl2 = 0, xr1 = 0, xr2 = 0, yr1 = 0, yr2 = 0;

    void set(double sr, double freq, double q, double gain)
    {
        const double w = 2.0 * M_PI * freq / sr, c = std::cos(w), al = std::sin(w) / (2.0 * q);
        const double A = std::pow(10.0, gain / 40.0);
        const double a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0; b1 = -2.0 * c / a0; b2 = (1.0 - al * A) / a0;
        a1 = -2.0 * c / a0; a2 = (1.0 - al / A) / a0;
    }
    double l(double x)
    {
        const double y = b0 * x + b1 * xl1 + b2 * xl2 - a1 * yl1 - a2 * yl2;
        xl2 = xl1; xl1 = x; yl2 = yl1; yl1 = y;
        return y;
    }
    double r(double x)
    {
        const double y = b0 * x + b1 * xr1 + b2 * xr2 - a1 * yr1 - a2 * yr2;
        xr2 = xr1; xr1 = x; yr2 = yr1; yr1 = y;
        return y;
    }
    void reset() { xl1 = xl2 = yl1 = yl2 = xr1 = xr2 = yr1 = yr2 = 0.0; }
    // magnitude at f Hz (for the display)
    double mag(double f, double sr) const
    {
        const std::complex<double> z1 = std::polar(1.0, -2.0 * M_PI * f / sr), z2 = z1 * z1;
        return std::abs((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
    }
};

// the display (kick_shaper_gfx.jsfx draws the same): the level of a hit's tail in dB at
// time t (s) after it, with the Attack Shape boost or cut fading out over the first 15 ms
inline double tailDb(const Params& p, double t)
{
    const double ds = p.decayMs / 1000.0;
    double tail = t < 0.02 ? 1.0 : std::exp(-(t - 0.02) / ds);
    tail = tail + (1.0 - tail) * (ds - 0.05) / 0.45 * 0.9;
    return 20.0 * std::log10(std::max(tail, 1e-7)) + p.attackDb * std::max(0.0, 1.0 - t / 0.015);
}

class KickShaperProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fFast = fSlow = 0.0;
        fT = 1000000.0;
        fArmed = true;
        fThump.reset(); fBody.reset(); fClick.reset();
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    // the three bells, for the display
    const Bell& thump() const { return fThump; }
    const Bell& body() const { return fBody; }
    const Bell& click() const { return fClick; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = fClick.l(fBody.l(fThump.l(inL[i])));
            const double r = fClick.r(fBody.r(fThump.r(inR[i])));

            const double lev = std::max(std::fabs(l), std::fabs(r));
            fFast = lev > fFast ? fFa * fFast + (1.0 - fFa) * lev : fFr * fFast + (1.0 - fFr) * lev;
            fSlow = lev > fSlow ? fSa * fSlow + (1.0 - fSa) * lev : fSr_ * fSlow + (1.0 - fSr_) * lev;

            const double trans = fFast > 0.000001 ? std::max(0.0, std::min(1.0, (fFast - fSlow) / fFast)) : 0.0;
            const double g = std::pow(10.0, fP.attackDb * trans / 20.0);

            if (fArmed && trans > 0.5 && fFast > 0.01)
            {
                fT = 0.0;
                fArmed = false;
            }
            if (trans < 0.2)
                fArmed = true;
            fT += 1.0 / fSr;
            double tail = fT < 0.02 ? 1.0 : std::exp(-(fT - 0.02) / fDecay);
            tail = tail + (1.0 - tail) * (fDecay - 0.05) / 0.45 * 0.9;

            outL[i] = static_cast<T>(l * g * tail * fOut);
            outR[i] = static_cast<T>(r * g * tail * fOut);
        }
    }

private:
    void updateParams()
    {
        fThump.set(fSr, 55.0, 0.8, fP.thumpDb);
        fBody.set(fSr, 120.0, 1.0, fP.bodyDb);
        fClick.set(fSr, 4000.0, 0.7, fP.clickDb);
        fDecay = fP.decayMs / 1000.0;
        fOut = std::pow(10.0, fP.outDb / 20.0);
        fFa = std::exp(-1.0 / (0.001 * fSr)); fFr = std::exp(-1.0 / (0.02 * fSr));
        fSa = std::exp(-1.0 / (0.015 * fSr)); fSr_ = std::exp(-1.0 / (0.02 * fSr));
    }

    Params fP;
    Bell fThump, fBody, fClick;
    double fSr = 48000.0, fFast = 0.0, fSlow = 0.0, fT = 1000000.0;
    bool fArmed = true;
    double fDecay = 0.25, fOut = 1.0, fFa = 0.0, fFr = 0.0, fSa = 0.0, fSr_ = 0.0;
};

} // namespace nullkick
