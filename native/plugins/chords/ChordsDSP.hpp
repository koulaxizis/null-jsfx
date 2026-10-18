/*
 * NULL JSFX - Chords (VST3/CLAP): the chord a key plays, as cd_build in chords.jsfx, and the
 * MIDI processing around the shared note tracking (common/NullMidiNotes.hpp)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullMidiNotes.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nullchords {

// scales: the semitones of the seven degrees (the Scale slider's order)
static const int kScales[9][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },  // Major
    { 0, 2, 3, 5, 7, 8, 10 },  // Minor
    { 0, 2, 3, 5, 7, 8, 11 },  // Harmonic Minor
    { 0, 2, 3, 5, 7, 9, 11 },  // Melodic Minor
    { 0, 2, 3, 5, 7, 9, 10 },  // Dorian
    { 0, 1, 3, 5, 7, 8, 10 },  // Phrygian
    { 0, 2, 4, 6, 7, 9, 11 },  // Lydian
    { 0, 2, 4, 5, 7, 9, 10 },  // Mixolydian
    { 0, 1, 3, 5, 6, 8, 10 },  // Locrian
};

// progressions: steps of (degree, scale: 0 major, 1 minor, 2 harmonic minor for a major V)
struct Progression { int steps; int deg[8]; int scale[8]; };
static const Progression kProgressions[9] = {
    { 4, { 0, 4, 5, 3 }, { 0, 0, 0, 0 } },                         // I-V-vi-IV
    { 4, { 0, 5, 3, 4 }, { 0, 0, 0, 0 } },                         // I-vi-IV-V
    { 4, { 5, 3, 0, 4 }, { 0, 0, 0, 0 } },                         // vi-IV-I-V
    { 4, { 0, 3, 5, 4 }, { 0, 0, 0, 0 } },                         // I-IV-vi-V
    { 3, { 1, 4, 0 }, { 0, 0, 0 } },                               // ii-V-I
    { 8, { 0, 4, 5, 2, 3, 0, 3, 4 }, { 0, 0, 0, 0, 0, 0, 0, 0 } }, // Canon
    { 4, { 0, 5, 2, 6 }, { 1, 1, 1, 1 } },                         // i-VI-III-VII
    { 4, { 0, 6, 5, 4 }, { 1, 1, 1, 2 } },                         // i-VII-VI-V
    { 4, { 0, 3, 4, 0 }, { 1, 1, 2, 1 } },                         // i-iv-V-i
};

struct Params {
    int mode = 0, key = 0, scale = 0, type = 0, inv = 0, voicing = 0, octave = 0, dir = 0, prog = 0;
    double strumMs = 0.0, curve = 0.0;
};

// the chord of one key (cd_build's o[]): notes ascending, how many, root, degree, scale, and
// the root-position intervals of the third, fifth, seventh and ninth
struct Chord {
    int notes[8] = {};
    int count = 0;
    int root = 0, degree = 0, scale = 0;
    int i3 = 0, i5 = 0, i7 = 0, i9 = 0;
};

static int semi(int s, int j) { return kScales[s][j % 7] + 12 * (j / 7); }

static void sortNotes(int* o, int n)
{
    for (int i = 1; i < n; ++i)
    {
        const int t = o[i];
        int j = i - 1;
        while (j >= 0 && o[j] > t)
        {
            o[j + 1] = o[j];
            --j;
        }
        o[j + 1] = t;
    }
}

static Chord build(int n, const Params& p)
{
    Chord o;
    int d = -1, s = 0, root = 0;
    if (p.mode == 0)
    {
        s = p.scale;
        const int pc = (n - p.key + 120) % 12;
        d = 6;
        while (d > 0 && kScales[s][d] > pc)
            --d;
        root = n - pc + kScales[s][d];
    }
    else
    {
        const int pc = n % 12;
        const int w = pc == 0 ? 0 : pc == 2 ? 1 : pc == 4 ? 2 : pc == 5 ? 3 : pc == 7 ? 4 : pc == 9 ? 5 : pc == 11 ? 6 : -1;
        if (w >= 0)
        {
            const Progression& pr = kProgressions[p.prog];
            const int k = w % pr.steps;
            d = pr.deg[k];
            s = pr.scale[k];
            root = n - pc + p.key + kScales[s][d];
        }
    }
    if (d < 0)
        return o;
    o.root = root; o.degree = d; o.scale = s;
    const int sd = kScales[s][d];
    int* t = o.notes;
    int c;
    if (p.type == 5)
    {
        t[0] = root; t[1] = root + semi(s, d + 4) - sd; t[2] = root + 12;
        c = 3;
        o.i3 = 0; o.i5 = t[1] - root; o.i7 = 0; o.i9 = 0;
    }
    else
    {
        c = p.type == 1 ? 4 : p.type == 2 ? 5 : 3;
        for (int k = 0; k < c; ++k)
        {
            const int iv = k == 1 && p.type == 3 ? 1 : k == 1 && p.type == 4 ? 3 : 2 * k;
            t[k] = root + semi(s, d + iv) - sd;
        }
        o.i3 = t[1] - root; o.i5 = t[2] - root;
        o.i7 = c > 3 ? t[3] - root : 0;
        o.i9 = c > 4 ? t[4] - root - 12 : 0;
    }
    for (int k = 0; k < p.inv % c; ++k)
    {
        t[0] += 12;
        sortNotes(t, c);
    }
    if (p.voicing == 1 && c >= 3)
    {
        t[c - 2] -= 12;
        sortNotes(t, c);
    }
    if (p.voicing >= 2)
    {
        for (int i = 1; i < c; i += 2)
            t[i] += 12;
        sortNotes(t, c);
        if (p.voicing == 3)
            t[0] -= 12;
    }
    int j = 0;
    for (int i = 0; i < c; ++i)
    {
        const int m = t[i] + 12 * p.octave;
        if (m >= 0 && m <= 127 && (j == 0 || m != t[j - 1]))
            t[j++] = m;
    }
    o.count = j;
    return o;
}

// ---- names, for the display (as chords_gfx.jsfx) ----------------------------------------------

static const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// the chord symbol, e.g. "Am7/G" (the bass note after a slash when it is not the root)
static void chordName(const Chord& c, const Params& p, char* buf, size_t n)
{
    if (c.count == 0)
    {
        std::snprintf(buf, n, "-");
        return;
    }
    char suf[24];
    const bool flat5 = c.i5 == 6;
    if (p.type == 5)
        std::snprintf(suf, sizeof(suf), "%s", flat5 ? "(b5)" : "5");
    else if (p.type == 3 || p.type == 4)
        std::snprintf(suf, sizeof(suf), "%s%s", p.type == 3 ? "sus2" : "sus4", flat5 ? "(b5)" : "");
    else
    {
        // quality: before and after the 7 (or 9)
        const bool maj = c.i3 == 4, aug = maj && c.i5 == 8, dim = !maj && flat5;
        const char* pre = aug ? (c.i7 == 11 ? "+maj" : "+") : dim ? (c.i7 == 9 ? "dim" : "m") : maj ? (c.i7 == 11 ? "maj" : "")
                        : (c.i7 == 11 ? "mMaj" : "m");
        const char* post = dim && c.i7 != 9 ? "b5" : "";
        if (p.type == 0)
            std::snprintf(suf, sizeof(suf), "%s", aug ? "aug" : dim ? "dim" : maj ? "" : "m");
        else
        {
            const bool nine = p.type == 2 && c.i9 == 2;
            std::snprintf(suf, sizeof(suf), "%s%s%s%s", pre, nine ? "9" : "7", post,
                          p.type == 2 && !nine ? (c.i9 == 1 ? "b9" : "#9") : "");
        }
    }
    const int bass = c.notes[0] % 12, rootPc = (c.root % 12 + 12) % 12;
    if (bass != rootPc)
        std::snprintf(buf, n, "%s%s/%s", kNoteNames[rootPc], suf, kNoteNames[bass]);
    else
        std::snprintf(buf, n, "%s%s", kNoteNames[rootPc], suf);
}

// the Roman numeral of the chord's degree: upper case for major, lower for minor, o for
// diminished, + for augmented
static void numeral(const Chord& c, const Params& p, char* buf, size_t n)
{
    static const char* const up[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* const lo[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    if (c.count == 0)
    {
        std::snprintf(buf, n, "-");
        return;
    }
    const bool minor = p.type != 5 && p.type != 3 && p.type != 4 && c.i3 == 3;
    const bool dim = minor && c.i5 == 6, aug = p.type <= 2 && c.i3 == 4 && c.i5 == 8;
    std::snprintf(buf, n, "%s%s", minor ? lo[c.degree] : up[c.degree], dim ? "o" : aug ? "+" : "");
}

// ---- the MIDI processing ----------------------------------------------------------------------

class ChordsProcessor
{
public:
    void setSampleRate(double sr) { fSr = sr; }

    // @init
    void reset()
    {
        fNotes.reset();
        fAlt = 0;
        fLast = -1;
    }

    void setParams(const Params& p)
    {
        fP = p;
        fVexp = std::pow(2.0, -p.curve / 50.0);
    }

    // @block: the messages of one block (offset, status, d1, d2), then what goes out
    void begin() { fStrum = std::floor(fP.strumMs * fSr / 1000.0 + 0.5); }

    void input(uint32_t ofs, int a, int b, int c)
    {
        fNotes.input(fNotes.position() + ofs, a, b, c, [this](double t, int, int n, int v, int s) {
            const Chord ch = build(n, fP);
            int notes[8];
            const int cnt = ch.count;
            if (cnt > 0)
            {
                fLast = n;
                const int vo = vel(v);
                const bool down = fP.dir == 1 || (fP.dir == 2 && fAlt);
                if (fP.dir == 2)
                    fAlt = 1 - fAlt;
                for (int i = 0; i < cnt; ++i)
                    notes[i] = ch.notes[down ? cnt - 1 - i : i];
                fNotes.start(t, s, notes, cnt, vo, fStrum);
            }
            else
                fNotes.start(t, s, notes, 0, 0, fStrum);
        });
    }

    template <class Send>
    void end(uint32_t frames, Send&& send) { fNotes.emit(frames, send); }

    int vel(int v) const
    {
        const double x = std::floor(127.0 * std::pow(v / 127.0, fVexp) + 0.5);
        return int(std::fmax(1.0, std::fmin(127.0, x)));
    }

    int lastNote() const { return fLast; }
    const nulljsfx::MidiNotes& notes() const { return fNotes; }

private:
    nulljsfx::MidiNotes fNotes;
    Params fP;
    double fSr = 44100.0, fVexp = 1.0, fStrum = 0.0;
    int fAlt = 0, fLast = -1;
};

} // namespace nullchords
