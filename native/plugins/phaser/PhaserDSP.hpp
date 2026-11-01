/*
 * NULL JSFX - Phaser, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/phaser.jsfx, computed in double like EEL2: 1 to 8 first-order
 * allpass stages per channel whose frequency a sine LFO sweeps around Center (1020 Hz) by a
 * half-width set by Depth (57% = 580 Hz, the classic 440 Hz to 1.6 kHz sweep), in a feedback loop
 * (30% by default, -95..95%); the allpass output is added to the dry signal (Mix). Stereo Phase
 * runs the right channel's LFO ahead; Tempo Sync locks the LFO to the host tempo and, while
 * playing, to the beat position.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullphaser {

static constexpr double kTwoPi = 2.0 * M_PI; // 2 * $pi

// one LFO cycle in beats for a Tempo Sync option (1..11; 0 = Off)
inline double syncBeats(int s) noexcept
{
    return s == 1 ? 4.0 : s == 2 ? 2.0 : s == 3 ? 1.0 : s == 4 ? 0.5 : s == 5 ? 0.25 : s == 6 ? 8.0
         : s == 7 ? 16.0 : s == 8 ? 2.0 / 3.0 : s == 9 ? 1.0 / 3.0 : s == 10 ? 1.5 : 0.75;
}

// |dry + mixW * wet| in dB at f Hz for the allpass coefficient c (the GFX's ph_db): each stage
// is (c - z^-1) / (1 - c z^-1), the chain sits in a loop with feedback over one sample
inline double responseDb(double f, double sr, double c, int stages, double feedback, double mixW) noexcept
{
    const double w = 2.0 * M_PI * f / sr;
    const double th = stages * (std::atan2(std::sin(w), c - std::cos(w)) - std::atan2(c * std::sin(w), 1.0 - c * std::cos(w)));
    const double dr = 1.0 - feedback * std::cos(th - w), di = -feedback * std::sin(th - w);
    const double mg = dr * dr + di * di;
    const double cr = 1.0 + mixW * (std::cos(th) * dr + std::sin(th) * di) / mg;
    const double ci = mixW * (std::sin(th) * dr - std::cos(th) * di) / mg;
    return 10.0 * std::log10(std::fmax(cr * cr + ci * ci, 1e-12));
}

// one allpass voice of the barber pole: up to 8 stages in a loop with feedback (the JSFX's pb_run)
struct BarberVoice {
    double y[8] = {}, x[8] = {}, last = 0.0;
    double run(double in, double c, int stages, double feedback) noexcept
    {
        double w = in + last * feedback;
        for (int k = 0; k < stages && k < 8; ++k)
        {
            y[k] = c * (y[k] + w) - x[k];
            x[k] = w;
            w = y[k];
        }
        last = w;
        return w;
    }
};

class PhaserProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPhase = 0.0;
        for (int k = 0; k < 8; ++k)
            fLy[k] = fLx[k] = fRy[k] = fRx[k] = 0.0;
        fLastL = fLastR = 0.0;
        fBla = fBlb = fBra = fBrb = BarberVoice();
    }

    // @slider
    void setParams(double rateHz, double stages, double mixPct, double depthPct = 57.0, double feedbackPct = 30.0,
                   double centerHz = 1020.0, double stereoDeg = 0.0, int sync = 0, int barber = 0)
    {
        fBarber = barber;
        fStDeg = stereoDeg;
        fStages = int(std::floor(stages));
        fMix = mixPct / 100.0;
        fInc = 2.0 * M_PI * rateHz / fSr;
        // the sweep: Center +- a half-width that grows with Depth (580 Hz at the defaults)
        const double hw = 580.0 * (centerHz / 1020.0) * (depthPct / 57.0);
        double dMin = (centerHz - hw) * 2.0 / fSr;
        const double dMax = (centerHz + hw) * 2.0 / fSr;
        fDSc = std::fabs(dMax - dMin) * 0.5;
        dMin = dMin < dMax ? dMin : dMax;
        fDMin = dMin;
        fFeedback = feedbackPct / 100.0;
        // above 30% the wet level is pulled down so the feedback peaks stay near the 30% level
        fMixW = std::fabs(fFeedback) > 0.3 ? fMix * (1.0 - std::fabs(fFeedback)) / 0.7 : fMix;
        fStOn = stereoDeg > 0.0;
        fStOff = stereoDeg / 180.0 * M_PI;
        fSync = sync;
        fSyncBeats = syncBeats(sync);
    }

    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle
    double stereoOffset() const noexcept { return fStOff; }

    // the allpass coefficient at LFO phase ph (radians), as @sample computes it
    double coefAt(double ph) const noexcept
    {
        const double d = fDMin + fDSc * (std::sin(ph) + 1.0);
        return (1.0 - d) / (1.0 + d);
    }
    // Barber Pole: the sweep position (0..1) of the first voice at LFO phase ph (radians), and
    // the coefficient there
    int barber() const noexcept { return fBarber; }
    double barberPos(double ph, bool right) const noexcept
    {
        double p = ph / kTwoPi;
        if (fBarber == 2) p = 1.0 - p;
        if (p >= 1.0) p -= 1.0;
        if (right)
        {
            p += fStDeg / 360.0;
            if (p >= 1.0) p -= 1.0;
        }
        return p;
    }
    double barberCoef(double p) const noexcept
    {
        const double d = fDMin + 2.0 * fDSc * p;
        return (1.0 - d) / (1.0 + d);
    }
    double sweepLoHz() const noexcept { return fDMin * fSr / 2.0; }
    double sweepHiHz() const noexcept { return (fDMin + 2.0 * fDSc) * fSr / 2.0; }
    double feedback() const noexcept { return fFeedback; }
    double mixW() const noexcept { return fMixW; }
    int stages() const noexcept { return fStages; }

    // tempo/playing/beatPos: the host transport at the start of the block (the JSFX's @block)
    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo = 120.0,
                 bool playing = false, double beatPos = 0.0)
    {
        if (fSync > 0)
        {
            fInc = 2.0 * M_PI * (tempo > 0.0 ? tempo : 120.0) / 60.0 / fSyncBeats / fSr;
            if (playing)
            {
                const double ph = beatPos / fSyncBeats;
                fPhase = 2.0 * M_PI * (ph - std::floor(ph));
            }
        }
        for (uint32_t i = 0; i < frames; ++i)
        {
            fPhase += fInc;
            if (fPhase >= 2.0 * M_PI)
                fPhase -= 2.0 * M_PI;
            if (fBarber)
            {
                // the LFO is a ramp and two voices half a sweep apart take turns (pb_pair)
                double p = fPhase / kTwoPi;
                if (fBarber == 2) p = 1.0 - p;
                if (p >= 1.0) p -= 1.0;
                double pr = p + fStDeg / 360.0;
                if (pr >= 1.0) pr -= 1.0;
                const double xl = inL[i], xr = inR[i];
                const double wl = pair(fBla, fBlb, xl, p);
                outL[i] = static_cast<T>(xl + wl * fMixW);
                const double wr = pair(fBra, fBrb, xr, fStOn ? pr : p);
                outR[i] = static_cast<T>(xr + wr * fMixW);
                continue;
            }
            const double d = fDMin + fDSc * (std::sin(fPhase) + 1.0);
            const double coef = (1.0 - d) / (1.0 + d);
            double coefR = coef;
            if (fStOn)
            {
                // the right channel's LFO runs Stereo Phase ahead
                const double dr = fDMin + fDSc * (std::sin(fPhase + fStOff) + 1.0);
                coefR = (1.0 - dr) / (1.0 + dr);
            }

            const double xl = inL[i];
            double wl = xl + fLastL * fFeedback;
            for (int k = 0; k < fStages && k < 8; ++k)
            {
                fLy[k] = coef * (fLy[k] + wl) - fLx[k];
                fLx[k] = wl;
                wl = fLy[k];
            }
            fLastL = wl;
            outL[i] = static_cast<T>(xl + wl * fMixW);

            const double xr = inR[i];
            double wr = xr + fLastR * fFeedback;
            for (int k = 0; k < fStages && k < 8; ++k)
            {
                fRy[k] = coefR * (fRy[k] + wr) - fRx[k];
                fRx[k] = wr;
                wr = fRy[k];
            }
            fLastR = wr;
            outR[i] = static_cast<T>(xr + wr * fMixW);
        }
    }

private:
    double pair(BarberVoice& a, BarberVoice& b, double x, double p) noexcept
    {
        double q = p + 0.5;
        if (q >= 1.0) q -= 1.0;
        const double da = fDMin + 2.0 * fDSc * p;
        const double db = fDMin + 2.0 * fDSc * q;
        const double sa = std::sin(M_PI * p);
        const double ra = a.run(x, (1.0 - da) / (1.0 + da), fStages, fFeedback);
        const double sb = std::sin(M_PI * q);
        const double rb = b.run(x, (1.0 - db) / (1.0 + db), fStages, fFeedback);
        return sa * ra + sb * rb;
    }

    BarberVoice fBla, fBlb, fBra, fBrb;
    double fStDeg = 0.0;
    int fBarber = 0;
    double fSr = 48000.0, fPhase = 0.0, fInc = 0.0, fMix = 0.3, fMixW = 0.3, fDMin = 0.0, fDSc = 0.0;
    double fFeedback = 0.3, fStOff = 0.0, fSyncBeats = 1.0;
    double fLy[8] = {}, fLx[8] = {}, fRy[8] = {}, fRx[8] = {};
    double fLastL = 0.0, fLastR = 0.0;
    int fStages = 4, fSync = 0;
    bool fStOn = false;
};

} // namespace nullphaser
