/*
 * NULL JSFX - Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/filter.jsfx, computed in double like EEL2: a stereo TPT
 * state-variable filter (low pass, high pass, band pass, notch) with resonance and output gain.
 * Slope 24 dB adds a second stage (same mode, Q 0.707); Drive soft-saturates the input; the
 * cutoff moves by a sine LFO (free in Hz, or a note value at the host tempo, locked to the
 * beat while playing) and an envelope follower, both in octaves (skipped at depth 0).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullfilter {

// the JSFX's @slider: g, R, 1 / (1 + Rg + g^2), the output gain and the mode
struct Coefs {
    double g = 0, R = 0.5, denom = 1, gainLin = 1;
    int mode = 0;
    bool slope24 = false;
    static constexpr double R2 = 1.4142135623730951; // the second stage (Slope 24 dB): Q 0.707

    void compute(double cutoff, double resonance, double gainDb, int m, double sr) noexcept
    {
        g = std::tan(M_PI * cutoff / sr);
        R = 1.0 / (2.0 * resonance);
        const double g2 = g * g, Rg = R * g;
        denom = 1.0 / (1.0 + Rg + g2);
        gainLin = std::exp(gainDb * 0.11513);
        mode = m;
    }

    // magnitude in dB at f Hz (the GFX's fl_db): the TPT SVF is the bilinear transform of
    // H(s) = {1, s^2, R s, s^2 + 1}[mode] / (s^2 + R s + 1) with s = j tan(pi f / sr) / g
    double magnitudeDb(double f, double sr) const noexcept
    {
        const double o = std::tan(M_PI * std::min(f, sr * 0.4999) / sr) / g;
        double db = stageDb(o, R) + 20.0 * std::log10(gainLin);
        if (slope24)
            db += stageDb(o, R2);
        return db;
    }

    double stageDb(double o, double r) const noexcept
    {
        const double dr = 1 - o * o, di = r * o;
        double nr, ni;
        if (mode == 0) { nr = 1; ni = 0; }
        else if (mode == 1) { nr = -o * o; ni = 0; }
        else if (mode == 2) { nr = 0; ni = r * o; }
        else { nr = 1 - o * o; ni = 0; }
        return 10.0 * std::log10(std::max((nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-20), 1e-20));
    }
};

struct Params {
    double cutoff = 1000.0, resonance = 1.0, gain = 0.0;
    int mode = 0;
    int slope = 0;          // 0 = 12 dB, 1 = 24 dB
    double drive = 0.0;     // %
    double lfoRate = 1.0;   // Hz
    int lfoSync = 0;        // 0 off, 1..9 = 1/1, 1/2, 1/4, 1/8, 1/16, 1/4T, 1/8T, 1/4D, 1/8D
    double lfoDepth = 0.0;  // octaves
    double envDepth = 0.0;  // octaves
    double envAttack = 10.0, envRelease = 150.0; // ms
};

// beats per LFO cycle for an LFO Sync setting
inline double lfoBeats(int sync) noexcept
{
    return sync == 1 ? 4.0 : sync == 2 ? 2.0 : sync == 3 ? 1.0 : sync == 4 ? 0.5 : sync == 5 ? 0.25
         : sync == 6 ? 2.0 / 3.0 : sync == 7 ? 1.0 / 3.0 : sync == 8 ? 1.5 : 0.75;
}

// a slider value back on the JSFX's grid (step 1 / inv), in double
inline double snapTo(double v, double inv) noexcept { return std::floor(v * inv + 0.5) / inv; }

class FilterProcessor {
public:
    void setSampleRate(double sr) { fSr = sr; apply(); }
    void reset()
    {
        fIc1L = fIc2L = fIc1R = fIc2R = 0.0;
        fIc3L = fIc4L = fIc3R = fIc4R = 0.0;
        fPh = 0.0;
        fInc = 0.0;
        fEnv = 0.0;
    }

    void setParams(double cutoff, double resonance, double gainDb, int mode)
    {
        Params p;
        p.cutoff = cutoff;
        p.resonance = resonance;
        p.gain = gainDb;
        p.mode = mode;
        setParams(p);
    }

    void setParams(const Params& p)
    {
        fP = p;
        fP.lfoRate = snapTo(p.lfoRate, 100.0);
        apply();
    }

    const Coefs& coefs() const noexcept { return fK; }
    double modulatedCutoff() const noexcept { return fModOn ? fFm : fP.cutoff; }

    // tempo for LFO Sync; while playing, beatPos (in quarter notes) locks the LFO to the beat
    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo = 120.0,
                 bool playing = false, double beatPos = 0.0)
    {
        // @block
        if (fP.lfoSync > 0)
        {
            const double bpm = tempo > 0.0 ? tempo : 120.0;
            fInc = bpm / (fBpc * fSr * 60.0);
            if (playing)
            {
                fPh = beatPos / fBpc;
                fPh -= std::floor(fPh);
            }
        }
        else
            fInc = fP.lfoRate / fSr;

        const Coefs& k = fK;
        for (uint32_t n = 0; n < frames; ++n)
        {
            double xl = inL[n], xr = inR[n];
            if (fModOn)
            {
                const double lv = std::sin(2.0 * M_PI * fPh);
                fPh += fInc;
                if (fPh >= 1.0)
                    fPh -= 1.0;
                if (fEnvOn)
                {
                    const double ex = std::max(std::fabs(xl), std::fabs(xr));
                    fEnv += (ex > fEnv ? fEnvA : fEnvR) * (ex - fEnv);
                }
                fFm = std::max(20.0, std::min(fFmax, fP.cutoff * std::pow(2.0, fP.lfoDepth * lv + fP.envDepth * std::min(1.0, std::sqrt(fEnv)))));
                fGm = std::tan(M_PI * fFm / fSr);
                fDm = 1.0 / (1.0 + k.R * fGm + fGm * fGm);
                fDm2 = 1.0 / (1.0 + Coefs::R2 * fGm + fGm * fGm);
            }
            if (fDrvOn)
            {
                xl = sat(xl * fDrvG) * fDrvC;
                xr = sat(xr * fDrvG) * fDrvC;
            }
            outL[n] = static_cast<T>(tick(xl, fIc1L, fIc2L, fIc3L, fIc4L));
            outR[n] = static_cast<T>(tick(xr, fIc1R, fIc2R, fIc3R, fIc4R));
        }
    }

private:
    static double sat(double x) noexcept
    {
        x = std::max(-3.0, std::min(3.0, x));
        return x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
    }

    double tick(double x, double& ic1, double& ic2, double& ic3, double& ic4) const noexcept
    {
        const Coefs& k = fK;
        double v3 = (x - ic2 - (k.R + fGm) * ic1) * fDm;
        double v1 = fGm * v3 + ic1;
        double v2 = fGm * v1 + ic2;
        ic1 = v1;
        ic2 = v2;
        double y = k.mode == 0 ? v2 : k.mode == 1 ? v3 : k.mode == 2 ? k.R * v1 : v3 + v2;
        if (k.slope24)
        {
            v3 = (y - ic4 - (Coefs::R2 + fGm) * ic3) * fDm2;
            v1 = fGm * v3 + ic3;
            v2 = fGm * v1 + ic4;
            ic3 = v1;
            ic4 = v2;
            y = k.mode == 0 ? v2 : k.mode == 1 ? v3 : k.mode == 2 ? Coefs::R2 * v1 : v3 + v2;
        }
        return y * k.gainLin;
    }

    void apply()
    {
        fK.compute(fP.cutoff, fP.resonance, fP.gain, fP.mode, fSr);
        fK.slope24 = fP.slope != 0;
        fGm = fK.g;
        fDm = fK.denom;
        fDm2 = 1.0 / (1.0 + Coefs::R2 * fK.g + fK.g * fK.g);
        if (!fK.slope24)
            fIc3L = fIc4L = fIc3R = fIc4R = 0.0;
        fDrvOn = fP.drive > 0.0;
        fDrvG = 1.0 + fP.drive * 0.07;
        fDrvC = 1.0 / std::sqrt(fDrvG);
        fBpc = lfoBeats(fP.lfoSync);
        fEnvOn = std::fabs(fP.envDepth) >= 0.00001;
        if (!fEnvOn)
            fEnv = 0.0;
        fEnvA = 1.0 - std::exp(-1.0 / (fP.envAttack * 0.001 * fSr));
        fEnvR = 1.0 - std::exp(-1.0 / (fP.envRelease * 0.001 * fSr));
        fModOn = fP.lfoDepth > 0.0 || fEnvOn;
        fFmax = std::min(20000.0, fSr * 0.45);
    }

    double fSr = 48000.0;
    Params fP;
    Coefs fK;
    double fGm = 0.0, fDm = 1.0, fDm2 = 1.0, fFm = 1000.0, fFmax = 20000.0;
    bool fDrvOn = false, fEnvOn = false, fModOn = false;
    double fDrvG = 1.0, fDrvC = 1.0, fBpc = 1.0, fEnvA = 0.0, fEnvR = 0.0;
    double fPh = 0.0, fInc = 0.0, fEnv = 0.0;
    double fIc1L = 0, fIc2L = 0, fIc1R = 0, fIc2R = 0, fIc3L = 0, fIc4L = 0, fIc3R = 0, fIc4R = 0;
};

} // namespace nullfilter
