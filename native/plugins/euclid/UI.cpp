/*
 * NULL JSFX - Euclid (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "EuclidDSP.hpp"
#include "EuclidSliders.hpp"

START_NAMESPACE_DISTRHO

// three rows, as in euclid_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "STEPS", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "PULSES", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "ROTATE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "RATE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "NOTE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "VELOCITY", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "ACCENT", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "GATE", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "SWING", 2, -1, false },
    { 9, NullKnobsUI::kSelect, "RUN", 2, -1, false },
    { 10, NullKnobsUI::kSelect, "CHANNEL", 2, -1, false },
};

static const char* const kNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class UIEuclid : public NullKnobsUI
{
public:
    UIEuclid()
        : NullKnobsUI({ "EUCLID", "MIDI", 480, 120, nullptr, 3, nulleuclid::kUiSliders,
                        nulleuclid::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in euclid_gfx.jsfx: the pattern, one cell per step after Rotate (bright = pulse,
    // brighter = the accented first pulse), the step that plays outlined, and the note and state
    void drawDisplay() override
    {
        float cur = 0.0f, playing = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            cur = t->extra.load(std::memory_order_relaxed);
            playing = t->inLevel.load(std::memory_order_relaxed);
        }
        const nulleuclid::Params p = nulleuclid::paramsFrom([this](uint32_t i) { return nulleuclid::sliderValue(i, value(i)); });
        const int steps = std::max(1, p.steps);
        const int pulses = std::min(p.pulses, steps);
        const int rot = p.rotate % steps;
        const float cw = std::fmin(26.0f, std::floor(432.0f / float(steps)));
        const float x0 = 240.0f - cw * float(steps) / 2.0f;
        const int curStep = int(cur) - 1;
        for (int i = 0; i < steps; ++i)
        {
            int j = i + rot;
            if (j >= steps)
                j -= steps;
            const bool on = (j * pulses) % steps < pulses;
            const float x = x0 + float(i) * cw + 1.0f;
            beginPath();
            rect(x, 86.0f, cw - 2.0f, 34.0f);
            fillColor(on ? col(theme::kAccent, j == 0 && p.accent > 0.0 ? 1.0f : 0.7f) : col(theme::kBackground));
            fill();
            strokeColor(col(theme::kBorder));
            strokeWidth(1.0f);
            stroke();
            if (i == curStep)
            {
                beginPath();
                rect(x + 0.5f, 86.5f, cw - 3.0f, 33.0f);
                strokeColor(col(theme::kText));
                strokeWidth(2.0f);
                stroke();
            }
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d OF %d   ROTATE %d", pulses, steps, rot);
        label(40.0f, 146.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        std::snprintf(buf, sizeof(buf), "%s%d   CH %d   %s", kNotes[p.note % 12], p.note / 12 - 1, p.channel + 1,
                      p.run ? "ALWAYS" : playing > 0.5f ? "PLAYING" : "WAITS FOR PLAY");
        label(440.0f, 146.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nulleuclid::sliderValue(param, v);
        if (param == 4)
        {
            const int k = int(std::lround(x));
            std::snprintf(buf, n, "%s%d", kNotes[k % 12], k / 12 - 1);
        }
        else if (param <= 2 || param == 5)
            std::snprintf(buf, n, "%d", int(x));
        else if (param == 6 || param == 7 || param == 8)
            std::snprintf(buf, n, "%d%%", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIEuclid();
}

END_NAMESPACE_DISTRHO
