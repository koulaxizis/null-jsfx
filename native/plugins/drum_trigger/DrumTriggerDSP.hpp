/*
 * NULL JSFX - Drum Trigger, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/drum_trigger.jsfx, computed in double in the EEL2 order: the
 * mono sum through a 12 dB/oct high-pass and low-pass (RBJ biquads), a peak follower with a 5 ms
 * release, a trigger with Retrigger hold and 3 dB hysteresis, a 2 ms velocity window, and a
 * note that ends after Note Length. Per sample, the MIDI that came in at that sample goes out
 * first, then the notes made there; send(ofs, a, b, c) gets them in that order.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulldrumtrigger {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    int note = 36, channel = 9;
    double threshold = -24.0, retrigger = 60.0, dynamics = 100.0, hpf = 20.0, lpf = 20000.0, length = 50.0;
};

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// dt_bq: kind 0 = high-pass, 1 = low-pass, Q = 1/sqrt(2)
inline Biquad biquad(int kind, double f, double sr) noexcept
{
    Biquad b;
    const double w0 = 2.0 * kPi * std::fmin(f, 0.45 * sr) / sr;
    const double cw = std::cos(w0);
    const double al = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double a0 = 1.0 + al;
    b.b0 = (kind ? (1.0 - cw) / 2.0 : (1.0 + cw) / 2.0) / a0;
    b.b1 = (kind ? 1.0 - cw : -(1.0 + cw)) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - al) / a0;
    return b;
}

class DrumTriggerProcessor {
public:
    static constexpr int kQueue = 256;

    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        coeffs();
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        coeffs();
    }

    // @init
    void reset() noexcept
    {
        fH1 = fH2 = fL1 = fL2 = 0.0;
        fEnv = 0.0;
        fArmed = true;
        fWin = 0;
        fPk = 0.0;
        fSince = 1000000000.0;
        fOffLeft = 0;
        fOnNote = fOnChan = -1;
        fLastVel = 0;
        fHits = 0;
        fQn = fQi = 0;
    }

    // @block: the messages that came in (offsets clamped to the block, at most kQueue)
    void begin() noexcept { fQn = fQi = 0; fSi = 0; }
    void input(uint32_t ofs, uint32_t frames, int a, int b, int c) noexcept
    {
        if (fQn >= kQueue)
            return;
        Msg& m = fQ[fQn++];
        m.ofs = frames > 0 ? (ofs < frames ? ofs : frames - 1) : 0;
        m.a = a;
        m.b = b;
        m.c = c;
    }

    double envelope() const noexcept { return fEnv; }
    int lastVelocity() const noexcept { return fLastVel; }
    int hits() const noexcept { return fHits; }
    bool sounding() const noexcept { return fOffLeft > 0; }

    // one sample of @sample (the audio itself passes through untouched)
    template <class Send>
    void processSample(double l, double r, Send&& send) noexcept
    {
        const uint32_t si = fSi;
        while (fQi < fQn && fQ[fQi].ofs <= si)
        {
            const Msg& m = fQ[fQi];
            if ((m.a & 0xF0) == 0xB0 && (m.b == 120 || m.b == 123) && fOffLeft > 0)
            {
                send(si, 0x80 | fOnChan, fOnNote, 0);
                fOffLeft = 0;
            }
            send(si, m.a, m.b, m.c);
            ++fQi;
        }
        if (fOffLeft > 0)
        {
            --fOffLeft;
            if (fOffLeft == 0)
                send(si, 0x80 | fOnChan, fOnNote, 0);
        }
        const double x = (l + r) * 0.5;
        const double y = fHp.b0 * x + fH1;
        fH1 = fHp.b1 * x - fHp.a1 * y + fH2;
        fH2 = fHp.b2 * x - fHp.a2 * y;
        const double z = fLp.b0 * y + fL1;
        fL1 = fLp.b1 * y - fLp.a1 * z + fL2;
        fL2 = fLp.b2 * y - fLp.a2 * z;
        const double d = std::fabs(z);
        fEnv = d > fEnv ? d : fEnv * fErel;
        if (fEnv < 0.0000000001)
            fEnv = 0.0;
        fSince += 1.0;
        if (!fArmed && fSince >= fRetrig && fEnv < fRearm)
            fArmed = true;
        if (fArmed && fEnv >= fThr)
        {
            fArmed = false;
            fSince = 0.0;
            fWin = fWlen;
            fPk = 0.0;
        }
        if (fWin > 0)
        {
            fPk = std::fmax(fPk, fEnv);
            --fWin;
            if (fWin == 0)
            {
                const double fr = fP.threshold < 0.0
                    ? std::fmin(1.0, std::fmax(0.0, (20.0 * std::log10(std::fmax(fPk, 0.000001)) - fP.threshold) / -fP.threshold))
                    : 1.0;
                double vel = std::floor(100.0 + (1.0 + 126.0 * fr - 100.0) * fDyn + 0.5);
                vel = std::fmax(1.0, std::fmin(127.0, vel));
                if (fOffLeft > 0)
                    send(si, 0x80 | fOnChan, fOnNote, 0);
                send(si, 0x90 | fP.channel, fP.note, int(vel));
                fOnNote = fP.note;
                fOnChan = fP.channel;
                fOffLeft = fLen;
                fLastVel = int(vel);
                ++fHits;
            }
        }
        ++fSi;
    }

private:
    struct Msg { uint32_t ofs; int a, b, c; };

    // the @slider code
    void coeffs() noexcept
    {
        fThr = std::pow(10.0, fP.threshold / 20.0);
        fRearm = std::pow(10.0, (fP.threshold - 3.0) / 20.0);
        fRetrig = std::floor(fP.retrigger * 0.001 * fSr);
        fDyn = fP.dynamics / 100.0;
        fHp = biquad(0, fP.hpf, fSr);
        fLp = biquad(1, fP.lpf, fSr);
        fLen = int(std::fmax(1.0, std::floor(fP.length * 0.001 * fSr)));
        fWlen = int(std::fmax(1.0, std::floor(0.002 * fSr)));
        fErel = std::exp(-1.0 / (0.005 * fSr));
    }

    double fSr = 48000.0;
    Params fP;
    Biquad fHp, fLp;
    double fThr = 0.063, fRearm = 0.045, fRetrig = 2880.0, fDyn = 1.0, fErel = 0.99;
    int fLen = 2400, fWlen = 96;
    double fH1 = 0.0, fH2 = 0.0, fL1 = 0.0, fL2 = 0.0, fEnv = 0.0, fPk = 0.0, fSince = 1000000000.0;
    bool fArmed = true;
    int fWin = 0, fOffLeft = 0, fOnNote = -1, fOnChan = -1, fLastVel = 0, fHits = 0;
    Msg fQ[kQueue] = {};
    int fQn = 0, fQi = 0;
    uint32_t fSi = 0;
};

} // namespace nulldrumtrigger
