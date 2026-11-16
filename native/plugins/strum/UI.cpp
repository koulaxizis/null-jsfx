/*
 * NULL JSFX - Strum (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StrumDSP.hpp"
#include "StrumSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in strum_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "DIRECTION", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SPEED", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "RHYTHM", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "GATE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "UPSTROKE", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "HUMANIZE", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "COLLECT", 1, -1, false },
};

class UIStrum : public NullKnobsUI
{
public:
    UIStrum()
        : NullKnobsUI({ "STRUM", "MIDI", 480, 120, nullptr, 2, nullstrum::kUiSliders,
                        nullstrum::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in strum_gfx.jsfx: the strokes of the rhythm (D down, U up, - rest, ? random), the one
    // last played lit while a rhythm runs; the strums and the keys held
    void drawDisplay() override
    {
        float strums = 0.0f, step = -1.0f, held = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            strums = t->extra.load(std::memory_order_relaxed);
            step = t->gr.load(std::memory_order_relaxed);
            held = t->aux[0].load(std::memory_order_relaxed);
        }
        const nullstrum::Params p = nullstrum::paramsFrom([this](uint32_t i) { return nullstrum::sliderValue(i, value(i)); });
        const float px = fVX + 8.0f, py = fVY + 18.0f, pw = fVW - 16.0f, ph = fVH - 34.0f;
        setPlot(px, py, pw, ph);
        const int n = p.rhythm == 0 ? 1 : 8;
        const int cur = step >= 1.0f ? (int(step) - 1) % 8 : -1;
        for (int i = 0; i < n; ++i)
        {
            const int d = nullstrum::StrumProcessor::pattern(p.rhythm, i);
            const char* s = d == 1 ? "D" : d == 2 ? "U" : d == 0 ? "-"
                          : p.direction == 0 ? "D" : p.direction == 1 ? "U" : p.direction == 2 ? (i % 2 ? "U" : "D") : "?";
            const float x = fPX + fPW * float(i) / float(n) + 2.0f, w = fPW / float(n) - 4.0f;
            const bool lit = i == cur && p.rhythm > 0;
            beginPath();
            rect(x, fPY + 4.0f, w, fPH - 8.0f);
            fillColor(lit ? col(theme::kAccent, 0.35f) : col(theme::kBorder, d == 0 ? 0.3f : 0.6f));
            fill();
            label(x + w * 0.5f, fPY + fPH * 0.5f, ALIGN_CENTER, 14.0f, 0.0f, lit ? theme::kText : theme::kMuted, s);
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s   %d MS", nullstrum::kUi2[p.rhythm], int(p.speed));
        label(px, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        std::snprintf(buf, sizeof(buf), "STRUMS %d   KEYS %d", int(strums), int(held));
        label(px + pw, fVY + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullstrum::sliderValue(param, v);
        if (param == 1 || param == 6 || param == 7)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param >= 3 && param <= 5)
            std::snprintf(buf, n, "%d%%", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIStrum();
}

END_NAMESPACE_DISTRHO
