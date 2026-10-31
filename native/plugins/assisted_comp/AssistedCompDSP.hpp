/*
 * NULL JSFX - Assisted Comp: the DSP of assisted_comp.jsfx in double, in the EEL2 order
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Learn listens in 50 ms windows (only windows above -60 dBFS RMS count): a level histogram,
 * the crest factor, the time between onsets and the BS.1770 integrated loudness. After 15 s of
 * signal, or when Learn is turned off after 0.5 s, it writes its measurements into its own
 * sliders 9-14 and sets Learn (slider 1) back to Off, as the JSFX does in its code; the plugin
 * then stores them as its parameter values. The compressor (peak detector, 6 dB soft knee,
 * attack/release gain smoothing, makeup, Mix, Output) is worked out from the sliders and the
 * rule tables (AssistedTargets.hpp) in update(). All buffers are members: nothing is allocated.
 */

#pragma once

#include "AssistedTargets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullassistedcomp {

static constexpr int kNumValues = 14; // slider1..slider14 of assisted_comp.jsfx
static constexpr int kLevelBins = 800; // 0.1 dB from -80 dBFS
static constexpr int kLoudBins = 700;  // 0.1 LU from -70 LUFS

class AssistedCompProcessor {
public:
    // s[i] is slider i+1 (the JSFX's doubles)
    double s[kNumValues] = { 0, 0, 0, 100, 0, 0, 100, 0, 0, -18, -24, 12, 0, -23 };

    // the JSFX's @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fOn = false;
        fPrev = 0;
        fEnv = 1;
        fGr = 0;
        double k = std::tan(M_PI * 1681.974450955533 / sr);
        const double vh = std::pow(10.0, 3.999843853973347 / 20);
        const double vb = std::pow(vh, 0.4996667741545416);
        double q = 0.7071752369554196;
        const double a0 = 1 + k / q + k * k;
        fB0 = (vh + vb * k / q + k * k) / a0;
        fB1 = 2 * (k * k - vh) / a0;
        fB2 = (vh - vb * k / q + k * k) / a0;
        fA1 = 2 * (k * k - 1) / a0;
        fA2 = (1 - k / q + k * k) / a0;
        k = std::tan(M_PI * 38.13547087602444 / sr);
        q = 0.5003270373238773;
        const double c0 = 1 + k / q + k * k;
        fC1 = 2 * (k * k - 1) / c0;
        fC2 = (1 - k / q + k * k) / c0;
        fEf = 1 - std::exp(-1 / (0.005 * sr));
        fEs = 1 - std::exp(-1 / (0.1 * sr));
        update();
    }

    // the JSFX's update(): the compressor from the sliders and the tables
    void update()
    {
        using namespace nullassisted;
        fSrc = std::max(0, std::min(kSources - 1, int(s[1])));
        fTgt = std::max(0, std::min(kTargets - 1, int(s[2])));
        const bool lr = s[8] >= 0.5;
        const double amt = s[3] / 100;
        fRed = kCompReduction[fTgt] * amt;
        fRatio = std::max(1.5, std::min(10.0, kCompRatio[fSrc] + kCompRatioAdd[fTgt]));
        fSlope = 1 - 1 / fRatio;
        fThr = std::max(-60.0, std::min(0.0, s[9] - fRed / fSlope - s[4]));
        fActive = fRed > 0.05 || s[4] > 0;
        const double sp = std::pow(2.0, -s[5]);
        const double t = std::max(0.0, std::min(1.0, (s[11] - 9) / 6));
        fAtt = std::max(0.1, std::min(200.0, (kCompAttackLo + t * (kCompAttackHi - kCompAttackLo)) * kCompAttackMul[fTgt] * sp));
        const double rel = s[12] > 0 ? std::max(kCompReleaseMin, std::min(kCompReleaseMax, kCompRelease * s[12])) : kCompReleaseNone;
        fRel = std::max(10.0, std::min(2000.0, rel * sp));
        fAtc = 1 - std::exp(-1 / (fAtt * fSr / 1000));
        fRtc = 1 - std::exp(-1 / (fRel * fSr / 1000));
        fMk = fActive ? (fRed + std::max(0.0, s[4]) * fSlope) * 0.5 : 0.0;
        fAuto = lr && kCompLoudness[fTgt] < 0
                    ? std::max(-24.0, std::min(24.0, kCompLoudness[fTgt] - (s[13] + fMk - kCompLoudnessShare * 2 * fMk)))
                    : 0.0;
        fMkl = std::pow(10.0, fMk / 20);
        fMix = s[6] / 100;
        fOut = std::pow(10.0, (s[7] + fAuto) / 20);
    }

    // the JSFX's @block: Learn turned on starts, turned off early finishes or gives up.
    // Returns true when the sliders changed (Learn finished).
    bool block()
    {
        bool changed = false;
        if (s[0] >= 0.5 && fPrev < 0.5)
            start();
        if (s[0] < 0.5 && fOn)
        {
            if (fN >= 0.5 * fSr)
            {
                finish();
                changed = true;
            }
            else
                fOn = false;
        }
        fPrev = s[0] >= 0.5 ? 1 : 0;
        return changed;
    }

    // the JSFX's @sample over a block; returns true when Learn finished in it
    bool process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
    {
        bool changed = false;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            if (fOn && listen(l, r))
                changed = true;
            if (fActive)
            {
                const double x = std::max(std::max(std::fabs(l), std::fabs(r)), 0.000001);
                const double ex = 20 * std::log10(x) - fThr;
                const double tg = ex <= 0 ? 0.0 : ex < 6 ? fSlope * ex * ex / 12 : fSlope * (ex - 3);
                const double gt = std::pow(10.0, -tg / 20);
                fEnv += (gt - fEnv) * (gt < fEnv ? fAtc : fRtc);
            }
            else
                fEnv = 1;
            fGr = -20 * std::log10(fEnv);
            const double w0 = l * fEnv * fMkl;
            const double w1 = r * fEnv * fMkl;
            outL[i] = float((l + (w0 - l) * fMix) * fOut);
            outR[i] = float((r + (w1 - r) * fMix) * fOut);
        }
        return changed;
    }

    // output level (dB) for an input level (dB), with makeup, at full Mix (the GFX's cg_curve)
    double curve(double x) const
    {
        const double ex = x - fThr;
        return fActive ? x - (ex <= 0 ? 0.0 : ex < 6 ? fSlope * ex * ex / 12 : fSlope * (ex - 3)) + fMk : x;
    }

    // for the interface
    bool learning() const { return fOn; }
    double progress() const { return fNeed > 0 ? std::min(1.0, fN / fNeed) : 0.0; }
    double heard() const { return fSr > 0 ? fN / fSr : 0.0; }
    double reduction() const { return fGr; }
    bool active() const { return fActive; }
    double threshold() const { return fThr; }
    double ratio() const { return fRatio; }
    double attack() const { return fAtt; }
    double release() const { return fRel; }
    double makeup() const { return fMk; }
    double autoLevel() const { return fAuto; }
    double loudnessTarget() const { return nullassisted::kCompLoudness[fTgt]; }

