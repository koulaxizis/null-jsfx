/*
 * NULL JSFX - Kick, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/kick.jsfx, computed in double in the EEL2 order: a note-on
 * starts a kick in the other of two voices (the old one fades out over 3 ms); a kick is a sine
 * whose pitch falls from Sweep semitones above Pitch, falling 60 dB over Decay, plus a burst of
 * low-passed Park-Miller noise (the click); a soft clip for Drive, then Volume.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullkick {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kMaxEvents = 512;

struct MidiEv {
    uint32_t frame;
    double m1, m2, m3;
};

struct Params {
    double pitch = 50.0;
    bool follow = false;
    double sweep = 24.0, sweepTime = 40.0, decay = 450.0, click = 30.0, clickTone = 3000.0;
    double drive = 0.0, velocity = 50.0, volume = 0.0;
};

// k_tanh of the JSFX: (e^2x - 1) / (e^2x + 1), clamped
inline double eelTanh(double x) noexcept
{
    if (x > 20.0)
        return 1.0;
    if (x < -20.0)
        return -1.0;
    const double e = std::exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}

class KickProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        derive();
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        derive();
    }

    // @init
    void reset() noexcept
    {
        for (Voice& v : fV)
            v = Voice();
        fLast = 1;
        fSeed = 12345.0;
    }

    void beginBlock(const MidiEv* ev, uint32_t count) noexcept
    {
        fMqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + fMqN, fMq);
        fMqI = 0;
        fSi = 0;
    }

    void process(float* outL, float* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            while (fMqI < fMqN && fMq[fMqI].frame <= fSi)
            {
                midiEvent(fMq[fMqI].m1, fMq[fMqI].m2, fMq[fMqI].m3);
                ++fMqI;
            }
            ++fSi;

            fSeed *= 16807.0;
            fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
            const double w = fSeed / 1073741824.0 - 1.0;
            double x = 0.0;
            for (Voice& v : fV)
            {
                if (v.on == 0.0)
                    continue;
                const double f = v.f0 * std::pow(2.0, fSw * v.pe);
                double y = std::sin(2.0 * kPi * v.ph) * v.ae;
                v.ph += f / fSr;
                v.ph -= std::floor(v.ph);
                v.clp += (w - v.clp) * fCk;
                y += v.clp * v.ce * fClick;
                x += y * v.velg * v.fade;
                v.pe *= fPek;
                v.ae *= fAek;
                v.ce *= fCek;
                v.fade -= v.fadeRate;
                if (v.fade <= 0.0 || (v.ae < 0.00001 && v.ce < 0.00001))
                    v.on = 0.0;
            }
            if (fDr > 0.0)
                x = eelTanh(fDg * x) / fDn;
            x *= fOg;
            outL[i] = float(x);
            outR[i] = float(x);
        }
    }

private:
    struct Voice {
        double on = 0.0, ph = 0.0, pe = 0.0, ae = 0.0, ce = 0.0, clp = 0.0, velg = 0.0, f0 = 0.0, fade = 0.0,
               fadeRate = 0.0;
    };

    // midi_ev of the JSFX
    void midiEvent(double m1, double m2, double m3) noexcept
    {
        const int st = int(m1) & 0xF0;
        if (st == 0x90 && m3 > 0.0)
        {
            Voice& o = fV[fLast];
            if (o.on != 0.0)
                o.fadeRate = 1.0 / (0.003 * fSr);
            fLast = 1 - fLast;
            Voice& v = fV[fLast];
            v.on = 1.0;
            v.ph = 0.0;
            v.pe = 1.0;
            v.ae = 1.0;
            v.ce = 1.0;
            v.clp = 0.0;
            v.velg = 1.0 - fVs + fVs * m3 / 127.0;
            v.f0 = fP.follow ? fP.pitch * std::pow(2.0, (m2 - 36.0) / 12.0) : fP.pitch;
            v.fade = 1.0;
            v.fadeRate = 0.0;
        }
    }

    // @slider
    void derive() noexcept
    {
        fSw = fP.sweep / 12.0;
        fPek = std::exp(-1.0 / (fP.sweepTime / 1000.0 * fSr));
        fAek = std::exp(-6.907755278982137 / (fP.decay / 1000.0 * fSr));
        fCek = std::exp(-6.907755278982137 / (0.02 * fSr));
        fClick = fP.click / 100.0 * 2.0;
        fCk = 1.0 - std::exp(-2.0 * kPi * fP.clickTone / fSr);
        fDr = fP.drive / 100.0;
        fDg = 1.0 + 9.0 * fDr;
        fDn = eelTanh(fDg);
        fVs = fP.velocity / 100.0;
        fOg = std::pow(10.0, fP.volume / 20.0);
    }

    Params fP;
    double fSr = 48000.0;
    double fSw = 2.0, fPek = 0.0, fAek = 0.0, fCek = 0.0, fClick = 0.6, fCk = 0.0, fDr = 0.0, fDg = 1.0, fDn = 1.0,
           fVs = 0.5, fOg = 1.0;
    Voice fV[2];
    int fLast = 1;
    double fSeed = 12345.0;
    MidiEv fMq[kMaxEvents];
    uint32_t fMqN = 0, fMqI = 0, fSi = 0;
};

} // namespace nullkick
