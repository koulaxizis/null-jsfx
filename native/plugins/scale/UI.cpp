/*
 * NULL JSFX - Scale (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullKeyboardUI.hpp"
#include "ScaleDSP.hpp"
#include "ScaleSliders.hpp"

START_NAMESPACE_DISTRHO

// as scale_gfx.jsfx: key, scale, mode, transpose, octave
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "KEY", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SCALE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "TRANSPOSE", 0, -1, true },
    { 4, NullKnobsUI::kKnob, "OCTAVE", 0, -1, true },
};

static const char* const kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleNames[12] = { "MAJOR", "MINOR", "HARMONIC MINOR", "MELODIC MINOR", "DORIAN", "PHRYGIAN",
                                             "LYDIAN", "MIXOLYDIAN", "LOCRIAN", "MAJOR PENTATONIC", "MINOR PENTATONIC", "BLUES" };
static const char* const kDigits[8] = { nullptr, "1", "2", "3", "4", "5", "6", "7" };

class UIScale : public NullKeyboardUI
{
public:
    UIScale()
        : NullKeyboardUI({ "SCALE", "MIDI", 480, 140, nullptr, 1, nullscale::kUiSliders, nullscale::kNumSliders,
                           kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    static void noteName(char* buf, size_t n, int note) { std::snprintf(buf, n, "%s%d", kNames[note % 12], note / 12 - 1); }

    // as in scale_gfx.jsfx: the key and scale with its notes, the last key and the note it played,
    // what the mode does, and a keyboard C2..C7: scale notes tinted, the tonic underlined, the
    // notes sounding silver, the held keys marked on top; in White Keys mode each white key shows
    // the degree it plays
    void drawDisplay() override
    {
        using namespace nullscale;
        Params p;
        p.key = iv(0); p.scale = iv(1); p.mode = iv(2); p.transpose = iv(3); p.octave = iv(4);
        const ScaleDef& sc = kScales[p.scale];
        int last = -1, lastOut = -1;
        if (nulljsfx::Taps* t = taps())
        {
            last = int(std::lround(t->extra.load(std::memory_order_relaxed))) - 1;
            lastOut = int(std::lround(t->inLevel.load(std::memory_order_relaxed))) - 1;
        }
        readNotes();
        std::memset(fInScale, 0, sizeof(fInScale));
        for (int i = 0; i < sc.size; ++i)
            fInScale[sc.semi[i]] = true;
        for (int n = 0; n < 128; ++n)
        {
            fKeyLabel[n] = nullptr;
            if (p.mode != 3 || n < 36 || n > 96)
                continue;
            const int o = mapNote(n, p);
            if (o < 0)
                continue;
            const int pc = (o - p.key + 120) % 12;
            for (int i = 0; i < sc.size; ++i)
                if (sc.semi[i] == pc)
                    fKeyLabel[n] = kDigits[i + 1];
        }

        char line[96], a[16], b[16];
        std::snprintf(line, sizeof(line), "%s %s", kNames[p.key], kScaleNames[p.scale]);
        label(30.0f, 80.0f, ALIGN_LEFT, 20.0f, 0.5f, theme::kText, line, true);
        line[0] = 0;
        for (int i = 0; i < sc.size; ++i)
        {
            const size_t l = std::strlen(line);
            std::snprintf(line + l, sizeof(line) - l, i ? "  %s" : "%s", kNames[(p.key + sc.semi[i]) % 12]);
        }
        label(30.0f, 103.0f, ALIGN_LEFT, 8.0f, 1.2f, theme::kMuted, line, true);

        label(456.0f, 68.0f, ALIGN_RIGHT, 7.5f, 1.5f, theme::kMuted, last >= 0 ? "LAST NOTE" : "PLAY A KEY", true);
        if (last >= 0)
        {
            noteName(a, sizeof(a), last);
            if (lastOut >= 0)
                noteName(b, sizeof(b), lastOut);
            else
                std::snprintf(b, sizeof(b), "-");
            label(392.0f, 86.0f, ALIGN_RIGHT, 14.0f, 0.5f, theme::kText, a, true);
            label(420.0f, 86.0f, ALIGN_LEFT, 14.0f, 0.5f, theme::kText, b, true);
            beginPath();
            moveTo(398.0f, 86.0f);
            lineTo(413.0f, 86.0f);
            moveTo(409.0f, 82.0f);
            lineTo(413.0f, 86.0f);
            lineTo(409.0f, 90.0f);
            strokeColor(col(theme::kMuted));
            strokeWidth(1.0f);
            stroke();
        }
        static const char* const modes[] = { "NEAREST NOTE", "NOTE ABOVE", "NOTE BELOW", "WHITE KEYS PLAY THE SCALE",
                                             "OUT-OF-SCALE NOTES DROPPED" };
        int l = std::snprintf(line, sizeof(line), "%s", modes[p.mode]);
        if (p.transpose != 0)
            l += std::snprintf(line + l, sizeof(line) - size_t(l), "  %s%d ST", p.transpose > 0 ? "+" : "", p.transpose);
        if (p.octave != 0)
            std::snprintf(line + l, sizeof(line) - size_t(l), "  OCT %s%d", p.octave > 0 ? "+" : "", p.octave);
        label(456.0f, 103.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, line, true);

        drawKeyboard(24.0f, 116.0f, 432.0f, 68.0f, 36, 96, p.key);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int i = int(std::lround(v));
        if (param == 3)
            std::snprintf(buf, n, i > 0 ? "+%d st" : "%d st", i);
        else if (param == 4)
            std::snprintf(buf, n, i > 0 ? "+%d" : "%d", i);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIScale();
}

END_NAMESPACE_DISTRHO
