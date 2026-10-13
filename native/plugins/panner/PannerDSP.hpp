/*
 * NULL JSFX - Panner, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/panner.jsfx, computed in double like EEL2 and in the same order.
 * The input is split into mid and side (Width scales the side); the mid is placed, the side keeps
 * the source's image. Balance: sine-law L/R balance. Natural: the far ear hears the mid later
 * (Woodworth ITD, up to ~0.66 ms, linear-interpolated delay) and duller (first-order high shelf,
 * down to -8 dB above ~1.8 kHz) at constant power. Haas: the far ear hears it up to Haas Delay
 * later. Gains, delays and shelves glide over ~20 ms.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullpanner {

static constexpr double kPi = 3.141592653589793;
static constexpr int kBufSize = 8192; // 30 ms + ITD at up to 192 kHz

// what @slider computes: per ear the mid gain, side gain, delay (samples) and shelf gain
struct Targets {
    double gl = 1, gr = 1, sl = 1, sr = 1, dl = 0, dr = 0, hl = 1, hr = 1;
    double outG = 1;
};

inline Targets targets(double srate, double panSlider, int mode, double haasMs, double outDb) noexcept
{
    Targets t;
    const double pan = panSlider / 100.0;
    const double a = std::fabs(pan);
    t.outG = std::pow(10.0, outDb / 20.0);
    if (mode == 0)
    {
        const double th = (pan + 1.0) * kPi / 4.0;
        t.gl = std::min(1.0, std::sqrt(2.0) * std::cos(th));
        t.gr = std::min(1.0, std::sqrt(2.0) * std::sin(th));
        t.sl = t.gl;
        t.sr = t.gr;
        t.dl = t.dr = 0.0;
        t.hl = t.hr = 1.0;
    }
    else if (mode == 1)
    {
        const double th = (1.0 + 0.6 * pan) * kPi / 4.0;
        t.gl = std::sqrt(2.0) * std::cos(th);
        t.gr = std::sqrt(2.0) * std::sin(th);
        t.sl = t.sr = 1.0 - 0.5 * a;
        const double itd = 0.0875 / 343.0 * (a * kPi / 2.0 + std::sin(a * kPi / 2.0)) * srate;
        t.dl = pan > 0.0 ? itd : 0.0;
        t.dr = pan < 0.0 ? itd : 0.0;
        const double sh = std::pow(10.0, -8.0 * a / 20.0);
        t.hl = pan > 0.0 ? sh : 1.0;
        t.hr = pan < 0.0 ? sh : 1.0;
    }
    else
    {
        t.gl = pan > 0.0 ? 1.0 - 0.3 * a : 1.0;
        t.gr = pan < 0.0 ? 1.0 - 0.3 * a : 1.0;
        t.sl = t.sr = 1.0 - 0.5 * a;
        const double hd = a * haasMs * srate / 1000.0;
        t.dl = pan > 0.0 ? hd : 0.0;
        t.dr = pan < 0.0 ? hd : 0.0;
        t.hl = t.hr = 1.0;
    }
    t.gl *= t.outG; t.gr *= t.outG;
    t.sl *= t.outG; t.sr *= t.outG;
    return t;
}

class PannerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBuf.assign(size_t(kBufSize), 0.0);
        fW = 0;
        fK = 1.0 - std::exp(-1.0 / (0.02 * sr));
        fHs = 1.0 - std::exp(-2.0 * kPi * 1800.0 / sr);
        fLpL = fLpR = 0.0;
        fFirst = true;
    }

    // @slider
    void setParams(double pan, int mode, double widthPct, double haasMs, double outDb)
    {
        fWid = widthPct / 100.0;
        fT = targets(fSr, pan, mode, haasMs, outDb);
        if (fFirst)
        {
            fC = fT;
            fFirst = false;
        }
    }

    template <typename T, typename Tap>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, Tap&& tap)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double x0 = inL[i], x1 = inR[i];
            const double mid = (x0 + x1) * 0.5;
            const double side = (x0 - x1) * 0.5 * fWid;
            fBuf[size_t(fW)] = mid;

            fC.gl = glide(fC.gl, fT.gl); fC.gr = glide(fC.gr, fT.gr);
            fC.sl = glide(fC.sl, fT.sl); fC.sr = glide(fC.sr, fT.sr);
            fC.dl = glide(fC.dl, fT.dl); fC.dr = glide(fC.dr, fT.dr);
            fC.hl = glide(fC.hl, fT.hl); fC.hr = glide(fC.hr, fT.hr);

            double ml = rd(fC.dl);
            double mr = rd(fC.dr);
            fLpL += fHs * (ml - fLpL);
            fLpR += fHs * (mr - fLpR);
            if (std::fabs(fLpL) < 1e-15) fLpL = 0.0;
            if (std::fabs(fLpR) < 1e-15) fLpR = 0.0;
            ml += (fC.hl - 1.0) * (ml - fLpL);
            mr += (fC.hr - 1.0) * (mr - fLpR);
            if (++fW >= kBufSize)
                fW = 0;

            const double yl = ml * fC.gl + side * fC.sl;
            const double yr = mr * fC.gr - side * fC.sr;
            outL[i] = static_cast<T>(yl);
            outR[i] = static_cast<T>(yr);
            tap(yl, yr);
        }
    }

private:
    double glide(double v, double t) const noexcept
    {
        v += fK * (t - v);
        return std::fabs(t - v) < 1e-12 ? t : v;
    }

    double rd(double d) const noexcept
    {
        double rp = double(fW) - d;
        if (rp < 0.0)
            rp += kBufSize;
        const int i = int(std::floor(rp));
        const double fr = rp - double(i);
        int i2 = i + 1;
        if (i2 >= kBufSize)
            i2 = 0;
        return fBuf[size_t(i)] + (fBuf[size_t(i2)] - fBuf[size_t(i)]) * fr;
    }

    double fSr = 48000.0, fK = 0.001, fHs = 0.2, fWid = 1.0, fLpL = 0.0, fLpR = 0.0;
    std::vector<double> fBuf;
    int fW = 0;
    bool fFirst = true;
    Targets fT, fC;
};

} // namespace nullpanner
