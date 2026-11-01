/*
 * NULL JSFX - Euclid, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/euclid.jsfx, computed in double in the EEL2 order: Pulses
 * spread over Steps (step j plays when (j x Pulses) mod Steps < Pulses), turned by Rotate, one
 * step per Rate on the host's beat, or on a free-running beat at the host's tempo (Run Always).
 * Per sample, the MIDI that came in at that sample goes out first, then the note-off that is
 * due, then the note of a step; send(ofs, a, b, c) gets them in that order.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulleuclid {

struct Params {
    int steps = 16, pulses = 4, rotate = 0, rate = 2, note = 36, velocity = 100, run = 0, channel = 9;
    double accent = 0.0, gate = 50.0, swing = 0.0;
};

class EuclidProcessor {
public:
    static constexpr int kQueue = 256;

    void setSampleRate(double sr) noexcept { fSr = sr > 0.0 ? sr : 48000.0; }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fSteps = p.steps;
        fPulses = p.pulses < fSteps ? p.pulses : fSteps;
        fRot = int(double(p.rotate) - std::floor(double(p.rotate) / fSteps) * fSteps);
        fSb = p.rate == 0 ? 1.0 : p.rate == 1 ? 0.5 : p.rate == 2 ? 0.25 : p.rate == 3 ? 0.125 : p.rate == 4 ? 1.0 / 3.0 : 1.0 / 6.0;
        fAcc = p.accent / 100.0;
        fGate = p.gate / 100.0;
        fSwd = p.swing / 100.0 * 0.25;
    }

    // @init
    void reset() noexcept
    {
        fQn = fQi = 0;
        fLastS = -1000.0;
        fFb = 0.0;
        fOffLeft = 0;
        fOnNote = fOnChan = -1;
        fCurStep = -1;
        fWasRun = false;
    }

    // @block: the host's transport, then the messages that came in (offsets clamped to the block)
    void begin(uint32_t frames, bool playing, double tempo, double beat) noexcept
    {
        fQn = fQi = 0;
        fFrames = frames;
        fRun = playing || fP.run != 0;
        fBInc = tempo / (60.0 * fSr);
        fB0 = playing ? beat : fFb;
        fFb = fB0 + double(frames) * fBInc;
        fLen = int(std::fmax(1.0, std::floor(fGate * (1.0 - 2.0 * fSwd) * fSb * 60.0 / tempo * fSr)));
        fSi = 0;
    }
    void input(uint32_t ofs, int a, int b, int c) noexcept
    {
        if (fQn >= kQueue)
            return;
        Msg& m = fQ[fQn++];
        m.ofs = fFrames > 0 ? (ofs < fFrames ? ofs : fFrames - 1) : 0;
        m.a = a;
        m.b = b;
        m.c = c;
    }

    int currentStep() const noexcept { return fCurStep; }
    bool sounding() const noexcept { return fOffLeft > 0; }

    // one sample of @sample (the audio passes through untouched)
    template <class Send>
    void processSample(Send&& send) noexcept
    {
        const uint32_t si = fSi;
        while (fQi < fQn && fQ[fQi].ofs <= si)
        {
            const Msg& m = fQ[fQi];
            if ((m.a & 0xF0) == 0xB0 && (m.b == 120 || m.b == 123))
                noteOff(si, send);
            send(si, m.a, m.b, m.c);
            ++fQi;
        }
        if (fOffLeft > 0)
        {
            --fOffLeft;
            if (fOffLeft == 0)
                send(si, 0x80 | fOnChan, fOnNote, 0);
        }
        if (!fRun && fWasRun)
        {
            noteOff(si, send);
            fLastS = -1000.0;
            fCurStep = -1;
        }
        fWasRun = fRun;
        if (fRun)
        {
            const double bb = fB0 + double(si) * fBInc;
            if (bb >= 0.0)
            {
                const double u = bb / fSb;
                const double pp = std::floor(u / 2.0 + 0.000001);
                const double s = pp * 2.0 + (u / 2.0 - pp + 0.000001 >= 0.5 + fSwd ? 1.0 : 0.0);
                if (s != fLastS)
                {
                    const bool odd = (int64_t(s) & 1) != 0;
                    if (fLastS > -1000.0 || u - (odd ? 2.0 * pp + 1.0 + 2.0 * fSwd : 2.0 * pp) < 0.004)
                        step(si, s, send);
                    fLastS = s;
                }
            }
        }
        ++fSi;
    }

private:
    struct Msg { uint32_t ofs; int a, b, c; };

    template <class Send>
    void noteOff(uint32_t ofs, Send& send) noexcept
    {
        if (fOffLeft > 0)
        {
            send(ofs, 0x80 | fOnChan, fOnNote, 0);
            fOffLeft = 0;
        }
    }

    // eu_step: step s of the running position; a pulse sends its note
    template <class Send>
    void step(uint32_t ofs, double s, Send& send) noexcept
    {
        const int i = int(s - std::floor(s / fSteps) * fSteps);
        fCurStep = i;
        int j = i + fRot;
        if (j >= fSteps)
            j -= fSteps;
        if ((j * fPulses) % fSteps < fPulses)
        {
            const int v = j == 0 ? int(std::floor(fP.velocity + (127 - fP.velocity) * fAcc + 0.5)) : fP.velocity;
            noteOff(ofs, send);
            send(ofs, 0x90 | fP.channel, fP.note, v);
            fOnNote = fP.note;
            fOnChan = fP.channel;
            fOffLeft = fLen;
        }
    }

    double fSr = 48000.0;
    Params fP;
    int fSteps = 16, fPulses = 4, fRot = 0;
    double fSb = 0.25, fAcc = 0.0, fGate = 0.5, fSwd = 0.0;
    double fLastS = -1000.0, fFb = 0.0, fB0 = 0.0, fBInc = 0.0;
    int fOffLeft = 0, fOnNote = -1, fOnChan = -1, fCurStep = -1, fLen = 1;
    bool fRun = false, fWasRun = false;
    Msg fQ[kQueue] = {};
    int fQn = 0, fQi = 0;
    uint32_t fSi = 0, fFrames = 0;
};

} // namespace nulleuclid
