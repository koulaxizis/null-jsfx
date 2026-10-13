/*
 * NULL JSFX - Tape, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tape.jsfx, computed in double like EEL2: wow and flutter as a
 * modulated delay around a fixed 3 ms centre (cubic Hermite read; the dry path reads the line at
 * exactly 3 ms, reported as latency), pre-emphasis, Drive into an asymmetric soft saturation with
 * a little memory of its own error, de-emphasis, a low cut and the head bump that move with
 * Speed, Park-Miller hiss and the high-frequency loss of the tape speed.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulltape {

static constexpr double kPi = 3.141592653589793;
static constexpr double kTiny = 1e-21;
static constexpr double kSatB = 0.1; // bias of the curve: even harmonics
static constexpr double kSatA = 0.2; // memory of the saturator's own error

inline double soft(double v) noexcept { return v / std::sqrt(1.0 + v * v); }
inline double satCurve(double v) noexcept { return soft(v + kSatB) - soft(kSatB); }

// the static curve (Drive and its level match), for the display
inline double satComp(double drive) noexcept
{
    return satCurve(0.25) / satCurve(0.25 * drive) / std::pow(1.0 + kSatB * kSatB, -1.5);
}

// RBJ biquad (normalised), transposed direct form II
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void lp(double sr, double f, double q)
    {
        const double w = 2.0 * kPi * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2.0 * q);
        b0 = (1.0 - std::cos(w)) / 2.0 / (1.0 + al); b1 = (1.0 - std::cos(w)) / (1.0 + al); b2 = b0;
        a1 = -2.0 * std::cos(w) / (1.0 + al); a2 = (1.0 - al) / (1.0 + al);
    }
    void hp(double sr, double f, double q)
    {
        const double w = 2.0 * kPi * f / sr, al = std::sin(w) / (2.0 * q);
        b0 = (1.0 + std::cos(w)) / 2.0 / (1.0 + al); b1 = -(1.0 + std::cos(w)) / (1.0 + al); b2 = b0;
        a1 = -2.0 * std::cos(w) / (1.0 + al); a2 = (1.0 - al) / (1.0 + al);
    }
    void peak(double sr, double f, double q, double db)
    {
        const double w = 2.0 * kPi * f / sr, al = std::sin(w) / (2.0 * q), A = std::pow(10.0, db / 40.0);
        b0 = (1.0 + al * A) / (1.0 + al / A); b1 = -2.0 * std::cos(w) / (1.0 + al / A); b2 = (1.0 - al * A) / (1.0 + al / A);
        a1 = b1; a2 = (1.0 - al / A) / (1.0 + al / A);
    }
    double tick(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::fabs(z1) < kTiny) z1 = 0.0;
        if (std::fabs(z2) < kTiny) z2 = 0.0;
        return y;
    }
    // display: |H| in dB at w (radians per sample)
    double db(double w) const
    {
        const double c1 = std::cos(w), c2 = std::cos(2.0 * w), s1 = std::sin(w), s2 = std::sin(2.0 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return 10.0 * std::log10(std::max((nr * nr + ni * ni) / (dr * dr + di * di), 1e-20));
    }
};

// the playback filters of a Speed and Head Bump (one channel), as the JSFX's @slider sets them
struct Playback {
    Biquad hp, pk, lp;
    void set(double sr, int speed, double bumpDb)
    {
        const double bumpF = speed == 0 ? 45.0 : speed == 1 ? 70.0 : 110.0;
        const double hfF = speed == 0 ? 9000.0 : speed == 1 ? 15000.0 : 22000.0;
        hp.hp(sr, bumpF * 0.4, 0.6);
        pk.peak(sr, bumpF, 1.0, bumpDb);
        lp.lp(sr, hfF, 0.6);
    }
    double db(double f, double sr) const
    {
        const double w = 2.0 * kPi * f / sr;
        return hp.db(w) + pk.db(w) + lp.db(w);
    }
};

class TapeProcessor {
public:
    // the plugin delay (samples) the JSFX reports as pdc_delay
    static int latencyFor(double sr) noexcept { return int(std::floor(0.003 * sr)); }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = 1024;
        while (fSize < int(std::ceil(0.008 * sr)))
            fSize *= 2;
        fMask = fSize - 1;
        fBase = std::floor(0.003 * sr);
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fWow1 = 0.0; fWow2 = 0.3; fFlt1 = 0.0; fFlt2 = 0.6;
        fSeedL = 13579.0;
        fSeedR = 97531.0;
        fSatUL = fSatYL = fSatUR = fSatYR = 0.0;
        fPreL = fPreR = fDeL = fDeR = OnePole();
        fPbL = Playback();
        fPbR = Playback();
    }

    // @slider; s[0..7] = slider1..slider8
    void setParams(const double* s)
    {
        fDrive = std::pow(10.0, s[0] / 20.0);
        const int speed = int(s[1]);
        const double wow = s[3] / 100.0, flutter = s[4] / 100.0;
        fHiss = s[5] / 100.0 * 0.02;
        fOut = std::pow(10.0, s[6] / 20.0);
        fMix = s[7] / 100.0;

        const double fc = speed == 0 ? 2500.0 : speed == 1 ? 3500.0 : 5000.0;
        const double g = 2.0, wc = 2.0 * kPi * fc, k = wc / std::tan(kPi * fc / fSr);
        fEmB0 = (g * k + wc) / (k + wc);
        fEmB1 = (wc - g * k) / (k + wc);
        fEmA1 = (wc - k) / (k + wc);
        setPlayback(fPbL, speed, s[2]);
        setPlayback(fPbR, speed, s[2]);

        fSatComp = satComp(fDrive);
        fWowDepth = wow * 0.002 * fSr / 1.31;
        fFltDepth = flutter * 0.0001 * fSr / 1.4;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double sr = fSr;
        for (uint32_t n = 0; n < frames; ++n)
        {
            fWow1 += 0.55 / sr; if (fWow1 >= 1.0) fWow1 -= 1.0;
            fWow2 += 1.37 / sr; if (fWow2 >= 1.0) fWow2 -= 1.0;
            fFlt1 += 6.9 / sr; if (fFlt1 >= 1.0) fFlt1 -= 1.0;
            fFlt2 += 11.3 / sr; if (fFlt2 >= 1.0) fFlt2 -= 1.0;
            const double dly = fBase + fWowDepth * (std::sin(2.0 * kPi * fWow1) + 0.31 * std::sin(2.0 * kPi * fWow2))
                                     + fFltDepth * (std::sin(2.0 * kPi * fFlt1) + 0.4 * std::sin(2.0 * kPi * fFlt2));
            const double xl = tap(fL, dly), xr = tap(fR, dly);
            const int dp = (fW - int(fBase)) & fMask;
            const double dryL = fL[size_t(dp)], dryR = fR[size_t(dp)];
            fL[size_t(fW)] = inL[n];
            fR[size_t(fW)] = inR[n];
            fW = (fW + 1) & fMask;

            const double ul = pre(fPreL, xl) * fDrive;
            const double ur = pre(fPreR, xr) * fDrive;
            const double yl = satCurve(ul + kSatA * (fSatYL - fSatUL));
            const double yr = satCurve(ur + kSatA * (fSatYR - fSatUR));
            fSatUL = ul; fSatYL = yl;
            fSatUR = ur; fSatYR = yr;
            double wl = de(fDeL, yl * fSatComp);
            double wr = de(fDeR, yr * fSatComp);

            wl = fPbL.pk.tick(fPbL.hp.tick(wl));
            wr = fPbR.pk.tick(fPbR.hp.tick(wr));
            fSeedL *= 16807.0; fSeedL -= std::floor(fSeedL / 2147483647.0) * 2147483647.0;
            fSeedR *= 16807.0; fSeedR -= std::floor(fSeedR / 2147483647.0) * 2147483647.0;
            wl = fPbL.lp.tick(wl + (fSeedL / 1073741824.0 - 1.0) * fHiss);
            wr = fPbR.lp.tick(wr + (fSeedR / 1073741824.0 - 1.0) * fHiss);

            outL[n] = static_cast<T>((dryL * (1.0 - fMix) + wl * fMix) * fOut);
            outR[n] = static_cast<T>((dryR * (1.0 - fMix) + wr * fMix) * fOut);
        }
    }

private:
    struct OnePole { double x1 = 0.0, y1 = 0.0; };

    // keeps the filter states (the JSFX's @slider only sets coefficients)
    void setPlayback(Playback& p, int speed, double bumpDb)
    {
        Playback c;
        c.set(fSr, speed, bumpDb);
        auto copy = [](Biquad& d, const Biquad& s) { d.b0 = s.b0; d.b1 = s.b1; d.b2 = s.b2; d.a1 = s.a1; d.a2 = s.a2; };
        copy(p.hp, c.hp);
        copy(p.pk, c.pk);
        copy(p.lp, c.lp);
    }

    double pre(OnePole& s, double x) const
    {
        const double y = fEmB0 * x + fEmB1 * s.x1 - fEmA1 * s.y1;
        s.x1 = x; s.y1 = y;
        if (std::fabs(s.y1) < kTiny) s.y1 = 0.0;
        return y;
    }
    double de(OnePole& s, double x) const
    {
        const double y = (x + fEmA1 * s.x1 - fEmB1 * s.y1) / fEmB0;
        s.x1 = x; s.y1 = y;
        if (std::fabs(s.y1) < kTiny) s.y1 = 0.0;
        return y;
    }

    // 4-point cubic (Hermite) read pos samples behind the write position
    double tap(const std::vector<double>& b, double pos) const
    {
        double rp = double(fW) - pos;
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

    double fSr = 48000.0;
    std::vector<double> fL, fR;
    int fSize = 1024, fMask = 1023, fW = 0;
    double fBase = 0.0;
    double fWow1 = 0, fWow2 = 0, fFlt1 = 0, fFlt2 = 0, fSeedL = 0, fSeedR = 0;
    double fSatUL = 0, fSatYL = 0, fSatUR = 0, fSatYR = 0;
    OnePole fPreL, fPreR, fDeL, fDeR;
    Playback fPbL, fPbR;
    double fDrive = 1, fHiss = 0, fOut = 1, fMix = 1, fEmB0 = 1, fEmB1 = 0, fEmA1 = 0;
    double fSatComp = 1, fWowDepth = 0, fFltDepth = 0;
};

} // namespace nulltape
