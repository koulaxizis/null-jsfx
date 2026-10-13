/*
 * NULL JSFX - Pitch Correction, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/pitch_correction.jsfx, computed in double like EEL2 and in
 * the same order, so every decision (voiced or not, which note, which pitch mark) comes out
 * the same. YIN pitch tracking on a band-limited mono copy decimated to about 11 kHz, the
 * nearest note of Key + Scale with hysteresis, a correction that glides there at Retune
 * Speed (Humanize keeps part of the vibrato), and a TD-PSOLA shifter: analysis marks one
 * input period apart on the same peak of every cycle, synthesis marks one output period
 * apart, each adding the Hann-windowed two-period grain of the nearest analysis mark.
 * Latency: the pitch frame plus 1.5 periods of 70 Hz (about 39 ms), as the JSFX's pdc_delay.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullpitchcorrection {

class PitchCorrectionProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fDec = std::max(1.0, std::floor(sr / 11025.0 + 0.5));
        fFa = sr / fDec;
        fTmax = std::ceil(fFa / 70.0);
        fTmin = std::max(2.0, std::floor(fFa / 1000.0));
        fHopS = kHop * fDec / sr;
        fHm = std::ceil(sr / 70.0);
        fMlag = fTmax * fDec + std::floor(kHop * fDec / 2.0);
        fLat = fMlag + std::ceil(1.5 * fHm) + 4.0;
        fBs = int(fLat + 2.0 * fHm + 16.0);
        fInL.assign(size_t(fBs), 0.0);
        fInR.assign(size_t(fBs), 0.0);
        fAccL.assign(size_t(fBs), 0.0);
        fAccR.assign(size_t(fBs), 0.0);
        fAccW.assign(size_t(fBs), 0.0);
        fLpb.assign(size_t(fBs), 0.0);
        std::fill(std::begin(fDbuf), std::end(fDbuf), 0.0);
        std::fill(std::begin(fFrm), std::end(fFrm), 0.0);
        std::fill(std::begin(fCmn), std::end(fCmn), 0.0);
        std::fill(std::begin(fAmk), std::end(fAmk), 0.0);

        fHoldN = std::floor(0.06 / fHopS) + 1.0;
        fTuv = std::floor(sr / 200.0);
        fLn2 = std::log(2.0);
        fGd = std::floor(sr * 0.00035 + 0.5);

        fHpC = std::exp(-2.0 * kPi * 50.0 / sr);
        const double lw = 2.0 * kPi * 1200.0 / sr;
        const double lc = std::cos(lw);
        double la = std::sin(lw) / (2.0 * 0.5411961);
        fB0a = (1.0 - lc) / 2.0 / (1.0 + la); fB1a = (1.0 - lc) / (1.0 + la);
        fA1a = -2.0 * lc / (1.0 + la); fA2a = (1.0 - la) / (1.0 + la);
        la = std::sin(lw) / (2.0 * 1.3065630);
        fB0b = (1.0 - lc) / 2.0 / (1.0 + la); fB1b = (1.0 - lc) / (1.0 + la);
        fA1b = -2.0 * lc / (1.0 + la); fA2b = (1.0 - la) / (1.0 + la);
        fHpX = fHpY = 0.0;
        fXa1 = fXa2 = fYa1 = fYa2 = 0.0;
        fXb1 = fXb2 = fYb1 = fYb2 = 0.0;

        fWp = 0; fN = 0.0;
        fDcnt = 0; fDw = 0; fHcnt = 0;
        fPer = fPerA = fTuv; fAmLast = 0.0; fMk = 0; fSNext = 0.0;
        fVoiced = false; fWasV = false; fUvc = fHoldN + 1.0;
        fTf = fTmax; fDet = 0.0; fMs = 0.0; fTgt = 60.0; fSh = fShA = 0.0; fShT = 0.0;
    }

    // @slider (Key, Scale and Transpose are whole numbers; the rest are the slider values)
    void setParams(int key, int scale, double retuneMs, double humanizePct, double transpose, double sensitivityPct,
                   double mixPct)
    {
        static constexpr int kMasks[] = { 4095, 2741, 1453, 2477, 661, 1193, 1257, 1709, 1717 };
        fKey = double(key);
        fMask = kMasks[std::max(0, std::min(8, scale))];
        fRk = retuneMs > 0.0 ? 1.0 - std::exp(-fHopS * 1000.0 / retuneMs) : 1.0;
        fHum = humanizePct / 100.0;
        fHumC = 1.0 - std::exp(-fHopS / 0.15);
        fTransp = transpose;
        fThr = 0.05 + 0.3 * sensitivityPct / 100.0;
        fMix = mixPct / 100.0;
    }

    uint32_t latency() const noexcept { return uint32_t(fLat); }

    // the interface's values: pitched now, detected and target note (MIDI), shift (semitones)
    bool voiced() const noexcept { return fVoiced; }
    double detected() const noexcept { return fDet; }
    double target() const noexcept { return fTgt; }
    double shift() const noexcept { return fSh; }
    double totalShift() const noexcept { return fSh + fTransp; }

    // @sample
    void processSample(double& l, double& r)
    {
        // pitch tracking on a band-limited mono copy, decimated
        const double ax = (l + r) * 0.5;
        fHpY = fHpC * (fHpY + ax - fHpX);
        fHpX = ax;
        if (std::fabs(fHpY) < kTiny)
            fHpY = 0.0;
        double ya = fB0a * fHpY + fB1a * fXa1 + fB0a * fXa2 - fA1a * fYa1 - fA2a * fYa2;
        if (std::fabs(ya) < kTiny)
            ya = 0.0;
        fXa2 = fXa1; fXa1 = fHpY; fYa2 = fYa1; fYa1 = ya;
        double yb = fB0b * ya + fB1b * fXb1 + fB0b * fXb2 - fA1b * fYb1 - fA2b * fYb2;
        if (std::fabs(yb) < kTiny)
            yb = 0.0;
        fXb2 = fXb1; fXb1 = ya; fYb2 = fYb1; fYb1 = yb;
        if (++fDcnt >= int(fDec))
        {
            fDcnt = 0;
            fDbuf[fDw] = yb;
            if (++fDw >= kDbs)
                fDw = 0;
            if (++fHcnt >= kHop)
            {
                fHcnt = 0;
                analyze();
                control();
            }
        }

        // the grains are older than the pitch estimate: the shift one hop late, interpolated
        const double hph = (double(fHcnt) * fDec + double(fDcnt)) / (kHop * fDec);
        fPerI = fPerA + (fPer - fPerA) * hph;
        fShI = fShA + (fSh - fShA) * hph;
        fInL[size_t(fWp)] = l;
        fInR[size_t(fWp)] = r;
        fLpb[size_t(fWp)] = yb;
        // analysis marks one input period apart, as far as the pitch estimate reaches
        while (fAmLast <= fN - fMlag)
        {
            fAmLast += fPer;
            if (fWasV)
                fAmLast = snap(fAmLast);
            if (++fMk >= 64)
                fMk = 0;
            fAmk[fMk] = fAmLast;
        }
        // synthesis marks one output period apart, placed one grain ahead of the output
        while (fSNext - fN <= fHm + 1.0 - fLat)
            fSNext += place(fSNext);

        int ro = fWp - int(fLat);
        if (ro < 0)
            ro += fBs;
        const double wv = std::max(fAccW[size_t(ro)], 0.5);
        const double wetL = fAccL[size_t(ro)] / wv;
        const double wetR = fAccR[size_t(ro)] / wv;
        fAccL[size_t(ro)] = 0.0; fAccR[size_t(ro)] = 0.0; fAccW[size_t(ro)] = 0.0;
        const double dryL = fInL[size_t(ro)];
        const double dryR = fInR[size_t(ro)];
        if (++fWp >= fBs)
            fWp = 0;
        fN += 1.0;

        l = dryL * (1.0 - fMix) + wetL * fMix;
        r = dryR * (1.0 - fMix) + wetR * fMix;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            processSample(l, r);
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    static constexpr double kPi = 3.141592653589793;
    static constexpr double kTiny = 0.000000000000000000000000000001;
    static constexpr int kHop = 64;
    static constexpr int kDbs = 1024;

    // EEL2's x >> n & 1 for the scale mask
    bool inScale(double pc) const noexcept { return ((fMask >> int(pc)) & 1) != 0; }

    // YIN on the last 2 * tmax decimated samples: sets voiced and tf
    void analyze()
    {
        const int tmax = int(fTmax);
        int j = fDw - 2 * tmax;
        if (j < 0)
            j += kDbs;
        for (int i = 0; i < 2 * tmax; ++i)
        {
            fFrm[i] = fDbuf[j];
            if (++j >= kDbs)
                j = 0;
        }
        double e = 0.0;
        for (int i = 0; i < tmax; ++i)
            e += fFrm[i] * fFrm[i];
        fCmn[0] = 1.0;
        double s = 0.0;
        for (int t = 1; t <= tmax; ++t)
        {
            double d = 0.0;
            for (int i = 0; i < tmax; ++i)
            {
                const double k = fFrm[i] - fFrm[i + t];
                d += k * k;
            }
            s += d;
            fCmn[t] = s > 0.0 ? d * double(t) / s : 1.0;
        }
        // the first dip under the threshold, followed down to its minimum
        int tau = 0;
        for (int t = int(fTmin); tau == 0 && t < tmax; ++t)
        {
            if (fCmn[t] < fThr)
            {
                while (t + 1 < tmax && fCmn[t + 1] < fCmn[t])
                    ++t;
                tau = t;
            }
        }
        fVoiced = tau > 0 && e > fTmax * 0.00001;
        if (fVoiced)
        {
            const double a = fCmn[tau - 1], b = fCmn[tau], c = fCmn[tau + 1];
            const double den = a - 2.0 * b + c;
            fTf = den > 0.0 ? double(tau) + 0.5 * (a - c) / den : double(tau);
        }
    }

    // pitch -> target note -> smoothed shift (semitones); keeps the previous hop's values
    void control()
    {
        fPerA = fPer;
        fShA = fSh;
        if (fVoiced)
        {
            fPer = std::min(fTf * fDec, fHm);
            fDet = 69.0 + 12.0 * std::log(fFa / fTf / 440.0) / fLn2;
            double best = 0.0, bd = 100.0;
            double k = std::floor(fDet + 0.5) - 6.0;
            for (int n = 0; n < 13; ++n)
            {
                const double kk = k - fKey;
                const double pc = kk - 12.0 * std::floor(kk / 12.0);
                if (inScale(pc))
                {
                    const double dd = std::fabs(k - fDet);
                    if (dd < bd)
                    {
                        bd = dd;
                        best = k;
                    }
                }
                k += 1.0;
            }
            // stay on the current note until the pitch is 0.7 semitone from it (chromatic)
            if (fWasV)
            {
                const double kk = fTgt - fKey;
                const double pc = kk - 12.0 * std::floor(kk / 12.0);
                if (inScale(pc) && std::fabs(fTgt - fDet) < bd + 0.4)
                    best = fTgt;
            }
            if (!fWasV || best != fTgt)
                fMs = fDet;
            else
                fMs += (fDet - fMs) * fHumC;
            fTgt = best;
            fShT = fTgt - fDet - fHum * (fMs - fDet);
            fWasV = true;
            fUvc = 0.0;
        }
        else
        {
            fUvc += 1.0;
            if (fUvc > fHoldN)
            {
                fWasV = false;
                fShT = 0.0;
                fPer = fTuv;
            }
        }
        fSh += (fShT - fSh) * fRk;
    }

    // a voiced analysis mark goes to the highest peak of the low-passed signal within a
    // quarter period of c, refined between samples
    double snap(double c) const
    {
        const double r = std::floor(fPer / 4.0);
        double j = std::floor(c + 0.5) - r + fGd;
        const double j1 = std::min(std::floor(c + 0.5) + r + fGd, fN - 1.0);
        double bj = -1.0, bv = -1.0;
        int bk = 0;
        int k = fWp + int(j - fN);
        if (k < 0)
            k += fBs;
        for (int n = int(j1 - j + 1.0); n > 0; --n)
        {
            if (fLpb[size_t(k)] > bv)
            {
                bv = fLpb[size_t(k)];
                bj = j;
                bk = k;
            }
            if (++k >= fBs)
                k = 0;
            j += 1.0;
        }
        if (bj < 0.0)
            return c;
        const double ym = fLpb[size_t(bk > 0 ? bk - 1 : fBs - 1)];
        const double yp = fLpb[size_t(bk < fBs - 1 ? bk + 1 : 0)];
        const double den = ym - 2.0 * bv + yp;
        return den < 0.0 && ym <= bv && yp <= bv ? (bj - fGd) + (0.5 * (ym - yp)) / den : bj - fGd;
    }

    // adds the grain for synthesis mark s; returns the output period (next mark distance)
    double place(double s)
    {
        const double t1 = fPerI / std::pow(2.0, (fShI + fTransp) / 12.0);
        const double h = std::min(std::max(fPerI, t1), fHm);
        // the analysis mark nearest to s, walking back from the newest
        int k = fMk;
        double c = fAmk[k];
        int cnt = 0;
        while (c > s && cnt < 63)
        {
            if (--k < 0)
                k += 64;
            const double a = fAmk[k];
            if (a <= s)
            {
                if (s - a <= c - s)
                    c = a;
                cnt = 63;
            }
            else
            {
                c = a;
                ++cnt;
            }
        }
        if (std::fabs(c - s) > fHm)
            c = s;
        // the grain is read at s + (c - s), between samples: linear interpolation
        const double d = std::floor(c - s);
        const double fr = c - s - d;
        const double i0 = std::ceil(s - h), i1 = std::floor(s + h);
        int ri = fWp + int(i0 - fN);
        if (ri < 0)
            ri += fBs;
        int si = ri + int(d);
        if (si < 0)
            si += fBs;
        else if (si >= fBs)
            si -= fBs;
        int s2 = si + 1;
        if (s2 >= fBs)
            s2 = 0;
        double i = i0;
        for (int n = int(i1 - i0 + 1.0); n > 0; --n)
        {
            const double w = 0.5 + 0.5 * std::cos(kPi * (i - s) / h);
            fAccL[size_t(ri)] += w * (fInL[size_t(si)] + (fInL[size_t(s2)] - fInL[size_t(si)]) * fr);
            fAccR[size_t(ri)] += w * (fInR[size_t(si)] + (fInR[size_t(s2)] - fInR[size_t(si)]) * fr);
            fAccW[size_t(ri)] += w;
            if (++ri >= fBs)
                ri = 0;
            si = s2;
            if (++s2 >= fBs)
                s2 = 0;
            i += 1.0;
        }
        return t1;
    }

    double fSr = 48000.0, fDec = 4.0, fFa = 12000.0, fTmax = 172.0, fTmin = 12.0, fHopS = 0.0;
    double fHm = 686.0, fMlag = 0.0, fLat = 0.0;
    int fBs = 1;
    std::vector<double> fInL, fInR, fAccL, fAccR, fAccW, fLpb;
    double fDbuf[kDbs] = {}, fFrm[1024] = {}, fCmn[512] = {}, fAmk[64] = {};
    double fHoldN = 1.0, fTuv = 240.0, fLn2 = 0.6931471805599453, fGd = 17.0;
    double fHpC = 0.0, fB0a = 0.0, fB1a = 0.0, fA1a = 0.0, fA2a = 0.0, fB0b = 0.0, fB1b = 0.0, fA1b = 0.0, fA2b = 0.0;
    double fHpX = 0.0, fHpY = 0.0, fXa1 = 0.0, fXa2 = 0.0, fYa1 = 0.0, fYa2 = 0.0;
    double fXb1 = 0.0, fXb2 = 0.0, fYb1 = 0.0, fYb2 = 0.0;
    int fWp = 0, fDcnt = 0, fDw = 0, fHcnt = 0, fMk = 0;
    double fN = 0.0, fPer = 240.0, fPerA = 240.0, fPerI = 240.0, fAmLast = 0.0, fSNext = 0.0;
    bool fVoiced = false, fWasV = false;
    double fUvc = 0.0, fTf = 0.0, fDet = 0.0, fMs = 0.0, fTgt = 60.0, fSh = 0.0, fShA = 0.0, fShI = 0.0, fShT = 0.0;
    double fKey = 0.0, fRk = 1.0, fHum = 0.0, fHumC = 0.0, fTransp = 0.0, fThr = 0.2, fMix = 1.0;
    int fMask = 4095;
};

} // namespace nullpitchcorrection
