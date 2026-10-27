/*
 * NULL JSFX - MIDI Utility (VST3/CLAP): the mu_* functions of midi_utility.jsfx around the
 * shared note tracking (common/NullMidiNotes.hpp)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullMidiNotes.hpp"

#include <algorithm>
#include <cmath>

namespace nullmidiutil {

struct Params
{
    int transpose = 0, channel = 0;
    double curve = 0.0;
    int velMin = 1, velMax = 127, fixed = 0, keyLow = 0, keyHigh = 127, velLow = 1, velHigh = 127;
    bool blockCC = false, blockBend = false, blockTouch = false, blockProgram = false;
};

// mu_vel: the output velocity of an input velocity v (1..127)
static int outVelocity(int v, const Params& p)
{
    const double e = std::pow(2.0, -p.curve / 50.0);
    const double o = std::floor(p.velMin + (p.velMax - p.velMin) * std::pow((v - 1) / 126.0, e) + 0.5);
    return int(std::max(1.0, std::min(127.0, o)));
}

class MidiUtilityProcessor
{
public:
    // @init
    void reset()
    {
        fNotes.reset();
        fChanWas = -1;
        fLast = fLastOut = -1;
        fLastVel = fLastVelOut = 0;
    }

    // @slider: a new Channel ends the keys still held
    void setParams(const Params& p)
    {
        fP = p;
        if (p.channel != fChanWas)
        {
            if (fChanWas >= 0)
                fNotes.releaseAll(fNotes.position());
            fChanWas = p.channel;
        }
    }

    void begin() {}

    void input(uint32_t ofs, int a, int b, int c)
    {
        const int st = a & 0xF0;
        if (st < 0xF0)
        {
            a = st | (fP.channel > 0 ? fP.channel - 1 : a & 0x0F);
            const bool pass = st == 0xB0 ? (!fP.blockCC || b == 120 || b == 123)
                            : st == 0xE0 ? !fP.blockBend
                            : (st == 0xA0 || st == 0xD0) ? !fP.blockTouch
                            : st == 0xC0 ? !fP.blockProgram : true;
            if (!pass)
                return;
        }
        fNotes.input(fNotes.position() + ofs, a, b, c, [this](double t, int, int n, int v, int s) {
            const int m = n + fP.transpose;
            const bool ok = n >= fP.keyLow && n <= fP.keyHigh && v >= fP.velLow && v <= fP.velHigh && m >= 0 && m <= 127;
            const int o = fP.fixed > 0 ? fP.fixed : outVelocity(v, fP);
            fLast = n;
            fLastVel = v;
            fLastOut = ok ? m : -1;
            fLastVelOut = ok ? o : 0;
            fNotes.start(t, s, &m, ok ? 1 : 0, o, 0.0);
        });
    }

    template <class Send>
    void end(uint32_t frames, Send&& send) { fNotes.emit(frames, send); }

    int lastNote() const { return fLast; }
    int lastOut() const { return fLastOut; }
    int lastVelocity() const { return fLastVel; }
    int lastVelocityOut() const { return fLastVelOut; }
    const nulljsfx::MidiNotes& notes() const { return fNotes; }

private:
    nulljsfx::MidiNotes fNotes;
    Params fP;
    int fChanWas = -1, fLast = -1, fLastOut = -1, fLastVel = 0, fLastVelOut = 0;
};

} // namespace nullmidiutil
