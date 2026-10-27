/*
 * NULL JSFX - Multiband Comp, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/multiband_compressor.jsfx, computed in double like EEL2. Three
 * crossovers split the signal into four bands: each split low-passes the remainder with four
 * cascaded one-pole stages, the band above is what is left, so the bands always sum back to
 * the input. Each band has a stereo-linked peak compressor (threshold, ratio, makeup, soft
 * knee) with shared attack and release, and a mode: active, bypass (no compression), mute or
 * solo. A master gain follows.
 * Appended in 2.7: Channels (Mid / Side: only that half of the mid/side pair is compressed, on
 * both channels, the other half is added back after) and Listen Delta (each band minus its
 * compressed self, before Makeup and Master Output; the modes still mute, solo and bypass).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullmultiband {

static constexpr double kPi = 3.14159265358979;   // the JSFX's PI
static constexpr double kLn10 = 2.30258509299405; // the JSFX's LN10

struct Band {
    double thr = -20.0, ratio = 3.0, makeupDb = 0.0;
    int mode = 0; // 0 active, 1 bypass, 2 mute, 3 solo
};

struct Params {
    double xover[3] = { 120.0, 1000.0, 6000.0 };
    Band band[4];
    double attackMs = 10.0, releaseMs = 100.0, kneePct = 0.0, masterDb = 0.0;
    int channels = 0; // 0 stereo, 1 mid, 2 side
    bool delta = false;
};

// the crossovers actually used (the JSFX's @slider limits): each at least 1.5x the one below,
// the top one below 0.45 x srate
inline void effectiveXovers(const double in[3], double sr, double out[3]) noexcept
{
    out[0] = std::max(50.0, std::min(1000.0, in[0]));
    out[1] = std::max(out[0] * 1.5, std::max(500.0, std::min(4000.0, in[1])));
    out[2] = std::max(out[1] * 1.5, std::max(4000.0, std::min(12000.0, in[2])));
    out[2] = std::min(out[2], 0.45 * sr);
}

class MultibandProcessor {
public:
    // @init: state cleared, envelopes at unity gain
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (auto& s : fX)
            for (double& v : s)
                v = 0.0;
        for (double& e : fEnv)
            e = 1.0;
        setParams(fP);
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        const double db2lin = kLn10 / 20.0;
        double xo[3];
        effectiveXovers(p.xover, fSr, xo);
        for (int k = 0; k < 3; ++k)
            fAlpha[k] = 1.0 - std::exp(-2.0 * kPi * xo[k] / fSr);
        const double att = std::max(0.0, p.attackMs), rel = std::max(0.0, p.releaseMs);
        fAtt = att > 0.0 ? 1.0 - std::exp(-1000.0 / (att * fSr)) : 1.0;
        fRel = rel > 0.0 ? 1.0 - std::exp(-1000.0 / (rel * fSr)) : 1.0;
        for (int b = 0; b < 4; ++b)
        {
            const double ratio = std::max(1.0, std::min(20.0, p.band[b].ratio));
            fThr[b] = p.band[b].thr;
            fMk[b] = std::exp(p.band[b].makeupDb * db2lin);
            fRf[b] = 1.0 - (1.0 / ratio);
            fMode[b] = p.band[b].mode;
        }
        fMaster = std::exp(p.masterDb * db2lin);
        fKneeW = std::max(0.0, std::min(100.0, p.kneePct)) * 0.6;
    }

    // envelope gain of band b (1 = no reduction)
    double env(int b) const noexcept { return fEnv[b]; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double db2lin = kLn10 / 20.0;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double s0 = inL[i], s1 = inR[i], other = 0.0;
            if (fP.channels != 0)
            {
                const double m = (s0 + s1) * 0.5;
                const double s = (s0 - s1) * 0.5;
                if (fP.channels == 1)
                {
                    s0 = s1 = m;
                    other = s;
                }
                else
                {
                    s0 = s1 = s;
                    other = m;
                }
            }
            double bl[4], br[4];
            double remL = s0, remR = s1;
            for (int k = 0; k < 3; ++k)
            {
                const double a = fAlpha[k];
                double* x = fX[k];
                x[0] = x[0] + a * (remL - x[0]);
                x[1] = x[1] + a * (x[0] - x[1]);
                x[2] = x[2] + a * (x[1] - x[2]);
                x[3] = x[3] + a * (x[2] - x[3]);
                x[4] = x[4] + a * (remR - x[4]);
                x[5] = x[5] + a * (x[4] - x[5]);
                x[6] = x[6] + a * (x[5] - x[6]);
                x[7] = x[7] + a * (x[6] - x[7]);
                bl[k] = x[3];
                br[k] = x[7];
                remL = remL - bl[k];
                remR = remR - br[k];
            }
            bl[3] = remL;
            br[3] = remR;

            double ol[4], orr[4];
            for (int b = 0; b < 4; ++b)
            {
                const double al = bl[b] < 0 ? -bl[b] : bl[b];
                const double ar = br[b] < 0 ? -br[b] : br[b];
                const double peak = al > ar ? al : ar;
                const double peakDb = peak > 0 ? std::log(peak) / kLn10 * 20.0 : -200.0;
                const double ks = fThr[b] - fKneeW, ke = fThr[b] + fKneeW;
                const double over = peakDb - fThr[b];
                double gr;
                if (peakDb > ke)
                    gr = over * fRf[b];
                else if (peakDb > ks)
                {
                    const double kp = (peakDb - ks) / (fKneeW * 2.0);
                    gr = over * fRf[b] * kp * kp;
                }
                else
                    gr = 0.0;
                gr = gr > 60.0 ? 60.0 : gr;
                const double g = std::exp(-gr * db2lin);
                if (g < fEnv[b])
                    fEnv[b] = fEnv[b] + (g - fEnv[b]) * fAtt;
                else
                    fEnv[b] = fEnv[b] + (g - fEnv[b]) * fRel;
                if (fMode[b] == 1)
                {
                    ol[b] = bl[b] * fMk[b];
                    orr[b] = br[b] * fMk[b];
                }
                else
                {
                    ol[b] = bl[b] * fEnv[b] * fMk[b];
                    orr[b] = br[b] * fEnv[b] * fMk[b];
                }
            }
            if (fP.delta)
                for (int b = 0; b < 4; ++b)
                {
                    ol[b] = fMode[b] == 1 ? 0.0 : bl[b] - bl[b] * fEnv[b];
                    orr[b] = fMode[b] == 1 ? 0.0 : br[b] - br[b] * fEnv[b];
                }

            const bool anySolo = fMode[0] == 3 || fMode[1] == 3 || fMode[2] == 3 || fMode[3] == 3;
            double l, r;
            if (anySolo)
            {
                l = 0.0;
                r = 0.0;
                for (int b = 0; b < 4; ++b)
                    if (fMode[b] == 3)
                    {
                        l = l + ol[b];
                        r = r + orr[b];
                    }
            }
            else
            {
                l = ol[0] + ol[1] + ol[2] + ol[3];
                r = orr[0] + orr[1] + orr[2] + orr[3];
                for (int b = 0; b < 4; ++b)
                    if (fMode[b] == 2)
                    {
                        l = l - ol[b];
                        r = r - orr[b];
                    }
            }
            if (!fP.delta)
            {
                l = l * fMaster;
                r = r * fMaster;
            }
            if (fP.channels != 0)
            {
                // back to left / right (Delta: the processed half alone)
                if (fP.delta)
                    other = 0.0;
                const double x = l;
                if (fP.channels == 1)
                {
                    l = x + other;
                    r = x - other;
                }
                else
                {
                    l = other + x;
                    r = other - x;
                }
            }
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    Params fP;
    double fSr = 48000.0;
    double fX[3][8] = {};
    double fEnv[4] = { 1.0, 1.0, 1.0, 1.0 };
    double fAlpha[3] = {}, fAtt = 1.0, fRel = 1.0, fThr[4] = {}, fMk[4] = {}, fRf[4] = {};
    int fMode[4] = {};
    double fMaster = 1.0, fKneeW = 0.0;
};

} // namespace nullmultiband
