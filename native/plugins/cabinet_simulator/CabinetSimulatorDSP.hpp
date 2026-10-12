/*
 * NULL JSFX - Cabinet Simulator, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/cabinet_simulator.jsfx, computed in double like EEL2: each
 * cabinet is seven biquads (low cut, box resonance, low-mid, mid scoop, presence and a double
 * roll-off); Mic Position darkens the roll-off and softens the presence peak.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

namespace nullcab {

static constexpr int kStages = 7;

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    // the JSFX's biquad(): type 0 high-pass, 1 low-pass, else peaking
    void design(int type, double freq, double q, double gain, double sr) noexcept
    {
        const double w = 2.0 * M_PI * std::min(freq, sr * 0.45) / sr;
        const double c = std::cos(w), s = std::sin(w);
        const double al = s / (2.0 * q), A = std::pow(10.0, gain / 40.0);
        double a0;
        if (type == 0)
        {
            b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al;
        }
        else if (type == 1)
        {
            b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al;
        }
        else
        {
            b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A;
        }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }

    std::complex<double> response(double w) const noexcept
    {
        const std::complex<double> z1 = std::polar(1.0, -w), z2 = std::polar(1.0, -2.0 * w);
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

// the JSFX's @slider: the seven stages for the settings, plus the mix, gain and low cut
struct Setup {
    Biquad st[kStages];
    double mix = 1, out = 1, lowCut = 80;

    void compute(int cab, double micPct, double lowCutHz, double mixPct, double outDb, double sr) noexcept
    {
        const double m = micPct / 100.0;
        // per cabinet: box resonance, low-mid, mid scoop, presence, roll-off frequency
        double res = 85, res_g = 7, lm = 250, lm_g = 2, sc = 650, sc_g = -6, pr = 2000, pr_g = 5, lp = 4500, base_lc = 50;
        switch (cab)
        {
        case 0: res = 110; res_g = 4; lm = 400; lm_g = 1; sc = 900;  sc_g = -3; pr = 2500; pr_g = 5; lp = 5500; base_lc = 70; break;
        case 1: res = 100; res_g = 5; lm = 350; lm_g = 2; sc = 800;  sc_g = -4; pr = 2200; pr_g = 4; lp = 5000; base_lc = 60; break;
        case 2: break;
        case 3: res = 60;  res_g = 6; lm = 200; lm_g = 3; sc = 600;  sc_g = -5; pr = 1500; pr_g = 3; lp = 3000; base_lc = 30; break;
        default: res = 300; res_g = 3; lm = 800; lm_g = 4; sc = 1500; sc_g = -2; pr = 2500; pr_g = 6; lp = 3500; base_lc = 250; break;
        }
        lp *= 1 - 0.45 * m;
        pr_g *= 1 - 0.7 * m;
        lowCut = std::max(lowCutHz, base_lc);
        st[0].design(0, lowCut, 0.707, 0, sr);
        st[1].design(2, res, 1.2, res_g, sr);
        st[2].design(2, lm, 0.9, lm_g, sr);
        st[3].design(2, sc, 1.0, sc_g, sr);
        st[4].design(2, pr, 1.5, pr_g, sr);
        st[5].design(1, lp, 0.6, 0, sr);
        st[6].design(1, lp * 1.3, 0.9, 0, sr);
        mix = mixPct / 100.0;
        out = std::pow(10.0, outDb / 20.0);
    }

    // the whole plugin at f Hz in dB (the GFX's cb_db)
    double responseDb(double f, double sr) const noexcept
    {
        const double w = 2.0 * M_PI * std::min(f, sr * 0.4999) / sr;
        std::complex<double> h = 1.0;
        for (const Biquad& b : st)
            h *= b.response(w);
        h = (1.0 - mix) + mix * h;
        return 10.0 * std::log10(std::max(std::norm(h), 1e-20)) + 20.0 * std::log10(out);
    }
};

class CabinetSimulatorProcessor {
public:
    void setSampleRate(double sr) { fSr = sr; apply(); }
    void reset() { std::fill(&fZ[0][0][0], &fZ[0][0][0] + 2 * kStages * 4, 0.0); }

    void setParams(int cab, double micPct, double lowCutHz, double mixPct, double outDb)
    {
        fCab = cab; fMic = micPct; fLowCut = lowCutHz; fMix = mixPct; fOut = outDb;
        apply();
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            const double wl = chain(xl, fZ[0]), wr = chain(xr, fZ[1]);
            outL[n] = static_cast<T>((xl * (1 - fS.mix) + wl * fS.mix) * fS.out);
            outR[n] = static_cast<T>((xr * (1 - fS.mix) + wr * fS.mix) * fS.out);
        }
    }

private:
    double chain(double y, double (*z)[4]) const noexcept
    {
        for (int i = 0; i < kStages; ++i)
        {
            const Biquad& k = fS.st[i];
            double* s = z[i]; // x1 x2 y1 y2
            const double o = k.b0 * y + k.b1 * s[0] + k.b2 * s[1] - k.a1 * s[2] - k.a2 * s[3];
            s[1] = s[0]; s[0] = y; s[3] = s[2]; s[2] = o;
            y = o;
        }
        return y;
    }

    void apply() { fS.compute(fCab, fMic, fLowCut, fMix, fOut, fSr); }

    double fSr = 48000.0, fMic = 30, fLowCut = 80, fMix = 100, fOut = 0;
    int fCab = 2;
    Setup fS;
    double fZ[2][kStages][4] = {};
};

} // namespace nullcab
