/*
 * NULL JSFX - Vinyl Age, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/vinyl_age.jsfx, computed in double like EEL2: wow (0.5 Hz)
 * and flutter (6 Hz) modulate a delay around a fixed 50 ms centre (the dry path reads the same
 * line at exactly 50 ms, reported as latency), then bitcrush, sample-and-hold downsampling, a
 * 12 dB/oct low-pass, crackle and background noise. RPM sets the wow rate (0.5 Hz at 33) and, at
 * 78, a shellac band; Scratches adds sparse louder clicks; Mono folds the record path to mono. The
 * random numbers are the JSFX's exact Park-Miller generators. Both lines sit in one block like the
 * JSFX's memory.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullvinyl {

inline double parkMiller(double seed) noexcept
{
    seed *= 16807.0;
    return seed - std::floor(seed / 2147483647.0) * 2147483647.0;
}

// the wow rate (Hz) for the RPM option: 33, 45, 78
inline double wowHz(int rpm) noexcept { return rpm == 0 ? 0.5 : rpm == 1 ? 0.675 : 1.17; }

// RBJ biquad (direct form I) for the shellac band
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    // type 0 high-pass, 1 low-pass, 2 peaking; keeps the state
    void set(int type, double f, double q, double db, double sr) noexcept
    {
        const double w = 2.0 * M_PI * f / sr, cs = std::cos(w), al = std::sin(w) / (2.0 * q);
        const double A = std::pow(10.0, db / 40.0);
        double a0;
        if (type == 0) { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = b0; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
        else if (type == 1) { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = b0; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
        else { b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A; }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }
    double tick(double x) noexcept
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

// the wow + flutter delay offset in ms at time t (s) of the LFOs (for the display)
inline double modulationMs(double t, double wowPct, double flutterPct, int rpm = 0) noexcept
{
    const double m = std::sin(2.0 * M_PI * wowHz(rpm) * t) * wowPct / 100.0 * 40.0 + std::sin(2.0 * M_PI * 6.0 * t) * flutterPct / 100.0 * 15.0;
    return std::max(-50.0, std::min(50.0, m));
}

class VinylAgeProcessor {
public:
    // the plugin delay (samples) the JSFX reports as pdc_delay
    static int latencyFor(double sr) noexcept { return int(std::floor(0.05 * sr)); }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 0.15));
        fBase = std::floor(0.05 * sr);
        fMem.assign(size_t(2 * fSize + 2), 0.0);
        fW = 0;
        fLp1L = fLp2L = fLp1R = fLp2R = 0.0;
        fCrackleEnv = 0.0;
        fCrackleCnt = 0.0;
        fSeedL = 12345.0;
        fSeedR = 67890.0;
        fSeedC = 24680.0;
        fWowPh = fFlutterPh = 0.0;
        fSrCnt = 0.0;
        fHoldL = fHoldR = 0.0;
        fSeedS = 86420.0;
        fScCnt = 0.0;
        fScNext = std::floor(0.5 * sr);
        fScE1 = fScE2 = 0.0;
        fScGl = fScGr = 1.0;
        for (Biquad& b : fSh)
            b = Biquad();
    }

    // @slider; s[0..7] = slider1..slider8
    void setParams(const double* s)
    {
        fWow = s[0] / 100.0;
        fFlutter = s[1] / 100.0;
        fCrackle = s[2] / 100.0;
        fNoise = s[3] / 100.0;
        fBits = s[4] < 4.0 ? 4.0 : s[4];
        fMix = s[7] / 100.0;
        fLpCoef = 1.0 - std::exp(-2.0 * M_PI * s[6] / fSr);
        const double d = fSr / s[5];
        fSrDiv = d < 1.0 ? 1.0 : std::floor(d);

        fRpm = int(s[8]);
        fWowHz = fRpm == 1 ? 0.675 : 1.17;
        if (fRpm == 2)
        {
            // shellac: high-pass 300 Hz, low-pass 4.5 kHz, a 1.5 kHz bump (L: 0..2, R: 3..5)
            for (int c = 0; c < 2; ++c)
            {
                fSh[3 * c].set(0, 300.0, 0.7, 0.0, fSr);
                fSh[3 * c + 1].set(1, 4500.0, 0.7, 0.0, fSr);
                fSh[3 * c + 2].set(2, 1500.0, 0.8, 4.0, fSr);
            }
        }
        fScratch = s[9];
        fMono = s[10] >= 0.5;
        fScD1 = std::exp(-1.0 / (0.0002 * fSr));
        fScD2 = std::exp(-1.0 / (0.004 * fSr));
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        double* const bl = fMem.data();
        double* const br = bl + fSize;
        const double sz = double(fSize);
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double inl = inL[n], inr = inR[n];

            if (fRpm == 0)
                fWowPh += 2.0 * M_PI * 0.5 / fSr;
            else
                fWowPh += 2.0 * M_PI * fWowHz / fSr;
            if (fWowPh > 2.0 * M_PI)
                fWowPh -= 2.0 * M_PI;
            const double wowVal = std::sin(fWowPh) * fWow;
            fFlutterPh += 2.0 * M_PI * 6.0 / fSr;
            if (fFlutterPh > 2.0 * M_PI)
                fFlutterPh -= 2.0 * M_PI;
            const double flutterVal = std::sin(fFlutterPh) * fFlutter;

            double mod = wowVal * 40.0 + flutterVal * 15.0;
            mod = mod < -50.0 ? -50.0 : (mod > 50.0 ? 50.0 : mod);
            double delay = fBase + mod * 0.001 * fSr;
            delay = delay < 1.0 ? 1.0 : delay;

            double rp = double(fW) - delay;
            if (rp < 0.0)
                rp += sz;
            const double ri = std::floor(rp), rf = rp - ri;
            double rn = ri + 1.0;
            if (rn >= sz)
                rn = 0.0;
            double wl = bl[size_t(ri)] * (1.0 - rf) + bl[size_t(rn)] * rf;
            double wr = br[size_t(ri)] * (1.0 - rf) + br[size_t(rn)] * rf;

            double dp = double(fW) - fBase;
            if (dp < 0.0)
                dp += sz;
            const double dryL = bl[size_t(dp)], dryR = br[size_t(dp)];

            bl[fW] = inl;
            br[fW] = inr;
            if (++fW >= fSize)
                fW = 0;

            // bitcrush
            const double levels = std::pow(2.0, fBits) - 1.0;
            wl = std::floor(wl * levels + 0.5) / levels;
            wr = std::floor(wr * levels + 0.5) / levels;

            // downsample (hold)
            fSrCnt += 1.0;
            if (fSrCnt >= fSrDiv)
            {
                fSrCnt = 0.0;
                fHoldL = wl;
                fHoldR = wr;
            }
            wl = fHoldL;
            wr = fHoldR;

            // 2-pole low-pass
            fLp1L += fLpCoef * (wl - fLp1L);
            fLp2L += fLpCoef * (fLp1L - fLp2L);
            wl = fLp2L;
            fLp1R += fLpCoef * (wr - fLp1R);
            fLp2R += fLpCoef * (fLp1R - fLp2R);
            wr = fLp2R;

            // shellac band (78 RPM)
            if (fRpm == 2)
            {
                wl = fSh[2].tick(fSh[1].tick(fSh[0].tick(wl)));
                wr = fSh[5].tick(fSh[4].tick(fSh[3].tick(wr)));
            }

            // crackle
            fCrackleCnt += 1.0;
            fCrackleEnv *= 0.95;
            if (fCrackleCnt > (fSr / (5.0 + fCrackle * 150.0)) && fCrackle > 0.0)
            {
                fCrackleCnt = 0.0;
                fSeedC = parkMiller(fSeedC);
                fCrackleEnv = (fSeedC / 1073741824.0 - 1.0) * fCrackle * 0.5;
            }
            wl += fCrackleEnv;
            wr += fCrackleEnv;

            // background noise
            fSeedL = parkMiller(fSeedL);
            fSeedR = parkMiller(fSeedR);
            wl += ((fSeedL / 2147483647.0) * 2.0 - 1.0) * fNoise * 0.05;
            wr += ((fSeedR / 2147483647.0) * 2.0 - 1.0) * fNoise * 0.05;

            // scratches (own Park-Miller generator)
            if (fScratch > 0.0)
            {
                fScCnt += 1.0;
                if (fScCnt >= fScNext)
                {
                    fScCnt = 0.0;
                    fSeedS = parkMiller(fSeedS);
                    const double r1 = fSeedS / 2147483648.0;
                    fSeedS = parkMiller(fSeedS);
                    const double r2 = fSeedS / 2147483648.0;
                    fSeedS = parkMiller(fSeedS);
                    const double r3 = fSeedS / 2147483648.0;
                    fScNext = std::floor(fSr * (25.0 + 225.0 * r1) / (30.0 + fScratch));
                    fScE1 = (0.4 + 0.6 * r2) * fScratch / 110.0;
                    if (r3 < 0.5)
                        fScE1 = -fScE1;
                    fScE2 = fScE1 * 0.3;
                    fScGl = 0.7 + 0.6 * r3;
                    fScGr = 1.3 - 0.6 * r3;
                }
                const double v = fScE1 - fScE2;
                fScE1 *= fScD1;
                fScE2 *= fScD2;
                wl += v * fScGl;
                wr += v * fScGr;
            }

            if (fMono)
            {
                wl = (wl + wr) * 0.5;
                wr = wl;
            }

            const double dry = 1.0 - fMix;
            outL[n] = static_cast<T>(dryL * dry + wl * fMix);
            outR[n] = static_cast<T>(dryR * dry + wr * fMix);
        }
    }

private:
    double fSr = 48000.0, fBase = 1.0;
    std::vector<double> fMem;
    int fSize = 1, fW = 0;
    double fLp1L = 0.0, fLp2L = 0.0, fLp1R = 0.0, fLp2R = 0.0;
    double fCrackleEnv = 0.0, fCrackleCnt = 0.0, fSeedL = 12345.0, fSeedR = 67890.0, fSeedC = 24680.0;
    double fWowPh = 0.0, fFlutterPh = 0.0, fSrCnt = 0.0, fHoldL = 0.0, fHoldR = 0.0;
    double fWow = 0.0, fFlutter = 0.0, fCrackle = 0.0, fNoise = 0.0, fBits = 12.0, fMix = 0.0;
    double fLpCoef = 0.0, fSrDiv = 1.0;
    int fRpm = 0;
    bool fMono = false;
    double fWowHz = 0.5, fScratch = 0.0, fScD1 = 0.0, fScD2 = 0.0;
    double fSeedS = 86420.0, fScCnt = 0.0, fScNext = 0.0, fScE1 = 0.0, fScE2 = 0.0, fScGl = 1.0, fScGr = 1.0;
    Biquad fSh[6];
};

} // namespace nullvinyl
