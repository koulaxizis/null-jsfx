/*
 * NULL JSFX - Reverb, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/reverb.jsfx, computed in double like EEL2: a stereo
 * Schroeder/Freeverb-style reverb. The mono sum feeds four damped feedback combs per side
 * (the right side's delays 23 samples longer at 44.1 kHz), then two series all-passes per
 * side; all lengths scale with srate / 44100. Damping is a one-pole low-pass in each comb's
 * feedback loop (16 kHz at 0% down to 1.2 kHz at 100%).
 * Extras, each skipped at its off setting so the defaults are the plain reverb: pre-delay of
 * the reverb input, Plate/Hall input diffusion (four all-passes, Dattorro lengths scaled from
 * 29761 Hz) with shorter/brighter (Plate) or longer/wider (Hall) combs, slow modulation of
 * the comb and all-pass delays (interpolated reads), Freeze (lossless loop, no input), and
 * mid/side width plus 12 dB/oct low and high cut on the wet signal.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullreverb {

static constexpr int kSpread = 23;
static constexpr int kCombLen[4] = { 1557, 1617, 1491, 1687 };
static constexpr int kApLen[2] = { 556, 441 };
static constexpr int kDiffLen[4] = { 142, 107, 379, 277 };
static constexpr double kDiffGain[4] = { 0.75, 0.75, 0.625, 0.625 };
static constexpr double kPi = 3.141592653589793; // $pi

// the comb length scale and the stereo spread of a Character (0 Classic, 1 Plate, 2 Hall)
inline double combScale(int chr) noexcept { return chr == 2 ? 1.45 : (chr == 1 ? 0.9 : 1.0); }
inline int spreadFor(int chr) noexcept { return chr == 2 ? 47 : kSpread; }

// the settings the JSFX's @slider derives (also used by the interface)
struct Params {
    double room, fb, dampFc, damp, dampA, mix;
};

inline Params paramsFor(double sizePct, double decayPct, double dampingPct, double mixPct, double srate,
                        int chr = 0, bool freeze = false) noexcept
{
    Params p;
    p.room = 0.25 + 0.75 * (sizePct / 100.0);
    p.fb = 0.7 + 0.27 * (decayPct / 100.0);
    p.dampFc = std::min(16000.0 * std::pow(1200.0 / 16000.0, dampingPct / 100.0), 0.45 * srate);
    if (chr == 1)
        p.dampFc = std::min(p.dampFc * 2.0, 0.45 * srate);
    p.damp = std::exp(-2.0 * 3.14159265358979 * p.dampFc / srate);
    p.dampA = 1.0 - p.damp;
    if (freeze)
    {
        p.fb = 1.0;
        p.damp = 0.0;
        p.dampA = 1.0;
    }
    p.mix = mixPct / 100.0;
    return p;
}

class ReverbProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSc = sr / 44100.0;
        fDkEnv = fGtEnv = fGtHold = 0.0;
        fGtG = 1.0;
        fBlen = int(std::ceil((1687 + 47) * 1.45 * fSc)) + 4;
        for (auto& c : fCombs)
            c.reset(fBlen);
        for (auto& a : fAps)
            a.reset(fBlen);
        const double dsc = sr / 29761.0;
        for (int i = 0; i < 4; ++i)
        {
            const int l = int(std::max(1.0, std::floor(kDiffLen[i] * dsc)));
            fDiff[i].reset(l);
            fDiff[i].len = l;
        }
        fPdSize = int(std::ceil(0.25 * sr)) + 2;
        fPd.assign(size_t(fPdSize), 0.0);
        fPdW = 0;
        fModA = fModB = 0.0;
        fLc[0].clear();
        fLc[1].clear();
        fHc[0].clear();
        fHc[1].clear();
    }

    // @slider
    void setParams(double sizePct, double decayPct, double dampingPct, double mixPct, bool bypass,
                   double preDelayMs = 0.0, double lowCut = 20.0, double highCut = 20000.0, double widthPct = 100.0,
                   double modPct = 0.0, bool freeze = false, int chr = 0, double duckPct = 0.0,
                   double gateDb = -80.0, double gateHoldMs = 150.0)
    {
        // Ducking and the gate on the wet signal (skipped at 0 % and -80 dB)
        fDuckOn = duckPct > 0.0;
        fDuckAmt = duckPct / 100.0;
        fDkAtt = 1.0 - std::exp(-1.0 / (0.01 * fSr));
        fDkRel = 1.0 - std::exp(-1.0 / (0.25 * fSr));
        fGtOn = gateDb > -80.0;
        fGtThr = std::pow(10.0, gateDb / 20.0);
        fGtLen = std::floor(gateHoldMs * fSr / 1000.0);
        fGtEr = std::exp(-1.0 / (0.02 * fSr));
        fGtGa = 1.0 - std::exp(-1.0 / (0.001 * fSr));
        fGtGr = 1.0 - std::exp(-1.0 / (0.01 * fSr));
        fP = paramsFor(sizePct, decayPct, dampingPct, mixPct, fSr, chr, freeze);
        const double k = fP.room * fSc;
        const double kc = k * combScale(chr);
        const int sp = spreadFor(chr);
        fDiffOn = chr > 0;
        for (int i = 0; i < 4; ++i)
        {
            fCombs[i].setLen(int(std::max(1.0, std::floor(kCombLen[i] * kc))));
            fCombs[4 + i].setLen(int(std::max(1.0, std::floor((kCombLen[i] + sp) * kc))));
        }
        for (int i = 0; i < 2; ++i)
        {
            fAps[i].setLen(int(std::max(1.0, std::floor(kApLen[i] * k))));
            fAps[2 + i].setLen(int(std::max(1.0, std::floor((kApLen[i] + sp) * k))));
        }
        fBypass = bypass;
        fFreeze = freeze;
        fPdOn = preDelayMs > 0.0;
        fPdLen = int(std::max(1.0, std::min(double(fPdSize - 1), std::floor(preDelayMs * fSr / 1000.0 + 0.5))));
        fWdOn = std::fabs(widthPct - 100.0) >= 0.00001; // EEL2's fuzzy !=
        fWd = widthPct / 100.0;
        fModOn = modPct > 0.0;
        fModC = modPct / 100.0 * 24.0 * fSc;
        fModP = modPct / 100.0 * 12.0 * fSc;
        fModIa = 2.0 * kPi * 0.73 / fSr;
        fModIb = 2.0 * kPi * 1.13 / fSr;
        fLcOn = lowCut > 20.0;
        fLcK.set(lowCut, fSr, true);
        fHcOn = highCut < 20000.0;
        fHcK.set(highCut, fSr, false);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double fb = fP.fb, damp = fP.damp, dampA = fP.dampA, mix = fP.mix;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double origL = inL[i], origR = inR[i];
            double x = (origL + origR) * 0.015;
            fPd[size_t(fPdW)] = x;
            if (fPdOn)
            {
                int r = fPdW - fPdLen;
                if (r < 0)
                    r += fPdSize;
                x = fPd[size_t(r)];
            }
            if (++fPdW >= fPdSize)
                fPdW = 0;
            if (fDiffOn)
                for (int d = 0; d < 4; ++d)
                    x = fDiff[d].diffuse(x, kDiffGain[d]);
            if (fFreeze)
                x = 0.0;

            double wl, wr;
            if (fModOn)
            {
                fModA += fModIa;
                if (fModA >= 2.0 * kPi)
                    fModA -= 2.0 * kPi;
                fModB += fModIb;
                if (fModB >= 2.0 * kPi)
                    fModB -= 2.0 * kPi;
                const double sa = std::sin(fModA), ca = std::cos(fModA);
                const double sb = std::sin(fModB), cb = std::cos(fModB);
                const double mc = fModC, mp = fModP;
                wl = fCombs[0].tickM(x, fb, damp, dampA, mc * (0.5 + 0.5 * sa))
                   + fCombs[1].tickM(x, fb, damp, dampA, mc * (0.5 + 0.5 * ca));
                wl = wl + fCombs[2].tickM(x, fb, damp, dampA, mc * (0.5 - 0.5 * sa));
                wl = wl + fCombs[3].tickM(x, fb, damp, dampA, mc * (0.5 - 0.5 * ca));
                wr = fCombs[4].tickM(x, fb, damp, dampA, mc * (0.5 + 0.5 * ca))
                   + fCombs[5].tickM(x, fb, damp, dampA, mc * (0.5 - 0.5 * sa));
                wr = wr + fCombs[6].tickM(x, fb, damp, dampA, mc * (0.5 - 0.5 * ca));
                wr = wr + fCombs[7].tickM(x, fb, damp, dampA, mc * (0.5 + 0.5 * sa));
                wl = fAps[1].tickM(fAps[0].tickM(wl, mp * (0.5 + 0.5 * sb)), mp * (0.5 + 0.5 * cb));
                wr = fAps[3].tickM(fAps[2].tickM(wr, mp * (0.5 - 0.5 * sb)), mp * (0.5 - 0.5 * cb));
            }
            else
            {
                wl = fCombs[0].tick(x, fb, damp, dampA) + fCombs[1].tick(x, fb, damp, dampA);
                wl = wl + fCombs[2].tick(x, fb, damp, dampA);
                wl = wl + fCombs[3].tick(x, fb, damp, dampA);
                wr = fCombs[4].tick(x, fb, damp, dampA) + fCombs[5].tick(x, fb, damp, dampA);
                wr = wr + fCombs[6].tick(x, fb, damp, dampA);
                wr = wr + fCombs[7].tick(x, fb, damp, dampA);
                wl = fAps[1].tick(fAps[0].tick(wl));
                wr = fAps[3].tick(fAps[2].tick(wr));
            }

            if (fWdOn)
            {
                const double m = (wl + wr) * 0.5;
                const double s = (wl - wr) * 0.5 * fWd;
                wl = m + s;
                wr = m - s;
            }
            if (fLcOn)
            {
                wl = fLc[0].tick(wl, fLcK);
                wr = fLc[1].tick(wr, fLcK);
            }
            else
            {
                fLc[0].clear();
                fLc[1].clear();
            }
            if (fHcOn)
            {
                wl = fHc[0].tick(wl, fHcK);
                wr = fHc[1].tick(wr, fHcK);
            }
            else
            {
                fHc[0].clear();
                fHc[1].clear();
            }

            if (fDuckOn)
            {
                const double x = std::fmax(std::fabs(origL), std::fabs(origR));
                fDkEnv += (x > fDkEnv ? fDkAtt : fDkRel) * (x - fDkEnv);
                const double g = 1.0 - fDuckAmt * std::fmin(1.0, fDkEnv * 4.0);
                wl *= g;
                wr *= g;
            }
            if (fGtOn)
            {
                const double x = std::fmax(std::fabs(origL), std::fabs(origR));
                fGtEnv = x > fGtEnv ? x : fGtEnv * fGtEr;
                if (fGtEnv >= fGtThr)
                    fGtHold = fGtLen;
                else if (fGtHold > 0.0)
                    fGtHold -= 1.0;
                const double t = fGtEnv >= fGtThr || fGtHold > 0.0 ? 1.0 : 0.0;
                fGtG += (t > fGtG ? fGtGa : fGtGr) * (t - fGtG);
                wl *= fGtG;
                wr *= fGtG;
            }

            const double ol = origL * (1.0 - mix) + wl * 3.0 * mix;
            const double orr = origR * (1.0 - mix) + wr * 3.0 * mix;
            outL[i] = static_cast<T>(fBypass ? origL : ol);
            outR[i] = static_cast<T>(fBypass ? origR : orr);
        }
    }

private:
    struct Line {
        std::vector<double> buf;
        int len = 1, w = 0;
        double f = 0.0;
        void reset(int size)
        {
            buf.assign(size_t(size), 0.0);
            w = 0;
            f = 0.0;
        }
        void setLen(int l)
        {
            len = l;
            if (w >= len)
                w = 0;
        }
        void step() noexcept
        {
            if (++w >= len)
                w = 0;
        }
        // the JSFX's rv_tap(): the line read m samples newer than its full length
        double tap(double m) const noexcept
        {
            m = std::min(m, double(len - 2));
            double p = w + std::max(0.0, m);
            if (p >= len)
                p -= len;
            const double fi = std::floor(p);
            const int ii = int(fi);
            int j = ii + 1;
            if (j >= len)
                j = 0;
            const double a = buf[size_t(ii)];
            return a + (buf[size_t(j)] - a) * (p - fi);
        }
        // the JSFX's comb()
        double tick(double x, double fb, double damp, double dampA) noexcept
        {
            const double y = buf[size_t(w)];
            f = y * dampA + f * damp;
            buf[size_t(w)] = x + f * fb;
            step();
            return y;
        }
        // the JSFX's combm()
        double tickM(double x, double fb, double damp, double dampA, double m) noexcept
        {
            const double y = tap(m);
            f = y * dampA + f * damp;
            buf[size_t(w)] = x + f * fb;
            step();
            return y;
        }
        // the JSFX's allpass()
        double tick(double x) noexcept
        {
            const double b = buf[size_t(w)];
            const double y = b - x;
            buf[size_t(w)] = x + b * 0.5;
            step();
            return y;
        }
        // the JSFX's allpassm()
        double tickM(double x, double m) noexcept
        {
            const double b = tap(m);
            const double y = b - x;
            buf[size_t(w)] = x + b * 0.5;
            step();
            return y;
        }
        // the JSFX's diffuse()
        double diffuse(double x, double g) noexcept
        {
            const double d = buf[size_t(w)];
            const double v = x - g * d;
            buf[size_t(w)] = v;
            step();
            return d + g * v;
        }
    };

    // the JSFX's 12 dB/oct Butterworth biquad coefficients and rv_bq() state
    struct BqCoef {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        void set(double fc, double sr, bool highpass) noexcept
        {
            const double w = 2.0 * kPi * std::min(fc, 0.45 * sr) / sr;
            const double al = std::sin(w) / (2.0 * 0.7071067811865476);
            const double c = std::cos(w);
            const double a0 = 1.0 + al;
            if (highpass)
            {
                b0 = (1.0 + c) / 2.0 / a0;
                b1 = -(1.0 + c) / a0;
            }
            else
            {
                b0 = (1.0 - c) / 2.0 / a0;
                b1 = (1.0 - c) / a0;
            }
            b2 = b0;
            a1 = -2.0 * c / a0;
            a2 = (1.0 - al) / a0;
        }
    };
    struct Bq {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void clear() noexcept { x1 = x2 = y1 = y2 = 0.0; }
        double tick(double x, const BqCoef& k) noexcept
        {
            const double y = k.b0 * x + k.b1 * x1 + k.b2 * x2 - k.a1 * y1 - k.a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    double fSr = 48000.0, fSc = 48000.0 / 44100.0;
    int fBlen = 1;
    Line fCombs[8], fAps[4], fDiff[4];
    std::vector<double> fPd = std::vector<double>(1, 0.0);
    int fPdSize = 1, fPdW = 0, fPdLen = 1;
    Params fP {};
    bool fBypass = false, fFreeze = false, fPdOn = false, fDiffOn = false, fWdOn = false, fModOn = false;
    bool fLcOn = false, fHcOn = false;
    double fWd = 1.0, fModC = 0.0, fModP = 0.0, fModIa = 0.0, fModIb = 0.0, fModA = 0.0, fModB = 0.0;
    BqCoef fLcK, fHcK;
    Bq fLc[2], fHc[2];
    bool fDuckOn = false, fGtOn = false;
    double fDuckAmt = 0.0, fDkAtt = 0.0, fDkRel = 0.0, fDkEnv = 0.0;
    double fGtThr = 0.0, fGtLen = 0.0, fGtEr = 0.0, fGtGa = 0.0, fGtGr = 0.0, fGtEnv = 0.0, fGtHold = 0.0, fGtG = 1.0;
};

} // namespace nullreverb
