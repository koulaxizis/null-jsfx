/*
 * NULL JSFX - Scale (VST3/CLAP): the note a key plays, as sc_map in scale.jsfx, and the MIDI
 * processing around the shared note tracking (common/NullMidiNotes.hpp)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullMidiNotes.hpp"

#include <cmath>

namespace nullscale {

// scales: size, then the semitones (the Scale slider's order)
struct ScaleDef { int size; int semi[7]; };
static const ScaleDef kScales[12] = {
    { 7, { 0, 2, 4, 5, 7, 9, 11 } },  // Major
    { 7, { 0, 2, 3, 5, 7, 8, 10 } },  // Minor
    { 7, { 0, 2, 3, 5, 7, 8, 11 } },  // Harmonic Minor
    { 7, { 0, 2, 3, 5, 7, 9, 11 } },  // Melodic Minor
    { 7, { 0, 2, 3, 5, 7, 9, 10 } },  // Dorian
    { 7, { 0, 1, 3, 5, 7, 8, 10 } },  // Phrygian
    { 7, { 0, 2, 4, 6, 7, 9, 11 } },  // Lydian
    { 7, { 0, 2, 4, 5, 7, 9, 10 } },  // Mixolydian
    { 7, { 0, 1, 3, 5, 6, 8, 10 } },  // Locrian
    { 5, { 0, 2, 4, 7, 9 } },         // Major Pentatonic
    { 5, { 0, 3, 5, 7, 10 } },        // Minor Pentatonic
    { 6, { 0, 3, 5, 6, 7, 10 } },     // Blues
};

static bool inScale(int s, int pc)
{
    for (int i = 0; i < kScales[s].size; ++i)
        if (kScales[s].semi[i] == pc)
            return true;
    return false;
}

struct Params { int key = 0, scale = 0, mode = 0, transpose = 0, octave = 0; };

static int whiteIndex(int pc)
{
    return pc == 0 ? 0 : pc == 2 ? 1 : pc == 4 ? 2 : pc == 5 ? 3 : pc == 7 ? 4 : pc == 9 ? 5 : pc == 11 ? 6 : -1;
}

// the note input note n plays, or -1 for none
static int mapNote(int n, const Params& p)
{
    const int t = n + p.transpose;
    const ScaleDef& sc = kScales[p.scale];
    int r = 0;
    bool ok = false;
    if (p.mode == 3)
    {
        const int oc = n / 12;
        const int w = whiteIndex(n - 12 * oc);
        if (w >= 0)
        {
            r = 12 * oc + p.key + sc.semi[w % sc.size] + 12 * (w / sc.size) + p.transpose;
            ok = true;
        }
    }
    else
    {
        const int pc = (t - p.key + 120) % 12;
        if (inScale(p.scale, pc))
        {
            r = t;
            ok = true;
        }
        else if (p.mode != 4)
        {
            int dd = 1, du = 1;
            while (!inScale(p.scale, (pc - dd + 12) % 12))
                ++dd;
            while (!inScale(p.scale, (pc + du) % 12))
                ++du;
            r = p.mode == 1 ? t + du : p.mode == 2 ? t - dd : dd <= du ? t - dd : t + du;
            ok = true;
        }
    }
    if (ok)
    {
        r += 12 * p.octave;
        if (r < 0 || r > 127)
            ok = false;
    }
    return ok ? r : -1;
}

class ScaleProcessor
{
public:
    // @init
    void reset()
    {
        fNotes.reset();
        fLast = -1;
        fLastOut = -1;
    }

    void setParams(const Params& p) { fP = p; }

    void begin() {}

    void input(uint32_t ofs, int a, int b, int c)
    {
        fNotes.input(fNotes.position() + ofs, a, b, c, [this](double t, int, int n, int v, int s) {
            const int m = mapNote(n, fP);
            fLast = n;
            fLastOut = m;
            fNotes.start(t, s, &m, m >= 0 ? 1 : 0, v, 0.0);
        });
    }

    template <class Send>
    void end(uint32_t frames, Send&& send) { fNotes.emit(frames, send); }

    int lastNote() const { return fLast; }
    int lastOut() const { return fLastOut; }
    const nulljsfx::MidiNotes& notes() const { return fNotes; }

private:
    nulljsfx::MidiNotes fNotes;
    Params fP;
    int fLast = -1, fLastOut = -1;
};

} // namespace nullscale
