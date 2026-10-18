/*
 * NULL JSFX - Draw EQ, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/draw_eq.jsfx in double, in the JSFX's order of operations:
 * ten points at fixed octave-spaced frequencies (31.25 Hz .. 16 kHz) set the gain the curve
 * passes through; a peaking filter per point, its gain solved so the cascade hits every point
 * (a linear model from the response of a 12 dB filter, a solve, two corrections with the true
 * response). Smoothness sets the filters' Q (1.5 .. 0.6). Every point at 0 dB bypasses the
 * filters. Mix with the dry signal, then Output.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>

namespace nulldraweq {

static constexpr double kPi = 3.141592653589793;

struct Params {
    double gain[10] = {}; // dB at the points
    double smooth = 50.0; // %
    double output = 0.0;  // dB
    double mix = 100.0;   // %
};

// the frequency of point i (FQ[i] in the JSFX)
inline double pointFreq(int i) { return 1000.0 * std::pow(2.0, double(i - 5)); }

// the solved filter bank: gains, coefficients, and the response for a display
class Bank {
public:
    double fq[10] = {}, gb[10] = {}, co[50] = {};
    double q = 1.0, sr = 48000.0;
    int n = 0;
    bool flat = true;

    // @slider
    void compute(const Params& p, double sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < 10; ++i)
            fq[i] = pointFreq(i);
        double tg[10];
        for (int i = 0; i < 10; ++i)
            tg[i] = p.gain[i];
        q = 1.5 * std::pow(0.4, p.smooth / 100.0);
        n = 0;
        for (int k = 0; k < 10; ++k)
            if (fq[n] < sr * 0.45)
                n += 1;
        flat = true;
        for (int i = 0; i < n; ++i)
            if (!eelEq(tg[i], 0.0))
                flat = false;
        if (flat)
        {
            std::fill(std::begin(gb), std::end(gb), 0.0);
            return;
        }
        for (int j = 0; j < n; ++j)
        {
            design(fq[j], 12.0, q);
            for (int i = 0; i < n; ++i)
                mt[i * 10 + j] = db(fq[i]) / 12.0;
        }
        for (int i = 0; i < n; ++i)
            rh[i] = tg[i];
        solve();
        for (int i = 0; i < n; ++i)
            gb[i] = std::max(-30.0, std::min(30.0, sx[i]));
        for (int it = 0; it < 2; ++it)
        {
            for (int i = 0; i < n; ++i)
                er[i] = tg[i];
            for (int j = 0; j < n; ++j)
            {
                design(fq[j], gb[j], q);
                for (int i = 0; i < n; ++i)
                    er[i] -= db(fq[i]);
            }
            for (int i = 0; i < n; ++i)
                rh[i] = er[i];
            solve();
            for (int i = 0; i < n; ++i)
                gb[i] = std::max(-30.0, std::min(30.0, gb[i] + sx[i]));
        }
        for (int j = 0; j < n; ++j)
        {
            design(fq[j], gb[j], q);
            co[j * 5] = b0;
            co[j * 5 + 1] = b1;
            co[j * 5 + 2] = b2;
            co[j * 5 + 3] = a1;
            co[j * 5 + 4] = a2;
        }
    }

    // the response of the whole bank at f Hz in dB (for the display; 0 when flat)
    double responseDb(double f)
    {
        if (flat)
            return 0.0;
        double s = 0.0;
        for (int j = 0; j < n; ++j)
        {
            b0 = co[j * 5];
            b1 = co[j * 5 + 1];
            b2 = co[j * 5 + 2];
            a1 = co[j * 5 + 3];
            a2 = co[j * 5 + 4];
            s += db(std::min(f, sr * 0.4999));
        }
        return s;
    }

private:
    // EEL2's == (|a - b| < 0.00001)
    static bool eelEq(double a, double b) { return std::fabs(a - b) < 0.00001; }

    void design(double f, double g, double qq)
    {
        const double w = 2.0 * kPi * f / sr;
        const double c = std::cos(w), s = std::sin(w);
        const double al = s / (2.0 * qq);
        const double A = std::pow(10.0, g / 40.0);
        const double a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0;
        b1 = -2.0 * c / a0;
        b2 = (1.0 - al * A) / a0;
        a1 = -2.0 * c / a0;
        a2 = (1.0 - al / A) / a0;
    }

    double db(double f) const
    {
        const double w = 2.0 * kPi * f / sr;
        const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return 10.0 * std::log10(std::max(nr * nr + ni * ni, 1e-20) / std::max(dr * dr + di * di, 1e-20));
    }

    // MT x = RH for the first n points and filters (Gaussian elimination, partial pivoting)
    void solve()
    {
        std::memcpy(wm, mt, sizeof(wm));
        for (int k = 0; k < n; ++k)
        {
            int p = k;
            double mx = std::fabs(wm[k * 10 + k]);
            for (int i = k + 1; i < n; ++i)
                if (std::fabs(wm[i * 10 + k]) > mx)
                {
                    mx = std::fabs(wm[i * 10 + k]);
                    p = i;
                }
            if (p > k)
            {
                for (int j = 0; j < n; ++j)
                    std::swap(wm[k * 10 + j], wm[p * 10 + j]);
                std::swap(rh[k], rh[p]);
            }
            for (int i = k + 1; i < n; ++i)
            {
                const double f = wm[i * 10 + k] / wm[k * 10 + k];
                for (int j = k; j < n; ++j)
                    wm[i * 10 + j] -= f * wm[k * 10 + j];
                rh[i] -= f * rh[k];
            }
        }
        for (int i = n - 1; i >= 0; --i)
        {
            double t = rh[i];
            for (int j = i + 1; j < n; ++j)
                t -= wm[i * 10 + j] * sx[j];
            sx[i] = t / wm[i * 10 + i];
        }
    }

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double mt[100] = {}, wm[100] = {}, rh[10] = {}, sx[10] = {}, er[10] = {};
};

class DrawEqProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        reset();
    }

    void reset() { std::memset(fSt, 0, sizeof(fSt)); }

    // @slider
    void setParams(const Params& p)
    {
        fBank.compute(p, fSr);
        if (fBank.flat)
            reset();
        fOut = std::pow(10.0, p.output / 20.0);
        fMix = p.mix / 100.0;
    }

    const Bank& bank() const { return fBank; }

    // @sample
    void processSample(double& spl0, double& spl1)
    {
        double l = spl0, r = spl1;
        if (!fBank.flat)
        {
            const double* c = fBank.co;
            double* s = fSt;
            for (int k = 0; k < fBank.n; ++k)
            {
                double y = c[0] * l + c[1] * s[0] + c[2] * s[1] - c[3] * s[2] - c[4] * s[3];
                s[1] = s[0];
                s[0] = l;
                s[3] = s[2];
                s[2] = y;
                l = y;
                y = c[0] * r + c[1] * s[4] + c[2] * s[5] - c[3] * s[6] - c[4] * s[7];
                s[5] = s[4];
                s[4] = r;
                s[7] = s[6];
                s[6] = y;
                r = y;
                c += 5;
                s += 8;
            }
        }
        spl0 = (spl0 * (1.0 - fMix) + l * fMix) * fOut;
        spl1 = (spl1 * (1.0 - fMix) + r * fMix) * fOut;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    Bank fBank;
    double fSt[80] = {};
    double fSr = 48000.0, fOut = 1.0, fMix = 1.0;
};

} // namespace nulldraweq
