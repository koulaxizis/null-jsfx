/*
 * NULL JSFX - Tape Echo, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tape_echo.jsfx, computed in double like EEL2. One tape loop per
 * channel (a power of two holding 3 x a dotted quarter at 40 bpm) is read by three playback heads
 * at 1x, 2x and 3x the repeat time (Repeat Time, or a note at the host tempo, re-read every block
 * as the JSFX's @block does) with 4-point Hermite interpolation. The heads of the selected mode
 * share one feedback loop: a 4.5 kHz RBJ low-pass, an asymmetric soft saturation with slope 1 at
 * 0 (bounded, so Intensity above 100% self-oscillates without blowing up) and a 90 Hz one-pole
 * low cut. Wow and flutter (four sines) move every head; the time glides like a tape motor. The
 * echoes get RBJ low and high shelves (Bass, Treble); the R modes add a two-spring tank (as
 * spring_reverb.jsfx) fed with the input and the echoes. Freeze loops the tape unchanged.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulltapeecho {

static constexpr double kPi = 3.141592653589793;

// heads (bits 1, 2, 4) and spring (bit 8) of the 12 positions of the Heads selector
inline int headMask(double heads) noexcept
{
    return heads < 0.5 ? 1 : heads < 1.5 ? 2 : heads < 2.5 ? 4 : heads < 3.5 ? 3 : heads < 4.5 ? 5
         : heads < 5.5 ? 6 : heads < 6.5 ? 7 : heads < 7.5 ? 9 : heads < 8.5 ? 11 : heads < 9.5 ? 14
         : heads < 10.5 ? 15 : 8;
}

// the 1x head time in ms (the JSFX's te_time)
inline double repeatMs(double timeMs, double sync, double tempo) noexcept
{
    if (sync >= 0.5 && tempo > 0.0)
        return 60000.0 / tempo * (sync < 1.5 ? 1.0 : sync < 2.5 ? 0.5 : sync < 3.5 ? 0.25 : sync < 4.5 ? 1.5
                                  : sync < 5.5 ? 0.75 : sync < 6.5 ? 2.0 / 3.0 : 1.0 / 3.0);
    return timeMs;
}

// The display (tape_echo_gfx.jsfx draws the same): the level of the n-th repeat (n = 1, 2, ...,
// at n x the repeat time) for an impulse, with the loop's filters and saturation left out.
// out[0..count-1] = linear levels of repeats 1..count.
inline void echoLevels(int mask, double intensity, bool freeze, int count, double* out) noexcept
{
    const int n = (mask & 1) + ((mask & 2) ? 1 : 0) + ((mask & 4) ? 1 : 0);
    const double c = n > 0 ? (freeze ? 1.0 : intensity) / n : 0.0, wg = n > 0 ? 1.0 / std::sqrt(double(n)) : 0.0;
    const double h1 = (mask & 1) ? 1.0 : 0.0, h2 = (mask & 2) ? 1.0 : 0.0, h3 = (mask & 4) ? 1.0 : 0.0;
    double t1 = 1.0, t2 = 0.0, t3 = 0.0; // the tape at 1, 2, 3 repeats back
    for (int j = 0; j < count; ++j)
    {
        const double s = h1 * t1 + h2 * t2 + h3 * t3;
        out[j] = wg * s;
        t3 = t2;
        t2 = t1;
        t1 = c * s;
    }
}

// RBJ biquad, transposed direct form II with the JSFX's denormal flush
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;

    void lowpass(double f, double q, double sr)
    {
        const double w = 2.0 * kPi * f / sr, al = std::sin(w) / (2.0 * q);
        b0 = (1.0 - std::cos(w)) / 2.0 / (1.0 + al);
        b1 = (1.0 - std::cos(w)) / (1.0 + al);
        b2 = b0;
        a1 = -2.0 * std::cos(w) / (1.0 + al);
        a2 = (1.0 - al) / (1.0 + al);
    }
    void shelf(double f, double db, bool high, double sr)
    {
        const double w = 2.0 * kPi * f / sr, cs = std::cos(w), A = std::pow(10.0, db / 40.0);
        const double sa = 2.0 * std::sqrt(A) * std::sin(w) / 2.0 * std::sqrt(2.0);
        if (high)
        {
            const double a0 = (A + 1.0) - (A - 1.0) * cs + sa;
            b0 = A * ((A + 1.0) + (A - 1.0) * cs + sa) / a0;
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs) / a0;
            b2 = A * ((A + 1.0) + (A - 1.0) * cs - sa) / a0;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs) / a0;
            a2 = ((A + 1.0) - (A - 1.0) * cs - sa) / a0;
        }
        else
        {
            const double a0 = (A + 1.0) + (A - 1.0) * cs + sa;
            b0 = A * ((A + 1.0) - (A - 1.0) * cs + sa) / a0;
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs) / a0;
            b2 = A * ((A + 1.0) - (A - 1.0) * cs - sa) / a0;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs) / a0;
            a2 = ((A + 1.0) + (A - 1.0) * cs - sa) / a0;
        }
    }
    double run(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::fabs(z1) < 1e-21)
            z1 = 0.0;
        if (std::fabs(z2) < 1e-21)
            z2 = 0.0;
        return y;
    }
};

struct Params {
    double time = 300.0, sync = 0.0, heads = 3.0, intensity = 40.0, bass = 0.0, treble = 0.0;
    double wowFlutter = 25.0, saturation = 30.0, spring = 0.0, mix = 35.0, freeze = 0.0;
};

class TapeEchoProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = 1024;
        while (fSize < int(std::ceil(sr * 6.8)) + 16)
            fSize *= 2;
        fMask = fSize - 1;
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fSpMax = int(std::ceil(sr * 0.08));
        fApL.assign(kStages, 0.0);
        fApR.assign(kStages, 0.0);
        fLineL.assign(size_t(fSpMax), 0.0);
        fLineR.assign(size_t(fSpMax), 0.0);
        fW = 0;
        fSpW = 0;
        fDMax = std::floor(sr * 2.25);
        fDMin = std::floor(sr * 0.02);
        fDt = fDs = std::floor(sr * 0.3);
        fFirst = true;
        fDsC = 1.0 - std::exp(-1.0 / (0.12 * sr));
        fP1 = 0.0;
        fP2 = 0.3;
        fP3 = 0.0;
        fP4 = 0.6;
        fI1 = 0.55 / sr;
        fI2 = 1.37 / sr;
        fI3 = 6.9 / sr;
        fI4 = 11.3 / sr;
        fLcA = 1.0 - std::exp(-2.0 * kPi * 90.0 / sr);
        fLcL = fLcR = 0.0;
        fHcL = fHcR = fBsL = fBsR = fTrL = fTrR = Biquad();
        fLenL = int(std::floor(sr * 0.0437));
        fLenR = int(std::floor(sr * 0.0531));
        fSpGl = std::min(0.97, std::pow(10.0, -3.0 * (fLenL / sr + kStages * 0.0001) / 2.2));
        fSpGr = std::min(0.97, std::pow(10.0, -3.0 * (fLenR / sr + kStages * 0.0001) / 2.2));
        fSpTc = std::exp(-2.0 * kPi * 3500.0 / sr);
        fSpHc = std::exp(-2.0 * kPi * 120.0 / sr);
        fSpHx = fSpHy = fSpLpL = fSpLpR = 0.0;
        fRvWas = false;
        fHcL.lowpass(4500.0, 0.707, sr);
        fHcR.lowpass(4500.0, 0.707, sr);
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        fHm = headMask(p.heads);
        const int n = (fHm & 1) + ((fHm & 2) ? 1 : 0) + ((fHm & 4) ? 1 : 0);
        fHMax = (fHm & 4) ? 3 : (fHm & 2) ? 2 : 1;
        fFbN = n > 0 ? 1.0 / n : 0.0;
        fWg = n > 0 ? 1.0 / std::sqrt(double(n)) : 0.0;
        fInten = p.intensity / 100.0;
        fBsL.shelf(250.0, p.bass, false, fSr);
        fBsR.shelf(250.0, p.bass, false, fSr);
        fTrL.shelf(2500.0, p.treble, true, fSr);
        fTrR.shelf(2500.0, p.treble, true, fSr);
        fDw = p.wowFlutter / 100.0 * 0.0025 * fSr / 1.31;
        fDf = p.wowFlutter / 100.0 * 0.00015 * fSr / 1.4;
        fSatG = 0.5 + p.saturation / 100.0 * 3.5;
        fSatB = p.saturation / 100.0 * 0.15;
        fSatComp = std::pow(1.0 + fSatB * fSatB, 1.5) / fSatG;
        fSpLvl = p.spring / 100.0;
        fRvOn = (fHm & 8) && p.spring > 0.0;
        if (fRvOn && !fRvWas)
        {
            std::fill(fApL.begin(), fApL.end(), 0.0);
            std::fill(fApR.begin(), fApR.end(), 0.0);
            std::fill(fLineL.begin(), fLineL.end(), 0.0);
            std::fill(fLineR.begin(), fLineR.end(), 0.0);
            fSpHx = fSpHy = fSpLpL = fSpLpR = 0.0;
            fSpW = 0;
        }
        fRvWas = fRvOn;
        fMix = p.mix / 100.0;
        fFrz = p.freeze >= 0.5;
        updateTime();
    }

    double repeatMsNow() const noexcept { return fRtMs; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo)
    {
        // @block
        fTempo = tempo;
        updateTime();
        if (fFirst)
        {
            fDs = fDt;
            fFirst = false;
        }
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            fDs += fDsC * (fDt - fDs);
            fP1 += fI1;
            if (fP1 >= 1.0)
                fP1 -= 1.0;
            fP2 += fI2;
            if (fP2 >= 1.0)
                fP2 -= 1.0;
            fP3 += fI3;
            if (fP3 >= 1.0)
                fP3 -= 1.0;
            fP4 += fI4;
            if (fP4 >= 1.0)
                fP4 -= 1.0;
            const double m = fDw * (std::sin(2.0 * kPi * fP1) + 0.31 * std::sin(2.0 * kPi * fP2))
                           + fDf * (std::sin(2.0 * kPi * fP3) + 0.4 * std::sin(2.0 * kPi * fP4));

            double hl = 0.0, hr = 0.0;
            if (fHm & 1)
            {
                const double d = fDs + m;
                hl += tap(fL, d);
                hr += tap(fR, d);
            }
            if (fHm & 2)
            {
                const double d = 2.0 * fDs + m;
                hl += tap(fL, d);
                hr += tap(fR, d);
            }
            if (fHm & 4)
            {
                const double d = 3.0 * fDs + m;
                hl += tap(fL, d);
                hr += tap(fR, d);
            }

            double el = sat(fHcL.run(hl * fFbN));
            double er = sat(fHcR.run(hr * fFbN));
            fLcL += fLcA * (el - fLcL);
            el -= fLcL;
            fLcR += fLcA * (er - fLcR);
            er -= fLcR;

            if (fFrz)
            {
                fL[size_t(fW)] = fL[size_t((fW - fLoop) & fMask)];
                fR[size_t(fW)] = fR[size_t((fW - fLoop) & fMask)];
            }
            else
            {
                fL[size_t(fW)] = xl + el * fInten;
                fR[size_t(fW)] = xr + er * fInten;
            }
            fW = (fW + 1) & fMask;

            double wl = fTrL.run(fBsL.run(hl * fWg));
            double wr = fTrR.run(fBsR.run(hr * fWg));

            if (fRvOn)
            {
                const double x = (xl + xr) * 0.5 + (wl + wr) * 0.5;
                fSpHy = fSpHc * (fSpHy + x - fSpHx);
                fSpHx = x;
                int rp = fSpW - fLenL;
                if (rp < 0)
                    rp += fSpMax;
                const double fl = fLineL[size_t(rp)];
                rp = fSpW - fLenR;
                if (rp < 0)
                    rp += fSpMax;
                const double fr = fLineR[size_t(rp)];
                const double sl = disperse(fSpHy + fl * fSpGl, fApL);
                const double sr = disperse(fSpHy + fr * fSpGr, fApR);
                fSpLpL = (1.0 - fSpTc) * sl + fSpTc * fSpLpL;
                fSpLpR = (1.0 - fSpTc) * sr + fSpTc * fSpLpR;
                fLineL[size_t(fSpW)] = fSpLpL;
                fLineR[size_t(fSpW)] = fSpLpR;
                if (++fSpW >= fSpMax)
                    fSpW = 0;
                wl += (fSpLpL + (sl - fSpLpL) * 0.28) * fSpLvl;
                wr += (fSpLpR + (sr - fSpLpR) * 0.28) * fSpLvl;
            }

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + wl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    static constexpr int kStages = 40;

    void updateTime()
    {
        fRtMs = repeatMs(fP.time, fP.sync, fTempo);
        fDt = std::max(fDMin, std::min(fDMax, fRtMs * fSr / 1000.0));
        fLoop = int(std::floor(fHMax * fDt + 0.5));
    }

    double tap(const std::vector<double>& b, double pos) const
    {
        double rp = fW - pos;
        if (rp < 0.0)
            rp += fSize;
        const double fi = std::floor(rp);
        const double fr = rp - fi;
        const int i = int(fi);
        const double xm1 = b[size_t((i - 1) & fMask)], x0 = b[size_t(i)];
        const double x1 = b[size_t((i + 1) & fMask)], x2 = b[size_t((i + 2) & fMask)];
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * fr + c2) * fr + c1) * fr + x0;
    }

    static double soft(double v) { return v / std::sqrt(1.0 + v * v); }
    double sat(double x) const { return (soft(x * fSatG + fSatB) - soft(fSatB)) * fSatComp; }

    static double disperse(double x, std::vector<double>& apb)
    {
        for (int i = 0; i < kStages; ++i)
        {
            const double s = apb[size_t(i)];
            const double y = s - 0.56 * x;
            apb[size_t(i)] = x + 0.56 * y;
            x = y;
        }
        return x;
    }

    Params fP;
    double fSr = 48000.0, fTempo = 120.0;
    std::vector<double> fL, fR, fApL, fApR, fLineL, fLineR;
    int fSize = 1024, fMask = 1023, fW = 0, fSpMax = 1, fSpW = 0, fLenL = 1, fLenR = 1;
    int fHm = 3, fHMax = 2, fLoop = 1;
    double fDMax = 1.0, fDMin = 1.0, fDt = 1.0, fDs = 1.0, fDsC = 0.0, fRtMs = 300.0;
    bool fFirst = true, fFrz = false, fRvOn = false, fRvWas = false;
    double fP1 = 0.0, fP2 = 0.0, fP3 = 0.0, fP4 = 0.0, fI1 = 0.0, fI2 = 0.0, fI3 = 0.0, fI4 = 0.0;
    double fDw = 0.0, fDf = 0.0, fLcA = 0.0, fLcL = 0.0, fLcR = 0.0;
    Biquad fHcL, fHcR, fBsL, fBsR, fTrL, fTrR;
    double fFbN = 0.5, fWg = 0.7, fInten = 0.4, fSatG = 1.0, fSatB = 0.0, fSatComp = 1.0;
    double fSpGl = 0.0, fSpGr = 0.0, fSpTc = 0.0, fSpHc = 0.0, fSpHx = 0.0, fSpHy = 0.0, fSpLpL = 0.0, fSpLpR = 0.0;
    double fSpLvl = 0.0, fMix = 0.35;
};

} // namespace nulltapeecho
