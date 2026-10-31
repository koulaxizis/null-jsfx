/*
 * NULL JSFX - Space, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/space.jsfx, computed in double in the EEL2 order: per side a
 * pre-delay and two Schroeder all-passes, then an 8-line feedback delay network (moving,
 * linearly interpolated reads, one-pole damping, a 60 dB decay in the decay time, an 8x8
 * Hadamard matrix). The buffers are allocated in setSampleRate, never in process.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullspace {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr double kTiny = 0.000000000000001;
static constexpr double kLineMs[8] = { 9.7, 11.3, 13.1, 14.9, 17.3, 19.9, 22.1, 24.7 };
static constexpr double kApMs[4] = { 1.31, 1.97, 1.43, 2.11 };

struct Params {
    int type = 2;
    double size = 50.0, decay = 50.0, damping = 30.0, detune = 30.0, preDelay = 0.0, mix = 30.0;
};

// the decay time (s) of a type at Decay 0..100 (rt in space.jsfx)
inline double decayTime(int typ, double decay) noexcept
{
    return typ == 0 ? 0.2 * std::pow(7.5, decay / 100.0)
         : typ == 1 ? 0.5 * std::pow(6.0, decay / 100.0)
         : typ == 2 ? std::pow(8.0, decay / 100.0)
                    : 3.0 * std::pow(10.0, decay / 100.0);
}

class SpaceProcessor {
public:
    void setSampleRate(double sr)
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        fLmax = int(std::ceil(0.26 * fSr)) + 8;
        fPmax = int(std::ceil(0.25 * fSr)) + 8;
        fAmax = int(std::ceil(0.03 * fSr)) + 8;
        fLines.assign(size_t(8) * size_t(fLmax), 0.0);
        fPre.assign(size_t(2) * size_t(fPmax), 0.0);
        fAp.assign(size_t(4) * size_t(fAmax), 0.0);
        clearState();
        fKs = 1.0 - std::exp(-1.0 / (0.05 * fSr));
        fSnap = true;
        update();
    }

    // the parameter maths alone (no buffers), for the interface's display
    void setParamsOnly(const Params& p, double sr) noexcept
    {
        fSr = sr;
        fP = p;
        update();
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        update();
    }

    // play and seek keep the tail (ext_noinit), so only a new sample rate clears it
    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames) noexcept
    {
        if (fLines.empty())
        {
            for (uint32_t i = 0; i < frames; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
            return;
        }
        double* const pb = fPre.data();
        double* const ab = fAp.data();
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double s0 = inL[n], s1 = inR[n];
            double xl, xr;
            if (fPd > 0)
            {
                int rp = fPw - fPd;
                if (rp < 0)
                    rp += fPmax;
                xl = pb[rp];
                xr = pb[fPmax + rp];
            }
            else
            {
                xl = s0;
                xr = s1;
            }
            pb[fPw] = s0;
            pb[fPmax + fPw] = s1;
            if (++fPw >= fPmax)
                fPw = 0;

            for (int i = 0; i < 4; ++i)
                fApLen[i] += (fApTgt[i] - fApLen[i]) * fKs;
            xl = allpass(allpass(xl, ab, int(std::floor(fApLen[0] + 0.5))), ab + fAmax, int(std::floor(fApLen[1] + 0.5)));
            xr = allpass(allpass(xr, ab + 2 * fAmax, int(std::floor(fApLen[2] + 0.5))), ab + 3 * fAmax,
                         int(std::floor(fApLen[3] + 0.5)));
            if (++fAw >= fAmax)
                fAw = 0;

            double o8[8];
            for (int i = 0; i < 8; ++i)
            {
                fLen[i] += (fTgt[i] - fLen[i]) * fKs;
                fPh[i] += fStep[i];
                if (fPh[i] >= 2.0 * kPi)
                    fPh[i] -= 2.0 * kPi;
                double rp = fWp - (fLen[i] + fMd * std::sin(fPh[i]));
                if (rp < 0.0)
                    rp += fLmax;
                const double i0 = std::floor(rp);
                const double fr = rp - i0;
                int j0 = int(i0), j1 = j0 + 1;
                if (j1 >= fLmax)
                    j1 = 0;
                const double* b = fLines.data() + size_t(i) * size_t(fLmax);
                double o = b[j0] + fr * (b[j1] - b[j0]);
                o = fDamp[i] + (o - fDamp[i]) * fDa;
                fDamp[i] = std::fabs(o) < kTiny ? 0.0 : o;
                o8[i] = fDamp[i] * fG[i];
            }
            const double wl = o8[0] - o8[2] + o8[4] - o8[6];
            const double wr = o8[1] - o8[3] + o8[5] - o8[7];

            for (int h = 1; h < 8; h *= 2)
                for (int j = 0; j < 8; ++j)
                    if ((j & h) == 0)
                    {
                        const double a = o8[j], c = o8[j + h];
                        o8[j] = a + c;
                        o8[j + h] = a - c;
                    }
            for (int i = 0; i < 8; ++i)
            {
                const double w = o8[i] * 0.35355339059327373 + ((i & 1) ? xr : xl);
                fLines[size_t(i) * size_t(fLmax) + size_t(fWp)] = std::fabs(w) < kTiny ? 0.0 : w;
            }
            if (++fWp >= fLmax)
                fWp = 0;

            outL[n] = float(s0 * (1.0 - fMx) + wl * 0.5 * fMx);
            outR[n] = float(s1 * (1.0 - fMx) + wr * 0.5 * fMx);
        }
    }

    // for the display: the decay times (s) at low and middle frequencies and at 8 kHz
    double rt() const noexcept { return fRt; }
    double rt8() const noexcept
    {
        const double w = 2.0 * kPi * std::fmin(8000.0, 0.45 * fSr) / fSr;
        const double q = 1.0 - fDa;
        const double lp = -10.0 * std::log10(fDa * fDa / (1.0 - 2.0 * q * std::cos(w) + q * q));
        const double ml = (fTgt[0] + fTgt[1] + fTgt[2] + fTgt[3] + fTgt[4] + fTgt[5] + fTgt[6] + fTgt[7]) / 8.0;
        return 60.0 / (60.0 / fRt + lp * fSr / ml);
    }

private:
    double allpass(double x, double* b, int n) noexcept
    {
        int rp = fAw - n;
        if (rp < 0)
            rp += fAmax;
        const double d = b[rp];
        double w = x + fApg * d;
        w = std::fabs(w) < kTiny ? 0.0 : w;
        b[fAw] = w;
        return d - fApg * w;
    }

    void clearState() noexcept
    {
        fWp = fPw = fAw = 0;
        for (int i = 0; i < 8; ++i)
            fLen[i] = fDamp[i] = fPh[i] = 0.0;
        for (int i = 0; i < 4; ++i)
            fApLen[i] = 0.0;
    }

    // the @slider code of space.jsfx
    void update() noexcept
    {
        const double sr = fSr;
        const int typ = fP.type;
        const double tm = typ == 0 ? 1.0 : typ == 1 ? 1.7 : typ == 2 ? 3.0 : 5.0;
        const double sz = 0.5 * std::pow(2.0, fP.size / 50.0);
        fRt = decayTime(typ, fP.decay);
        fDa = 1.0 - std::exp(-2.0 * kPi * std::fmin(20000.0 * std::pow(0.075, fP.damping / 100.0), 0.45 * sr) / sr);
        fMd = fP.detune / 100.0 * (typ == 3 ? 1.5 : 0.5) * sr / 1000.0;
        fPd = int(std::floor(fP.preDelay / 1000.0 * sr + 0.5));
        fMx = fP.mix / 100.0;
        fApg = typ == 3 ? 0.7 : 0.6;
        for (int i = 0; i < 8; ++i)
        {
            fTgt[i] = kLineMs[i] * tm * sz * sr / 1000.0;
            fG[i] = std::pow(10.0, -3.0 * fTgt[i] / (fRt * sr));
            fStep[i] = (0.13 + 0.05 * i) * 2.0 * kPi / sr;
        }
        for (int i = 0; i < 4; ++i)
            fApTgt[i] = kApMs[i] * tm * sz * sr / 1000.0;
        if (fSnap)
        {
            for (int i = 0; i < 8; ++i)
                fLen[i] = fTgt[i];
            for (int i = 0; i < 4; ++i)
                fApLen[i] = fApTgt[i];
            fSnap = false;
        }
    }

    Params fP;
    double fSr = 48000.0, fKs = 0.0, fRt = 1.0, fDa = 1.0, fMd = 0.0, fMx = 0.3, fApg = 0.6;
    int fLmax = 0, fPmax = 0, fAmax = 0, fWp = 0, fPw = 0, fAw = 0, fPd = 0;
    bool fSnap = true;
    double fLen[8] = {}, fTgt[8] = {}, fG[8] = {}, fDamp[8] = {}, fPh[8] = {}, fStep[8] = {};
    double fApLen[4] = {}, fApTgt[4] = {};
    std::vector<double> fLines, fPre, fAp;
};

} // namespace nullspace
