/*
 * NULL JSFX - Crossfeed, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/crossfeed.jsfx, computed in double in the EEL2 order.
 * Crossfeed: Bauer's filter as libbs2b computes it (a first-order low pass at Cutoff, Feed dB
 * down, to the other ear; a high boost on the own ear). Speakers: Brown-Duda head shadow for
 * the near and the far ear, Woodworth's interaural time on the far ear (linear interpolation),
 * six early reflections per ear for Room. Then Mix and Output. No latency.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullxfeed {

struct Params {
    bool speakers = false;
    double cutoff = 700.0, feed = 4.5, angle = 30.0, room = 20.0, mix = 100.0, output = 0.0;
};

inline Params params(const double* v)
{
    Params p;
    p.speakers = v[0] >= 0.5;
    p.cutoff = v[1];
    p.feed = v[2];
    p.angle = v[3];
    p.room = v[4];
    p.mix = v[5];
    p.output = v[6];
    return p;
}

// first-order filter coefficients of the display and the audio: (b0 + b1 z^-1) / (1 + a1 z^-1)
struct First { double b0 = 1.0, b1 = 0.0, a1 = 0.0; };

// what @slider computes
struct Coefs {
    double a0lo = 0.0, b1lo = 0.0, a0hi = 1.0, a1hi = 0.0, b1hi = 0.0, gain = 1.0;
    First nearEar, farEar;
    double itd = 0.0, room = 0.0, mix = 1.0, out = 1.0;
    bool speakers = false;

    // magnitude in dB at f of each ear's own side (which = 0) or the other side (which = 1), as
    // xg_path in crossfeed_gfx.jsfx
    double pathDb(int which, double f, double sr) const
    {
        double g, b0, b1, a1;
        if (speakers)
        {
            const First& h = which ? farEar : nearEar;
            g = 0.5; b0 = h.b0; b1 = h.b1; a1 = h.a1;
        }
        else if (which)
        {
            g = gain; b0 = a0lo; b1 = 0.0; a1 = -b1lo;
        }
        else
        {
            g = gain; b0 = a0hi; b1 = a1hi; a1 = -b1hi;
        }
        const double w = 2.0 * M_PI * std::fmin(f, sr * 0.499) / sr;
        const double nr = b0 + b1 * std::cos(w), ni = -b1 * std::sin(w);
        const double dr = 1.0 + a1 * std::cos(w), di = -a1 * std::sin(w);
        return 10.0 * std::log10(std::fmax(g * g * (nr * nr + ni * ni) / (dr * dr + di * di), 0.0000000001));
    }
};

inline double alphaAt(double phi) { return 1.05 + 0.95 * std::cos(phi / 150.0 * M_PI); }

inline First headShadow(double al, double sr)
{
    const double be = 2.0 * 343.0 / 0.0875, k = 2.0 * sr;
    First h;
    h.b0 = (al * k + be) / (k + be);
    h.b1 = (be - al * k) / (k + be);
    h.a1 = (be - k) / (k + be);
    return h;
}

inline Coefs coefs(const Params& p, double sr)
{
    Coefs c;
    c.speakers = p.speakers;
    const double gblo = p.feed * -5.0 / 6.0 - 3.0, gbhi = p.feed / 6.0 - 3.0;
    const double glo = std::pow(10.0, gblo / 20.0), ghi = 1.0 - std::pow(10.0, gbhi / 20.0);
    const double fchi = p.cutoff * std::pow(2.0, (gblo - 20.0 * std::log10(ghi)) / 12.0);
    double x = std::exp(-2.0 * M_PI * p.cutoff / sr);
    c.b1lo = x;
    c.a0lo = glo * (1.0 - x);
    x = std::exp(-2.0 * M_PI * fchi / sr);
    c.b1hi = x;
    c.a0hi = 1.0 - ghi * (1.0 - x);
    c.a1hi = -x;
    c.gain = 1.0 / (1.0 - ghi + glo);
    const double an = alphaAt(90.0 - p.angle), af = alphaAt(90.0 + p.angle);
    c.nearEar = headShadow(2.0 * an / (an + af), sr);
    c.farEar = headShadow(2.0 * af / (an + af), sr);
    const double th = p.angle / 180.0 * M_PI;
    c.itd = 0.0875 / 343.0 * (th + std::sin(th)) * sr;
    c.room = p.room / 100.0 * 0.5;
    c.mix = p.mix / 100.0;
    c.out = std::pow(10.0, p.output / 20.0);
    return c;
}

class CrossfeedProcessor {
public:
    static constexpr int kDelay = 256, kRoom = 8192;

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (int i = 0; i < kDelay; ++i)
            fDL[i] = fDR[i] = 0.0;
        for (int i = 0; i < kRoom; ++i)
            fRL[i] = fRR[i] = 0.0;
        fW = fRw = 0;
        fLoL = fLoR = fHiL = fHiR = fAsL = fAsR = 0.0;
        fNl1 = fNr1 = fYnl = fYnr = 0.0;
        fFl1 = fFr1 = fYfl = fYfr = 0.0;
        fRlpL = fRlpR = 0.0;
        fRlpA = std::exp(-2.0 * M_PI * std::fmin(5000.0, sr * 0.45) / sr);
        for (int i = 0; i < 6; ++i)
        {
            fTapL[i] = int(std::floor(kTaps[i][0] * sr / 1000.0 + 0.5));
            fTapR[i] = int(std::floor(kTaps[i][1] * sr / 1000.0 + 0.5));
        }
    }

    // @slider
    void setParams(const Params& p)
    {
        fC = coefs(p, fSr);
        fItdI = int(std::floor(fC.itd));
        fItdF = fC.itd - fItdI;
    }

    const Coefs& current() const noexcept { return fC; }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        const double inl = l, inr = r;
        double wl, wr;
        if (fC.speakers)
        {
            const First& n = fC.nearEar;
            const First& f = fC.farEar;
            fYnl = n.b0 * inl + n.b1 * fNl1 - n.a1 * fYnl;
            fYnr = n.b0 * inr + n.b1 * fNr1 - n.a1 * fYnr;
            fNl1 = inl;
            fNr1 = inr;
            fDL[fW] = inl;
            fDR[fW] = inr;
            const int a = (fW - fItdI + kDelay) % kDelay, b = (a + kDelay - 1) % kDelay;
            const double dl = fDL[a] + (fDL[b] - fDL[a]) * fItdF;
            const double dr = fDR[a] + (fDR[b] - fDR[a]) * fItdF;
            fW = (fW + 1) % kDelay;
            fYfl = f.b0 * dl + f.b1 * fFl1 - f.a1 * fYfl;
            fYfr = f.b0 * dr + f.b1 * fFr1 - f.a1 * fYfr;
            fFl1 = dl;
            fFr1 = dr;
            wl = (fYnl + fYfr) * 0.5;
            wr = (fYnr + fYfl) * 0.5;
            fRL[fRw] = inl;
            fRR[fRw] = inr;
            double el = 0.0, er = 0.0;
            for (int i = 0; i < 6; ++i)
            {
                const int ia = (fRw - fTapL[i] + kRoom) % kRoom, ib = (fRw - fTapR[i] + kRoom) % kRoom;
                el += kTaps[i][2] * (kTaps[i][3] != 0.0 ? fRR[ia] : fRL[ia]);
                er += kTaps[i][2] * (kTaps[i][3] != 0.0 ? fRL[ib] : fRR[ib]);
            }
            fRw = (fRw + 1) % kRoom;
            fRlpL = el + (fRlpL - el) * fRlpA;
            fRlpR = er + (fRlpR - er) * fRlpA;
            wl += fRlpL * fC.room;
            wr += fRlpR * fC.room;
        }
        else
        {
            fLoL = fC.a0lo * inl + fC.b1lo * fLoL;
            fLoR = fC.a0lo * inr + fC.b1lo * fLoR;
            fHiL = fC.a0hi * inl + fC.a1hi * fAsL + fC.b1hi * fHiL;
            fHiR = fC.a0hi * inr + fC.a1hi * fAsR + fC.b1hi * fHiR;
            fAsL = inl;
            fAsR = inr;
            wl = (fHiL + fLoR) * fC.gain;
            wr = (fHiR + fLoL) * fC.gain;
        }
        l = (inl + (wl - inl) * fC.mix) * fC.out;
        r = (inr + (wr - inr) * fC.mix) * fC.out;
    }

private:
    // room taps: delay ms (left ear, right ear), gain, source side (1 = the other side)
    static constexpr double kTaps[6][4] = {
        { 4.3, 4.67, 0.62, 0.0 },   { 6.1, 5.81, 0.53, 1.0 },   { 8.9, 9.43, 0.45, 0.0 },
        { 11.4, 10.99, 0.37, 1.0 }, { 15.7, 16.31, 0.29, 0.0 }, { 21.2, 20.73, 0.22, 1.0 },
    };

    double fSr = 44100.0;
    Coefs fC;
    double fDL[kDelay] = {}, fDR[kDelay] = {};
    double fRL[kRoom] = {}, fRR[kRoom] = {};
    int fW = 0, fRw = 0, fItdI = 0;
    double fItdF = 0.0;
    int fTapL[6] = {}, fTapR[6] = {};
    double fLoL = 0.0, fLoR = 0.0, fHiL = 0.0, fHiR = 0.0, fAsL = 0.0, fAsR = 0.0;
    double fNl1 = 0.0, fNr1 = 0.0, fYnl = 0.0, fYnr = 0.0;
    double fFl1 = 0.0, fFr1 = 0.0, fYfl = 0.0, fYfr = 0.0;
    double fRlpL = 0.0, fRlpR = 0.0, fRlpA = 0.0;
};

} // namespace nullxfeed
