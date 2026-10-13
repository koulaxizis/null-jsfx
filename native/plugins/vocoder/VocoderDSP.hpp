/*
 * NULL JSFX - Vocoder, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/vocoder.jsfx, computed in double like EEL2: a 16-band channel
 * vocoder (100 Hz to 8 kHz, log spaced, two cascaded band-passes per band). The left input is
 * the modulator; the carrier is the right input or a saw, pulse or noise oscillator (the
 * JSFX's Park-Miller generator). Band envelopes follow the modulator with Attack/Release, the
 * carrier bands can be shifted (Formant Shift), the bands are spread across the stereo field
 * (Stereo Width) and a high-passed copy of the voice adds sibilance.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullvocoder {

static constexpr int kBands = 16;

// centre frequency of band i (100 Hz .. 8 kHz)
inline double bandFreq(int i) noexcept { return 100.0 * std::pow(8000.0 / 100.0, double(i) / double(kBands - 1)); }

class VocoderProcessor {
public:
    // @init: memory and variables at 0, the oscillator and the noise seed restart
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (Band& b : fBands)
            b = Band();
        fOsc = 0.0;
        fSibX = fSibY = 0.0;
        fSeed = 12345.0;
    }

    // @slider (filter states are kept)
    void setParams(int carrier, double pitchHz, double formantSt, double attackMs, double releaseMs, double sibPct,
                   double mixPct, double outDb, double widthPct)
    {
        fSrc = carrier;
        fInc = pitchHz / fSr;
        const double shift = std::pow(2.0, formantSt / 12.0);
        fAtt = std::exp(-1.0 / (attackMs / 1000.0 * fSr));
        fRel = std::exp(-1.0 / (releaseMs / 1000.0 * fSr));
        fSib = sibPct / 100.0;
        const double width = widthPct / 100.0;
        fMix = mixPct / 100.0;
        fOut = std::pow(10.0, outDb / 20.0) * 30.0;
        for (int i = 0; i < kBands; ++i)
        {
            Band& b = fBands[i];
            const double f = bandFreq(i);
            setBp(b.mc, f);
            setBp(b.cc, f * shift);
            const double th = M_PI / 4.0 * (1.0 + (i % 2 ? width : -width));
            b.gl = std::sqrt(2.0) * std::cos(th);
            b.gr = std::sqrt(2.0) * std::sin(th);
        }
        fSibC = std::exp(-2.0 * M_PI * 6000.0 / fSr);
    }

    double bandEnv(int i) const noexcept { return fBands[i].env; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            const double mod = xl;
            fOsc += fInc;
            if (fOsc >= 1.0)
                fOsc -= 1.0;
            double car;
            if (fSrc == 0)
                car = xr;
            else if (fSrc == 1)
                car = 2.0 * fOsc - 1.0;
            else if (fSrc == 2)
                car = fOsc < 0.5 ? 1.0 : -1.0;
            else
            {
                fSeed *= 16807.0;
                fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
                car = fSeed / 1073741824.0 - 1.0;
            }

            double accL = 0.0, accR = 0.0;
            for (Band& b : fBands)
            {
                const double m = bp2(b.mc, b.ms, mod);
                double c = bp2(b.cc, b.cs, car);
                const double lev = std::fabs(m);
                b.env = lev > b.env ? fAtt * b.env + (1.0 - fAtt) * lev : fRel * b.env + (1.0 - fRel) * lev;
                c *= b.env;
                accL += c * b.gl;
                accR += c * b.gr;
            }
            fSibY = fSibC * (fSibY + mod - fSibX);
            fSibX = mod;
            const double wetL = accL * fOut + fSibY * fSib;
            const double wetR = accR * fOut + fSibY * fSib;
            outL[n] = static_cast<T>(xl * (1.0 - fMix) + wetL * fMix);
            outR[n] = static_cast<T>(xr * (1.0 - fMix) + wetR * fMix);
        }
    }

private:
    struct Band {
        double mc[3] = {}, cc[3] = {}; // band-pass coefficients: modulator, carrier
        double ms[8] = {}, cs[8] = {}; // two cascaded stages of state each
        double env = 0.0, gl = 0.0, gr = 0.0;
    };

    // set_bp
    void setBp(double* b, double f) const noexcept
    {
        const double w = 2.0 * M_PI * std::min(f, fSr * 0.45) / fSr;
        const double al = std::sin(w) / (2.0 * 6.0);
        const double n = 1.0 / (1.0 + al);
        b[0] = al * n;
        b[1] = -2.0 * std::cos(w) * n;
        b[2] = (1.0 - al) * n;
    }

    // bp2: two cascaded band-passes
    static double bp2(const double* b, double* s, double x) noexcept
    {
        double y = b[0] * (x - s[1]) - b[1] * s[2] - b[2] * s[3];
        s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
        x = y;
        y = b[0] * (x - s[5]) - b[1] * s[6] - b[2] * s[7];
        s[5] = s[4]; s[4] = x; s[7] = s[6]; s[6] = y;
        return y;
    }

    double fSr = 48000.0;
    Band fBands[kBands];
    int fSrc = 1;
    double fInc = 0.0, fAtt = 0.0, fRel = 0.0, fSib = 0.0, fMix = 1.0, fOut = 30.0, fSibC = 0.0;
    double fOsc = 0.0, fSibX = 0.0, fSibY = 0.0, fSeed = 12345.0;
};

} // namespace nullvocoder
