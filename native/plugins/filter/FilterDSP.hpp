/*
 * NULL JSFX - Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/filter.jsfx, computed in double like EEL2: a stereo TPT
 * state-variable filter (low pass, high pass, band pass, notch) with resonance and output gain.
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
        const double dr = 1 - o * o, di = R * o;
        double nr, ni;
        if (mode == 0) { nr = 1; ni = 0; }
        else if (mode == 1) { nr = -o * o; ni = 0; }
        else if (mode == 2) { nr = 0; ni = R * o; }
        else { nr = 1 - o * o; ni = 0; }
        return 10.0 * std::log10(std::max((nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-20), 1e-20))
             + 20.0 * std::log10(gainLin);
    }
};

class FilterProcessor {
public:
    void setSampleRate(double sr) { fSr = sr; apply(); }
    void reset() { fIc1L = fIc2L = fIc1R = fIc2R = 0.0; }

    void setParams(double cutoff, double resonance, double gainDb, int mode)
    {
        fCutoff = cutoff;
        fRes = resonance;
        fGainDb = gainDb;
        fMode = mode;
        apply();
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            outL[n] = static_cast<T>(tick(inL[n], fIc1L, fIc2L));
            outR[n] = static_cast<T>(tick(inR[n], fIc1R, fIc2R));
        }
    }

private:
    double tick(double x, double& ic1, double& ic2) const noexcept
    {
        const Coefs& k = fK;
        const double v3 = (x - ic2 - (k.R + k.g) * ic1) * k.denom;
        const double v1 = k.g * v3 + ic1;
        const double v2 = k.g * v1 + ic2;
        ic1 = v1;
        ic2 = v2;
        const double y = k.mode == 0 ? v2 : k.mode == 1 ? v3 : k.mode == 2 ? k.R * v1 : v3 + v2;
        return y * k.gainLin;
    }

    void apply() { fK.compute(fCutoff, fRes, fGainDb, fMode, fSr); }

    double fSr = 48000.0, fCutoff = 1000.0, fRes = 1.0, fGainDb = 0.0;
    int fMode = 0;
    Coefs fK;
    double fIc1L = 0, fIc2L = 0, fIc1R = 0, fIc2R = 0;
};

} // namespace nullfilter
