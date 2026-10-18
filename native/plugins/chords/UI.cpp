/*
 * NULL JSFX - Chords (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullKeyboardUI.hpp"
#include "ChordsDSP.hpp"
#include "ChordsSliders.hpp"

START_NAMESPACE_DISTRHO

// as chords_gfx.jsfx: mode, key, scale, chord, inversion, voicing / octave, strum, direction,
// velocity, progression
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "KEY", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "SCALE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "CHORD", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "INVERSION", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "VOICING", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "OCTAVE", 1, -1, true },
    { 7, NullKnobsUI::kKnob, "STRUM", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "DIRECTION", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "VELOCITY", 1, -1, true },
    { 10, NullKnobsUI::kSelect, "PROGRESSION", 1, -1, false },
};

static const char* const kScaleNames[] = { "MAJOR", "MINOR", "HARMONIC MINOR", "MELODIC MINOR", "DORIAN", "PHRYGIAN",
                                           "LYDIAN", "MIXOLYDIAN", "LOCRIAN" };

class UIChords : public NullKeyboardUI
{
public:
    UIChords()
        : NullKeyboardUI({ "CHORDS", "MIDI", 480, 140, nullptr, 2, nullchords::kUiSliders, nullchords::kNumSliders,
                           kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    nullchords::Params params() const
    {
        nullchords::Params p;
        p.mode = iv(0); p.key = iv(1); p.scale = iv(2); p.type = iv(3); p.inv = iv(4); p.voicing = iv(5);
        p.octave = iv(6); p.strumMs = iv(7); p.dir = iv(8); p.curve = iv(9); p.prog = iv(10);
        return p;
    }

    // as in chords_gfx.jsfx: the chord of the last key (before any, the tonic's from C4 as a
    // preview, dimmed), its symbol and degree, the scale's degrees (Chord) or the progression's
    // steps (Progression) with the chord's lit, and a keyboard C2..C7: scale notes tinted, the
    // chord dotted, the notes sounding silver, the held keys marked on top
    void drawDisplay() override
    {
        using namespace nullchords;
        const Params p = params();
        int last = -1;
        if (nulljsfx::Taps* t = taps())
            last = int(std::lround(t->extra.load(std::memory_order_relaxed))) - 1;
        readNotes();
        const int n = last >= 0 ? last : p.mode ? 60 : 60 + p.key;
        const Chord c = build(n, p);
        std::memset(fPreview, 0, sizeof(fPreview));
        for (int i = 0; i < c.count; ++i)
            fPreview[c.notes[i]] = true;
        const int gs = p.mode ? kProgressions[p.prog].scale[0] : p.scale;
        std::memset(fInScale, 0, sizeof(fInScale));
        for (int i = 0; i < 7; ++i)
            fInScale[kScales[gs][i]] = true;

        char name[32], num[16], line[96];
        chordName(c, p, name, sizeof(name));
        label(30.0f, 80.0f, ALIGN_LEFT, 22.0f, 0.5f, last >= 0 ? theme::kText : theme::kMuted, name, true);
        numeral(c, p, num, sizeof(num));
        std::snprintf(line, sizeof(line), "%s   %s %s%s", c.count ? num : "-", kNoteNames[p.key], kScaleNames[gs],
                      last >= 0 ? "" : "   PLAY A KEY");
        label(30.0f, 103.0f, ALIGN_LEFT, 8.0f, 1.2f, theme::kMuted, line, true);

        // the chips: the 7 degrees, or the steps
        const Progression& pr = kProgressions[p.prog];
        const int count = p.mode ? pr.steps : 7;
        int hi = -1;
        if (p.mode)
        {
            const int pc = n % 12;
            const int w = pc == 0 ? 0 : pc == 2 ? 1 : pc == 4 ? 2 : pc == 5 ? 3 : pc == 7 ? 4 : pc == 9 ? 5 : pc == 11 ? 6 : -1;
            if (w >= 0)
                hi = w % count;
        }
        else if (c.count)
            hi = c.degree;
        float x = 456.0f - float(count) * 31.0f + 3.0f;
        for (int i = 0; i < count; ++i, x += 31.0f)
        {
            const int d = p.mode ? pr.deg[i] : i, s = p.mode ? pr.scale[i] : p.scale;
            const int sd = kScales[s][d], t3 = semi(s, d + 2) - sd, t5 = semi(s, d + 4) - sd;
            Chord tri;
            tri.count = 1; tri.degree = d; tri.i3 = t3; tri.i5 = t5;
            Params tp;
            tp.type = 0;
            numeral(tri, tp, num, sizeof(num));
            chip(x, 70.0f, num, i == hi);
        }
        static const char* const types[] = { "TRIAD", "7TH", "9TH", "SUS2", "SUS4", "POWER" };
        static const char* const invs[] = { "ROOT", "1ST INV", "2ND INV", "3RD INV" };
        static const char* const voic[] = { "CLOSE", "DROP 2", "OPEN", "WIDE" };
        static const char* const dirs[] = { "UP", "DOWN", "ALT" };
        if (p.strumMs > 0)
            std::snprintf(line, sizeof(line), "%s  %s  %s  STRUM %d MS %s", types[p.type], invs[p.inv], voic[p.voicing],
                          int(p.strumMs), dirs[p.dir]);
        else
            std::snprintf(line, sizeof(line), "%s  %s  %s", types[p.type], invs[p.inv], voic[p.voicing]);
        label(456.0f, 101.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, line, true);

        drawKeyboard(24.0f, 116.0f, 432.0f, 68.0f, 36, 96, p.key);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int i = int(std::lround(v));
        if (param == 6)
            std::snprintf(buf, n, i > 0 ? "+%d" : "%d", i);
        else if (param == 9)
            std::snprintf(buf, n, i > 0 ? "+%d%%" : "%d%%", i);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIChords();
}

END_NAMESPACE_DISTRHO
