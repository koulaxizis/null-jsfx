/*
 * NULL JSFX - MIDI LFO (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullKeyboardUI.hpp"
#include "MidiLfoDSP.hpp"
#include "MidiLfoSliders.hpp"

START_NAMESPACE_DISTRHO

// as midi_lfo_gfx.jsfx: two tabs
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SHAPE", 0, 0, false },
    { 1, NullKnobsUI::kSelect, "SYNC", 0, 0, false },
    { 2, NullKnobsUI::kKnob, "RATE", 0, 0, false, true },
    { 3, NullKnobsUI::kSelect, "LENGTH", 0, 0, false },
    { 4, NullKnobsUI::kKnob, "PHASE", 0, 0, false },
    { 10, NullKnobsUI::kSelect, "RETRIGGER", 0, 0, false },
    { 5, NullKnobsUI::kSelect, "DESTINATION", 0, 1, false },
    { 6, NullKnobsUI::kKnob, "CC NUMBER", 0, 1, false },
    { 7, NullKnobsUI::kKnob, "CHANNEL", 0, 1, false },
    { 8, NullKnobsUI::kKnob, "MIN", 0, 1, false },
    { 9, NullKnobsUI::kKnob, "MAX", 0, 1, false },
    { 11, NullKnobsUI::kKnob, "SEED", 0, 1, false },
};

class UIMidiLfo : public NullKeyboardUI
{
public:
    UIMidiLfo()
        : NullKeyboardUI({ "MIDI LFO", "MIDI", 480, 140, "LFO|OUTPUT", 1, nullmidilfo::kUiSliders,
                           nullmidilfo::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true })
    {
    }

protected:
    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    void line(float x0, float y0, float x1, float y1, uint32_t rgb)
    {
        beginPath();
        moveTo(x0, y0);
        lineTo(x1, y1);
        strokeColor(col(rgb));
        strokeWidth(1.0f);
        stroke();
    }

    // as in midi_lfo_gfx.jsfx: one cycle of the wave from Min to Max with the phase now marked,
    // the value sent, where it goes and how fast, and the value on the Min..Max range
    void drawDisplay() override
    {
        using namespace nullmidilfo;
        const int shape = iv(0), sync = iv(1), len = std::max(0, std::min(15, iv(3)));
        const int dest = iv(5), cc = iv(6), chan = iv(7);
        const double vmin = std::round(value(8)) / 100.0, vmax = std::round(value(9)) / 100.0;
        double ph = 0.0, u = 0.5, prev = 0.0, cur = 0.0, tempo = 120.0;
        bool playing = false;
        if (nulljsfx::Taps* t = taps())
        {
            ph = t->extra.load(std::memory_order_relaxed);
            u = t->inLevel.load(std::memory_order_relaxed);
            prev = t->outLevel.load(std::memory_order_relaxed);
            cur = t->gr.load(std::memory_order_relaxed);
            playing = t->aux[0].load(std::memory_order_relaxed) > 0.5f;
            tempo = t->aux[1].load(std::memory_order_relaxed);
        }
        auto at = [&](double p) { return vmin + (vmax - vmin) * (MidiLfoProcessor::wave(shape, p, prev, cur) + 1.0) / 2.0; };

        box(24.0f, 62.0f, 280.0f, 76.0f, theme::kBackground);
        line(24.0f, float(138.0 - vmin * 76.0), 304.0f, float(138.0 - vmin * 76.0), theme::kBorder);
        line(24.0f, float(138.0 - vmax * 76.0), 304.0f, float(138.0 - vmax * 76.0), theme::kBorder);
        beginPath();
        moveTo(24.0f, float(138.0 - at(0.0) * 76.0));
        for (int i = 1; i <= 140; ++i)
            lineTo(24.0f + float(i) * 2.0f, float(138.0 - at(i / 140.0) * 76.0));
        strokeColor(col(theme::kAccent));
        strokeWidth(1.0f);
        stroke();
        const double v = vmin + (vmax - vmin) * u;
        const float px = 24.0f + float(ph * 280.0);
        line(px, 62.0f, px, 138.0f, theme::kMuted);
        dot(px, float(138.0 - v * 76.0), 2.5f, theme::kText);
        label(164.0f, 145.0f, ALIGN_CENTER, 7.0f, 1.2f, theme::kMuted, "ONE CYCLE", true);

        char buf[64];
        label(322.0f, 68.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted, "VALUE", true);
        if (dest == 1)
            std::snprintf(buf, sizeof(buf), "%d", int(std::floor(v * 16383.0 + 0.5)) - 8192);
        else
            std::snprintf(buf, sizeof(buf), "%d", int(std::floor(v * 127.0 + 0.5)));
        label(322.0f, 86.0f, ALIGN_LEFT, 14.0f, 0.5f, theme::kText, buf, true);
        if (dest == 0)
            std::snprintf(buf, sizeof(buf), "CC %d, CHANNEL %d", cc, chan);
        else if (dest == 1)
            std::snprintf(buf, sizeof(buf), "PITCH BEND, CHANNEL %d", chan);
        else
            std::snprintf(buf, sizeof(buf), "PRESSURE, CHANNEL %d", chan);
        label(322.0f, 103.0f, ALIGN_LEFT, 7.5f, 1.2f, theme::kMuted, buf, true);
        if (!sync)
            std::snprintf(buf, sizeof(buf), "%.2f HZ", jsfxValue(kSliders[2], value(2)));
        else if (playing)
            std::snprintf(buf, sizeof(buf), "%s, ON THE BEAT", kLengthShort[len]);
        else
            std::snprintf(buf, sizeof(buf), "%s AT %d BPM", kLengthShort[len], int(std::floor(tempo + 0.5)));
        label(322.0f, 118.0f, ALIGN_LEFT, 7.5f, 1.2f, theme::kMuted, buf, true);

        box(24.0f, 150.0f, 432.0f, 36.0f, theme::kBackground);
        box(24.0f + float(std::min(vmin, vmax) * 432.0), 150.0f, float(std::fabs(vmax - vmin) * 432.0), 36.0f, theme::kBorder);
        box(24.0f, 178.0f, float(v * 432.0), 8.0f, theme::kAccent);
        line(24.0f + float(v * 432.0), 150.0f, 24.0f + float(v * 432.0), 185.0f, theme::kText);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int i = int(std::lround(v));
        if (param == 2)
            std::snprintf(buf, n, "%.2f Hz", jsfxValue(nullmidilfo::kSliders[2], v));
        else if (param == 4)
            std::snprintf(buf, n, "%d deg", i);
        else if (param == 8 || param == 9)
            std::snprintf(buf, n, "%d%%", i);
        else if (param == 6 || param == 7 || param == 11)
            std::snprintf(buf, n, "%d", i);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIMidiLfo();
}

END_NAMESPACE_DISTRHO
