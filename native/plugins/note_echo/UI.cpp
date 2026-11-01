/*
 * NULL JSFX - Note Echo (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "NoteEchoDSP.hpp"
#include "NoteEchoSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in note_echo_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "REPEATS", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TIME", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "SYNC", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "NOTE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DECAY", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "PITCH", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "GATE", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "DRY", 1, -1, false },
};

class UINoteEcho : public NullKnobsUI
{
public:
    UINoteEcho()
        : NullKnobsUI({ "NOTE ECHO", "MIDI", 480, 120, nullptr, 2, nullnoteecho::kUiSliders,
                        nullnoteecho::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in note_echo_gfx.jsfx: the note that comes in (dim, unless Dry is Off) and its echoes,
    // one bar each at its time, as high as its velocity, with its pitch step; the interval and
    // the echoes sent
    void drawDisplay() override
    {
        float echoes = 0.0f, ms = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            echoes = t->extra.load(std::memory_order_relaxed);
            ms = t->gr.load(std::memory_order_relaxed);
        }
        const nullnoteecho::Params p = nullnoteecho::paramsFrom([this](uint32_t i) { return nullnoteecho::sliderValue(i, value(i)); });
        const float px = fVX + 8.0f, py = fVY + 18.0f, pw = fVW - 16.0f, ph = fVH - 34.0f;
        setPlot(px, py, pw, ph);
        grid(p.repeats + 1, 4, false);
        const double dec = p.decay / 100.0, gate = p.gate / 100.0;
        char buf[64];
        for (int k = 0; k <= p.repeats; ++k)
        {
            const double v = std::pow(1.0 - dec, double(k));
            const double pk = k * p.pitchStep;
            if (!((k == 0 || std::floor(100.0 * v + 0.5) >= 1.0) && 60.0 + pk >= 0.0 && 60.0 + pk <= 127.0))
                break;
            const float x = fPX + fPW * (float(k) + 0.1f) / float(p.repeats + 1);
            const float w = std::fmax(1.0f, fPW * 0.8f * float(gate) / float(p.repeats + 1));
            const float top = fPY + fPH * (1.0f - float(v));
            beginPath();
            rect(x, top, w, fPH * float(v));
            fillColor(k == 0 ? col(p.dry ? theme::kMuted : theme::kBorder) : col(theme::kAccent, 0.4f + 0.6f * float(v)));
            fill();
            if (p.pitchStep != 0.0 && k > 0)
            {
                std::snprintf(buf, sizeof(buf), "%+d", int(pk));
                label(x, top - 7.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
            }
        }
        std::snprintf(buf, sizeof(buf), "%d REPEATS   %.0f MS", p.repeats, double(ms));
        label(px, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        std::snprintf(buf, sizeof(buf), "ECHOES SENT %d", int(echoes));
        label(px + pw, fVY + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullnoteecho::sliderValue(param, v);
        if (param == 0)
            std::snprintf(buf, n, "%d", int(x));
        else if (param == 1)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param == 4 || param == 6)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 5)
            std::snprintf(buf, n, "%+d st", int(std::lround(x)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UINoteEcho();
}

END_NAMESPACE_DISTRHO
