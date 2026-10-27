/*
 * NULL JSFX - Wah, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/wah.jsfx, computed in double like EEL2: a resonant TPT
 * state-variable band-pass (normalised to 0 dB at its peak, lifted by 1.5 + 1.5 x Resonance)
 * swept on an exponential curve by the pedal. Classic: 400 Hz to 2.2 kHz; Vocal: two formant
 * peaks (300-800 Hz and 900 Hz-2.4 kHz, the second at 0.7); Wide: 200 Hz to 3.5 kHz. The pedal is
 * the Pedal slider smoothed over 15 ms (Manual), an envelope follower mapped over 30 dB below the
 * Sensitivity level (Auto), or a raised-cosine LFO at Rate or one cycle per note at the host
 * tempo, locked to the beat position while the transport plays (LFO, re-read every block as the
 * JSFX's @block does). The wah is then level-matched to its input: both powers smoothed over 1 s,
 * the gain held between -12 and +24 dB, so the pedal changes the tone and not the loudness.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullwah {

static constexpr double kPi = 3.141592653589793;

struct Params {
    double pedal = 50.0, mode = 0.0, model = 0.0, resonance = 60.0, sensitivity = 50.0, attack = 10.0;
    double release = 150.0, rate = 2.0, sync = 0.0, mix = 100.0, output = 0.0;
};

// the filter centre frequencies at pedal position ps (0..1); f2 only for Vocal
inline double centre1(int model, double ps) noexcept
{
    return model == 0 ? 400.0 * std::pow(5.5, ps) : model == 1 ? 300.0 * std::pow(800.0 / 300.0, ps)
                                                                : 200.0 * std::pow(17.5, ps);
}
inline double centre2(double ps) noexcept { return 900.0 * std::pow(2400.0 / 900.0, ps); }
inline double q1For(int model, double res) noexcept
{
    return model == 0 ? 2.0 + res * 8.0 : model == 1 ? 3.0 + res * 7.0 : 1.2 + res * 5.0;
}

// The display (wah_gfx.jsfx draws the same): the plugin's response in dB at f with the pedal at
// ps, from the analog prototype at the pre-warped frequency (exact for the TPT filter)
inline double responseDb(double f, double sr, int model, double res, double ps, double mix, double outDb) noexcept
{
    const double q1 = q1For(model, res), k1 = 1.0 / q1, k2 = 1.0 / (q1 * 1.3), pk = 1.5 + 1.5 * res;
    const double t = std::tan(kPi * std::min(f, 0.49 * sr) / sr);
    double re = 0.0, im = 0.0;
    for (int b = 0; b < (model == 1 ? 2 : 1); ++b)
    {
        const double fc = b == 0 ? centre1(model, ps) : centre2(ps), k = b == 0 ? k1 : k2;
        const double w = t / std::tan(kPi * fc / sr), a = 1.0 - w * w, d = a * a + k * k * w * w;
        const double c = b == 0 ? 1.0 : 0.7;
        re += c * k * k * w * w / d;
        im += c * k * w * a / d;
    }
    re = (1.0 - mix) + mix * pk * re;
    im = mix * pk * im;
    const double mag = std::sqrt(re * re + im * im) * std::pow(10.0, outDb / 20.0);
    return mag > 1e-7 ? 20.0 * std::log10(mag) : -140.0;
}

// TPT state-variable filter; run() returns the band-pass output
struct Svf {
    double ic1 = 0.0, ic2 = 0.0;
    double run(double x, double a1, double a2, double a3)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return v1;
    }
};

class WahProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPs = 0.5;
        fFirst = true;
        fEnv = 0.0;
        fLfoP = 0.0;
        fAL = fAR = fBL = fBR = Svf();
        fPsC = 1.0 - std::exp(-1.0 / (0.015 * sr));
        fLmC = 1.0 - std::exp(-1.0 / sr);
        fLmI = fLmW = 0.0;
        fLmD = std::pow(10.0, -20.0);
        fLmE = std::pow(10.0, -10.0);
    }

    // @slider
    void setParams(const Params& p)
    {
        fPedal = p.pedal / 100.0;
        fMode = int(std::floor(p.mode + 0.5));
        fModel = int(std::floor(p.model + 0.5));
        fRes = p.resonance / 100.0;
        fTopDb = -p.sensitivity / 100.0 * 42.0;
        fAttC = 1.0 - std::exp(-1.0 / (p.attack * 0.001 * fSr));
        fRelC = 1.0 - std::exp(-1.0 / (p.release * 0.001 * fSr));
        fRate = p.rate;
        fSync = int(std::floor(p.sync + 0.5));
        fBpc = fSync == 1 ? 4.0 : fSync == 2 ? 2.0 : fSync == 3 ? 1.0 : fSync == 4 ? 0.5 : 0.25;
        fMix = p.mix / 100.0;
        fOutG = std::pow(10.0, p.output / 20.0);
        fQ1 = q1For(fModel, fRes);
        fQ2 = fQ1 * 1.3;
        fK1 = 1.0 / fQ1;
        fK2 = 1.0 / fQ2;
        fPk = 1.5 + 1.5 * fRes;
    }

    double pedalNow() const noexcept { return fPs; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo, bool playing,
                 double beatPos)
    {
        // @block
        if (fFirst)
        {
            fPs = fPedal;
            fFirst = false;
        }
        const double bpm = tempo > 0.0 ? tempo : 120.0;
        if (fSync > 0)
        {
            fLfoI = bpm / 60.0 / fBpc / fSr;
            if (playing)
                fLfoP = beatPos / fBpc - std::floor(beatPos / fBpc);
        }
        else
            fLfoI = fRate / fSr;

        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            if (fMode == 0)
                fPs += fPsC * (fPedal - fPs);
            else if (fMode == 1)
            {
                const double ex = std::max(std::fabs(xl), std::fabs(xr));
                fEnv += (ex > fEnv ? fAttC : fRelC) * (ex - fEnv);
                const double edb = fEnv > 0.000000001 ? 20.0 * std::log10(fEnv) : -180.0;
                fPs = std::min(1.0, std::max(0.0, (edb - fTopDb + 30.0) / 30.0));
            }
            else
            {
                fLfoP += fLfoI;
                if (fLfoP >= 1.0)
                    fLfoP -= 1.0;
                fPs = 0.5 - 0.5 * std::cos(2.0 * kPi * fLfoP);
            }

            const double f1 = centre1(fModel, fPs);
            double g = std::tan(kPi * std::min(f1, 0.45 * fSr) / fSr);
            double c1 = 1.0 / (1.0 + g * (g + fK1));
            double c2 = g * c1;
            double c3 = g * c2;
            double wl = fAL.run(xl, c1, c2, c3) * fK1;
            double wr = fAR.run(xr, c1, c2, c3) * fK1;
            if (fModel == 1)
            {
                const double f2 = centre2(fPs);
                g = std::tan(kPi * std::min(f2, 0.45 * fSr) / fSr);
                c1 = 1.0 / (1.0 + g * (g + fK2));
                c2 = g * c1;
                c3 = g * c2;
                wl += fBL.run(xl, c1, c2, c3) * fK2 * 0.7;
                wr += fBR.run(xr, c1, c2, c3) * fK2 * 0.7;
            }
            // the level match (fLmD keeps the powers clear of denormals in silence)
            wl *= fPk;
            wr *= fPk;
            fLmI += fLmC * (xl * xl + xr * xr + fLmD - fLmI);
            fLmW += fLmC * (wl * wl + wr * wr + fLmD - fLmW);
            const double lmG = std::min(16.0, std::max(0.25, std::sqrt((fLmI + fLmE) / (fLmW + fLmE))));
            outL[i] = static_cast<T>((xl * (1.0 - fMix) + wl * lmG * fMix) * fOutG);
            outR[i] = static_cast<T>((xr * (1.0 - fMix) + wr * lmG * fMix) * fOutG);
        }
    }

private:
    double fSr = 48000.0, fPs = 0.5, fEnv = 0.0, fLfoP = 0.0, fLfoI = 0.0, fPsC = 0.0;
    double fLmC = 0.0, fLmI = 0.0, fLmW = 0.0, fLmD = 1e-20, fLmE = 1e-10;
    bool fFirst = true;
    double fPedal = 0.5, fRes = 0.6, fTopDb = -21.0, fAttC = 0.0, fRelC = 0.0, fRate = 2.0, fBpc = 0.25;
    double fMix = 1.0, fOutG = 1.0, fQ1 = 6.8, fQ2 = 8.84, fK1 = 0.147, fK2 = 0.113, fPk = 2.4;
    int fMode = 0, fModel = 0, fSync = 0;
    Svf fAL, fAR, fBL, fBR;
};

} // namespace nullwah
