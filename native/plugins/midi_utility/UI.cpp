/*
 * NULL JSFX - MIDI Utility (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullKeyboardUI.hpp"
#include "MidiUtilityDSP.hpp"
#include "MidiUtilitySliders.hpp"

START_NAMESPACE_DISTRHO

// as midi_utility_gfx.jsfx: three tabs
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TRANSPOSE", 0, 0, true },
    { 1, NullKnobsUI::kSelect, "CHANNEL", 0, 0, false },
    { 6, NullKnobsUI::kKnob, "KEY LOW", 0, 0, false },
    { 7, NullKnobsUI::kKnob, "KEY HIGH", 0, 0, false },
    { 2, NullKnobsUI::kKnob, "CURVE", 0, 1, true },
    { 3, NullKnobsUI::kKnob, "VEL MIN", 0, 1, false },
    { 4, NullKnobsUI::kKnob, "VEL MAX", 0, 1, false },
    { 5, NullKnobsUI::kKnob, "FIXED", 0, 1, false },
    { 8, NullKnobsUI::kKnob, "VEL LOW", 0, 1, false },
    { 9, NullKnobsUI::kKnob, "VEL HIGH", 0, 1, false },
    { 10, NullKnobsUI::kSelect, "CC", 0, 2, false },
    { 11, NullKnobsUI::kSelect, "PITCH BEND", 0, 2, false },
    { 12, NullKnobsUI::kSelect, "AFTERTOUCH", 0, 2, false },
    { 13, NullKnobsUI::kSelect, "PROGRAM", 0, 2, false },
};

static const char* const kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class UIMidiUtility : public NullKeyboardUI
{
public:
    UIMidiUtility()
        : NullKeyboardUI({ "MIDI UTILITY", "MIDI", 480, 140, "NOTES|VELOCITY|FILTER", 1, nullmidiutil::kUiSliders,
                           nullmidiutil::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true })
    {
        fTintRange = true;
    }

protected:
    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    static void noteName(char* buf, size_t n, int note) { std::snprintf(buf, n, "%s%d", kNames[note % 12], note / 12 - 1); }

    void line(float x0, float y0, float x1, float y1, uint32_t rgb)
    {
        beginPath();
        moveTo(x0, y0);
        lineTo(x1, y1);
        strokeColor(col(rgb));
        strokeWidth(1.0f);
        stroke();
    }

    // as in midi_utility_gfx.jsfx: the velocity curve (the last note's velocity marked), the last
    // key and the note and velocity it played, the channel and what is blocked, and a keyboard
    // C1..C8 with the keys in range tinted, the notes sounding silver and the held keys marked
    void drawDisplay() override
    {
        using namespace nullmidiutil;
        Params p;
        p.transpose = iv(0); p.channel = iv(1); p.curve = value(2); p.velMin = iv(3); p.velMax = iv(4);
        p.fixed = iv(5); p.keyLow = iv(6); p.keyHigh = iv(7);
        int last = -1, lastOut = -1, vin = 0, vout = 0;
        if (nulljsfx::Taps* t = taps())
        {
            last = int(std::lround(t->extra.load(std::memory_order_relaxed))) - 1;
            lastOut = int(std::lround(t->inLevel.load(std::memory_order_relaxed))) - 1;
            vin = int(std::lround(t->outLevel.load(std::memory_order_relaxed)));
            vout = int(std::lround(t->gr.load(std::memory_order_relaxed)));
        }
        readNotes();
        for (int n = 0; n < 128; ++n)
            fInRange[n] = n >= p.keyLow && n <= p.keyHigh;

        box(24.0f, 62.0f, 120.0f, 76.0f, theme::kBackground);
        line(24.0f, 138.0f, 144.0f, 62.0f, theme::kBorder);
        auto vel = [&](int v) { return p.fixed > 0 ? p.fixed : outVelocity(v, p); };
        beginPath();
        moveTo(24.0f, 138.0f - float(vel(1) - 1) * 76.0f / 126.0f);
        for (int v = 2; v <= 127; ++v)
            lineTo(24.0f + float(v - 1) * 120.0f / 126.0f, 138.0f - float(vel(v) - 1) * 76.0f / 126.0f);
        strokeColor(col(theme::kAccent));
        strokeWidth(1.0f);
        stroke();
        if (lastOut >= 0)
            dot(24.0f + float(vin - 1) * 120.0f / 126.0f, 138.0f - float(vout - 1) * 76.0f / 126.0f, 2.5f, theme::kText);
        label(84.0f, 145.0f, ALIGN_CENTER, 7.0f, 1.2f, theme::kMuted, "VELOCITY IN", true);

        char a[16], b[16], buf[64];
        label(162.0f, 68.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted, last >= 0 ? "LAST NOTE" : "PLAY A KEY", true);
        if (last >= 0)
        {
            noteName(a, sizeof(a), last);
            if (lastOut >= 0)
                noteName(b, sizeof(b), lastOut);
            else
                std::snprintf(b, sizeof(b), "-");
            label(162.0f, 86.0f, ALIGN_LEFT, 14.0f, 0.5f, theme::kText, a, true);
            label(222.0f, 86.0f, ALIGN_LEFT, 14.0f, 0.5f, theme::kText, b, true);
            line(200.0f, 86.0f, 215.0f, 86.0f, theme::kMuted);
            line(211.0f, 82.0f, 215.0f, 86.0f, theme::kMuted);
            line(211.0f, 90.0f, 215.0f, 86.0f, theme::kMuted);
            if (lastOut >= 0)
                std::snprintf(buf, sizeof(buf), "VELOCITY %d > %d", vin, vout);
            else
                std::snprintf(buf, sizeof(buf), "VELOCITY %d, OUT OF RANGE", vin);
            label(162.0f, 103.0f, ALIGN_LEFT, 7.5f, 1.2f, theme::kMuted, buf, true);
        }
        if (p.channel > 0)
            std::snprintf(buf, sizeof(buf), "CHANNEL %d", p.channel);
        else
            std::snprintf(buf, sizeof(buf), "CHANNEL THRU");
        label(456.0f, 68.0f, ALIGN_RIGHT, 7.5f, 1.5f, theme::kMuted, buf, true);
        char blocks[48] = "";
        if (iv(10)) std::strcat(blocks, " CC");
        if (iv(11)) std::strcat(blocks, " BEND");
        if (iv(12)) std::strcat(blocks, " TOUCH");
        if (iv(13)) std::strcat(blocks, " PROGRAM");
        if (blocks[0])
            std::snprintf(buf, sizeof(buf), "BLOCKS%s", blocks);
        else
            std::snprintf(buf, sizeof(buf), "PASSES ALL");
        label(456.0f, 103.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, buf, true);

        drawKeyboard(24.0f, 150.0f, 432.0f, 36.0f, 24, 108, -1);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int i = int(std::lround(v));
        if (param == 0)
            std::snprintf(buf, n, i > 0 ? "+%d st" : "%d st", i);
        else if (param == 2)
            std::snprintf(buf, n, i > 0 ? "+%d%%" : "%d%%", i);
        else if (param == 6 || param == 7)
            noteName(buf, n, i);
        else if (param == 5 && i == 0)
            std::snprintf(buf, n, "OFF");
        else if (param == 3 || param == 4 || param == 5 || param == 8 || param == 9)
            std::snprintf(buf, n, "%d", i);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIMidiUtility();
}

END_NAMESPACE_DISTRHO
