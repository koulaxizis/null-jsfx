/*
 * NULL JSFX - Glitch FX (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "GlitchFxDSP.hpp"
#include "GlitchFxSliders.hpp"

#include <cstring>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    // STUTTER tab
    { 2, NullKnobsUI::kSelect, "STUTTER", 0, 0, false },
    { 0, NullKnobsUI::kKnob, "RATE", 0, 0, false },
    { 1, NullKnobsUI::kKnob, "LENGTH", 0, 0, false },
    { 13, NullKnobsUI::kKnob, "MIX", 0, 0, false },
    { 4, NullKnobsUI::kSelect, "GATE", 1, 0, false },
    { 3, NullKnobsUI::kKnob, "DEPTH", 1, 0, false },
    { 12, NullKnobsUI::kSelect, "GRANULAR", 1, 0, false },
    { 11, NullKnobsUI::kKnob, "GRAIN", 1, 0, false },
    // EFFECTS tab
    { 6, NullKnobsUI::kSelect, "PITCH", 0, 1, false },
    { 5, NullKnobsUI::kKnob, "SHIFT", 0, 1, true },
    { 8, NullKnobsUI::kSelect, "REVERSE", 0, 1, false },
    { 7, NullKnobsUI::kKnob, "CHANCE", 0, 1, false },
    { 10, NullKnobsUI::kSelect, "TAPE STOP", 1, 1, false },
    { 9, NullKnobsUI::kKnob, "TAPE TIME", 1, 1, false },
    { 14, NullKnobsUI::kSelect, "TRIGGER", 1, 1, false },
};

class UIGlitchFx : public NullKnobsUI
{
public:
    UIGlitchFx()
        : NullKnobsUI({ "GLITCH FX", "ADVANCED", 480, 120, "STUTTER|EFFECTS", 2, nullglitch::kSliders,
                        nullglitch::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in glitch_fx_gfx.jsfx: one second of the wet level (loop seams or grain windows and
    // the gate), events dashed, the playhead inside the stutter loop as a thin line; under
    // Trigger MIDI the effects the held notes play, from the note-on, and their names
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(5, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "100%");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "50%");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        char buf[16];
        for (int i = 1; i <= 4; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%.1f s", i * 0.2);
            label(fPX + fPW * float(i) / 5.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }

        double bpm = 120.0;
        int held = 0;
        if (nulljsfx::Taps* t = taps())
        {
            const float tp = t->extra.load(std::memory_order_relaxed);
            if (tp > 0.0f)
                bpm = tp;
            held = int(std::lround(t->aux[0].load(std::memory_order_relaxed)));
        }
        nullglitch::Params p = nullglitch::paramsFrom([this](uint32_t i) { return value(i); });
        if (p.midi)
        {
            // one event at the note-on; the switches are the held notes
            p.rate = 0.5;
            p.stutter = (held & 1) != 0;
            p.gate = (held & 2) != 0;
            p.pitch = (held & 4) != 0;
            p.granular = (held & 32) != 0;
        }

        for (double t = 0.0; t < 1.0; t += 1.0 / p.rate)
            dashedV(linX(float(t), 0.0f, 1.0f));

        const int n = int(fPW);
        float xs[512], ys[512];
        beginPath();
        double ph0 = 0.0;
        for (int i = 0; i <= n && i < 512; ++i)
        {
            double head;
            const double lv = nullglitch::displayLevel(p, bpm, double(i) / n, &head);
            xs[i] = fPX + fPW * float(i) / float(n);
            ys[i] = linY(float(lv), 0.0f, 1.0f);
            if (head >= 0.0)
            {
                const float hy = linY(float(head), 0.0f, 1.0f);
                if (i > 0 && head >= ph0)
                    lineTo(xs[i], hy);
                else
                    moveTo(xs[i], hy);
                ph0 = head;
            }
        }
        strokeColor(col(theme::kMuted, 0.8f));
        strokeWidth(1.0f);
        stroke();
        curve(xs, ys, std::min(n + 1, 512), fPY + fPH);

        // Trigger MIDI: the held effects by name
        if (p.midi)
        {
            static const char* const kNames[] = { "STUTTER", "GATE", "PITCH", "REVERSE", "TAPE STOP", "GRANULAR" };
            char m[96] = "MIDI:";
            for (int k = 0; k < 6; ++k)
                if (held & (1 << k))
                {
                    std::strncat(m, " ", sizeof(m) - std::strlen(m) - 1);
                    std::strncat(m, kNames[k], sizeof(m) - std::strlen(m) - 1);
                }
            if (!held)
                std::snprintf(m, sizeof(m), "MIDI: HOLD C3 TO F3");
            label(fPX + 6.0f, fPY + 8.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, m);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        switch (param)
        {
        case 0: std::snprintf(buf, n, "%.1f Hz", nullglitch::tenths(v)); break;
        case 1: std::snprintf(buf, n, "1/%d", int(std::lround(v))); break;
        case 5:
            std::snprintf(buf, n, nullglitch::tenths(v) > 0.0 ? "+%.1f st" : "%.1f st", nullglitch::tenths(v));
            break;
        default: NullKnobsUI::formatValue(param, v, buf, n);
        }
    }
};

UI* createUI()
{
    return new UIGlitchFx();
}

END_NAMESPACE_DISTRHO