private:
    void start()
    {
        std::fill(fLh, fLh + kLevelBins, 0.0);
        std::fill(fKc, fKc + kLoudBins, 0.0);
        std::fill(fKp, fKp + kLoudBins, 0.0);
        std::fill(fSub, fSub + 4, 0.0);
        fWin = std::floor(0.05 * fSr);
        fNeed = std::floor(15 * fSr);
        fSublen = std::floor(0.1 * fSr);
        fWn = fWp = fWpk = 0;
        fN = 0;
        fCs = fCn = 0;
        fZ1l = fZ2l = fZ3l = fZ4l = 0;
        fZ1r = fZ2r = fZ3r = fZ4r = 0;
        fKacc = 0; fKn = 0; fKsubs = 0; fKw = 0;
        fFf = fFs = 0;
        fT = 0; fLast = -1; fIs = 0; fIn = 0;
        fOn = true;
    }

    static double round1(double v) { return std::floor(v * 10 + 0.5) / 10; }
    static double pdb(double x) { return x > 0 ? 10 * std::log10(x) : -200.0; }

    // the level under which a share p of the windows lies
    double pct(double p, double total) const
    {
        double c = 0;
        int r = -1;
        for (int i = 0; i < kLevelBins; ++i)
        {
            c += fLh[i];
            if (r < 0 && c >= p * total)
                r = i;
        }
        return -80 + (std::max(0, r) + 0.5) / 10;
    }

    // one sample of the JSFX's learn analysis; returns true when Learn finished
    bool listen(double l, double r)
    {
        // 50 ms windows: level and crest factor
        const double pw = (l * l + r * r) * 0.5;
        fWp += pw;
        fWpk = std::max(fWpk, std::max(std::fabs(l), std::fabs(r)));
        fWn += 1;
        // onsets: a fast envelope jumping 6 dB over a slow one, 50 ms apart at least
        const double x = std::max(std::fabs(l), std::fabs(r));
        fFf += (x - fFf) * fEf;
        fFs += (x - fFs) * fEs;
        if (fFf > 0.003 && fFf > 2 * fFs && (fLast < 0 || fT - fLast >= 0.05 * fSr))
        {
            if (fLast >= 0 && fT - fLast <= 2 * fSr)
            {
                fIs += fT - fLast;
                fIn += 1;
            }
            fLast = fT;
        }
        fT += 1;
        // loudness: K-weighted power in 100 ms sums, a 400 ms block every 100 ms
        double y = fB0 * l + fZ1l;
        fZ1l = fB1 * l - fA1 * y + fZ2l;
        fZ2l = fB2 * l - fA2 * y;
        const double kl = y + fZ3l;
        fZ3l = -2 * y - fC1 * kl + fZ4l;
        fZ4l = y - fC2 * kl;
        y = fB0 * r + fZ1r;
        fZ1r = fB1 * r - fA1 * y + fZ2r;
        fZ2r = fB2 * r - fA2 * y;
        const double kr = y + fZ3r;
        fZ3r = -2 * y - fC1 * kr + fZ4r;
        fZ4r = y - fC2 * kr;
        fKacc += kl * kl + kr * kr;
        fKn += 1;
        if (fKn >= fSublen)
        {
            fSub[fKw] = fKacc;
            fKw = (fKw + 1) % 4;
            fKsubs += 1;
            fKacc = 0;
            fKn = 0;
            if (fKsubs >= 4)
            {
                const double bp = (fSub[0] + fSub[1] + fSub[2] + fSub[3]) / (4 * fSublen);
                const double bl = pdb(bp) - 0.691;
                if (bl > -70)
                {
                    const int bi = int(std::min(double(kLoudBins - 1), std::floor((bl + 70) * 10)));
                    fKc[bi] += 1;
                    fKp[bi] += bp;
                }
            }
        }
        if (fWn >= fWin)
        {
            if (fWp > 0.000001 * fWn)
            {
                const double wr = std::sqrt(fWp / fWn);
                const double lv = 20 * std::log10(wr);
                fLh[int(std::max(0.0, std::min(double(kLevelBins - 1), std::floor((lv + 80) * 10))))] += 1;
                fCs += 20 * std::log10(std::max(fWpk, wr) / wr);
                fCn += 1;
                fN += fWn;
            }
            fWp = 0;
            fWpk = 0;
            fWn = 0;
            if (fN >= fNeed)
            {
                finish();
                return true;
            }
        }
        return false;
    }

    // the measurements, written to the sliders; Learn goes back to Off
    void finish()
    {
        double total = 0;
        for (int i = 0; i < kLevelBins; ++i)
            total += fLh[i];
        s[9] = round1(std::max(-80.0, std::min(0.0, pct(0.9, total))));
        s[10] = round1(std::max(-80.0, std::min(0.0, pct(0.5, total))));
        s[11] = round1(std::max(0.0, std::min(30.0, fCn > 0 ? fCs / fCn : 12.0)));
        s[12] = fIn >= 2 ? std::max(0.0, std::min(2000.0, std::floor(fIs / fIn * 1000 / fSr + 0.5))) : 0.0;
        // integrated loudness: the mean of the blocks over -70 LUFS, then of those over it - 10 LU
        double p = 0, c = 0;
        for (int i = 0; i < kLoudBins; ++i)
        {
            p += fKp[i];
            c += fKc[i];
        }
        if (c > 0)
        {
            const double g = -0.691 + 10 * std::log10(p / c) - 10;
            const int lo = int(std::max(0.0, std::ceil((g + 70) * 10)));
            p = c = 0;
            for (int i = lo; i < kLoudBins; ++i)
            {
                p += fKp[i];
                c += fKc[i];
            }
            s[13] = c > 0 ? round1(std::max(-70.0, std::min(0.0, -0.691 + 10 * std::log10(p / c)))) : -70.0;
        }
        else
            s[13] = -70;
        s[8] = 1;
        s[0] = 0;
        fOn = false;
        fPrev = 0;
        update();
    }

    double fSr = 48000;
    // K-weighting and envelope coefficients
    double fB0 = 1, fB1 = 0, fB2 = 0, fA1 = 0, fA2 = 0, fC1 = 0, fC2 = 0, fEf = 0, fEs = 0;
    // learn state
    bool fOn = false;
    double fPrev = 0;
    double fLh[kLevelBins] = {}, fKc[kLoudBins] = {}, fKp[kLoudBins] = {}, fSub[4] = {};
    double fWin = 2400, fNeed = 720000, fSublen = 4800;
    double fWn = 0, fWp = 0, fWpk = 0, fN = 0, fCs = 0, fCn = 0;
    double fZ1l = 0, fZ2l = 0, fZ3l = 0, fZ4l = 0, fZ1r = 0, fZ2r = 0, fZ3r = 0, fZ4r = 0;
    double fKacc = 0, fKn = 0, fKsubs = 0;
    int fKw = 0;
    double fFf = 0, fFs = 0, fT = 0, fLast = -1, fIs = 0, fIn = 0;
    // the compressor
    int fSrc = 0, fTgt = 0;
    double fRed = 0, fRatio = 2, fSlope = 0.5, fThr = 0, fAtt = 10, fRel = 150, fAtc = 0, fRtc = 0;
    double fMk = 0, fAuto = 0, fMkl = 1, fMix = 1, fOut = 1;
    bool fActive = false;
    double fEnv = 1, fGr = 0;
};

} // namespace nullassistedcomp
