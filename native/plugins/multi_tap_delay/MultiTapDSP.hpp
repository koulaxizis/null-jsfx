/*
 * NULL JSFX - Multi-Tap Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/multi_tap_delay.jsfx, computed in double in the EEL2 order: one
 * stereo line of 4 s per channel (allocated in setSampleRate), up to eight taps placed by Pattern
 * at multiples of the step (Time, or a note at the host tempo, re-read every block when synced),
 * each with its level and pan; the last tap feeds back; the echoes pass a 12 dB/oct low cut and
 * high cut (RBJ biquads) and are mixed with the dry signal.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace nullmultitap {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    int taps = 4, pattern = 0, subdivision = 1;
    bool sync = false;
    double time = 250.0, decay = 30.0, spread = 50.0, feedback = 0.0, lowCut = 20.0, highCut = 20000.0, mix = 35.0;
};

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// mt_bq: kind 0 = high-pass, 1 = low-pass, Q = 1/sqrt(2)
inline Biquad biquad(int kind, double f, double sr) noexcept
{
    Biquad b;
    const double w0 = 2.0 * kPi * std::fmin(f, 0.45 * sr) / sr;
    const double cw = std::cos(w0);
    const double al = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double a0 = 1.0 + al;
    b.b0 = (kind ? (1.0 - cw) / 2.0 : (1.0 + cw) / 2.0) / a0;
    b.b1 = (kind ? 1.0 - cw : -(1.0 + cw)) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - al) / a0;
    return b;
}

// where tap k (1..8) sits, in steps
inline double tapSteps(int pattern, int k) noexcept
{
    switch (pattern)
    {
    case 0: return k;
    case 1: return 1.5 * k;
    case 2: return 2.0 * k / 3.0;
    case 3: return k + (k % 2 == 1 ? 1.0 / 3.0 : 0.0);
    case 4: return (1.0 - std::pow(0.8, double(k))) / 0.2;
    case 5: return (std::pow(1.25, double(k)) - 1.0) / 0.25;
    default:
        return k == 1 ? 0.75 : k == 2 ? 1.5 : k == 3 ? 2.75 : k == 4 ? 3.0 : k == 5 ? 4.25 : k == 6 ? 5.5 : k == 7 ? 6.0 : 7.25;
    }
}

// mt_update: tap positions (samples), left and right gains for a line of n samples; returns the
// number of taps
inline int tapLayout(const Params& p, double sr, double tempo, int n, int* pos, double* gl, double* gr) noexcept
{
    const int s = p.subdivision;
    double step = p.sync && tempo > 0.0
        ? 60000.0 / tempo * (s == 0 ? 1.0 : s == 1 ? 0.5 : s == 2 ? 0.25 : s == 3 ? 0.125 : 2.0)
        : p.time;
    step = step * sr / 1000.0;
    double g = 1.0;
    const double sp = p.spread / 100.0;
    for (int k = 1; k <= 8; ++k)
    {
        const double t = tapSteps(p.pattern, k);
        pos[k - 1] = int(std::fmax(1.0, std::fmin(double(n - 2), std::floor(t * step + 0.5))));
        const double q = (k % 2 == 1 ? -1.0 : 1.0) * sp;
        gl[k - 1] = g * (q <= 0.0 ? 1.0 : 1.0 - q);
        gr[k - 1] = g * (q >= 0.0 ? 1.0 : 1.0 - q);
        g *= 1.0 - p.decay / 100.0;
    }
    return p.taps < 1 ? 1 : p.taps > 8 ? 8 : p.taps;
}

class MultiTapProcessor {
public:
    void setSampleRate(double sr)
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        fN = int(std::floor(4.0 * fSr));
        fL.assign(size_t(fN), 0.0);
        fR.assign(size_t(fN), 0.0);
        fWp = 0;
        coeffs();
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        coeffs();
    }

    void reset() noexcept
    {
        std::fill(fL.begin(), fL.end(), 0.0);
        std::fill(fR.begin(), fR.end(), 0.0);
        fWp = 0;
        fH1l = fH2l = fH1r = fH2r = fL1l = fL2l = fL1r = fL2r = 0.0;
    }

    // @block: a synced step follows the host tempo
    void beginBlock(double tempo) noexcept
    {
        fTempo = tempo;
        if (fP.sync)
            update();
    }

    void processSample(double& l, double& r) noexcept
    {
        if (fN < 4)
            return;
        double wl = 0.0, wr = 0.0, dl = 0.0, dr = 0.0;
        for (int i = 0; i < fTaps; ++i)
        {
            int rp = fWp - fPos[i];
            if (rp < 0)
                rp += fN;
            dl = fL[size_t(rp)];
            dr = fR[size_t(rp)];
            wl += dl * fGl[i];
            wr += dr * fGr[i];
        }
        fL[size_t(fWp)] = l + fFb * dl;
        fR[size_t(fWp)] = r + fFb * dr;
        if (++fWp >= fN)
            fWp = 0;
        if (fLcOn)
        {
            double x = wl;
            wl = fLc.b0 * x + fH1l;
            fH1l = fLc.b1 * x - fLc.a1 * wl + fH2l;
            fH2l = fLc.b2 * x - fLc.a2 * wl;
            x = wr;
            wr = fLc.b0 * x + fH1r;
            fH1r = fLc.b1 * x - fLc.a1 * wr + fH2r;
            fH2r = fLc.b2 * x - fLc.a2 * wr;
        }
        if (fHcOn)
        {
            double x = wl;
            wl = fHc.b0 * x + fL1l;
            fL1l = fHc.b1 * x - fHc.a1 * wl + fL2l;
            fL2l = fHc.b2 * x - fHc.a2 * wl;
            x = wr;
            wr = fHc.b0 * x + fL1r;
            fL1r = fHc.b1 * x - fHc.a1 * wr + fL2r;
            fL2r = fHc.b2 * x - fHc.a2 * wr;
        }
        l += (wl - l) * fMix;
        r += (wr - r) * fMix;
    }

private:
    // the @slider code
    void coeffs() noexcept
    {
        fFb = fP.feedback / 100.0;
        fLcOn = fP.lowCut > 20.0;
        fHcOn = fP.highCut < 20000.0;
        fLc = biquad(0, fP.lowCut, fSr);
        fHc = biquad(1, fP.highCut, fSr);
        fMix = fP.mix / 100.0;
        update();
    }

    // mt_update
    void update() noexcept { fTaps = tapLayout(fP, fSr, fTempo, fN, fPos, fGl, fGr); }

    double fSr = 48000.0, fTempo = 120.0;
    Params fP;
    std::vector<double> fL, fR;
    int fN = 0, fWp = 0, fTaps = 4;
    int fPos[8] = {};
    double fGl[8] = {}, fGr[8] = {};
    double fFb = 0.0, fMix = 0.35;
    bool fLcOn = false, fHcOn = false;
    Biquad fLc, fHc;
    double fH1l = 0.0, fH2l = 0.0, fH1r = 0.0, fH2r = 0.0, fL1l = 0.0, fL2l = 0.0, fL1r = 0.0, fL2r = 0.0;
};

} // namespace nullmultitap
