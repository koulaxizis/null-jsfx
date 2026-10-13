/*
 * NULL JSFX - Granular Delay (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "GranularDelaySliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIME", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SIZE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DENSITY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "PITCH", 0, -1, true },
    { 4, NullKnobsUI::kKnob, "SPRAY", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "REVERSE", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "FEEDBACK", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

static float frac(float x) { return x - std::floor(x); }

class UIGranularDelay : public NullKnobsUI
{
public:
    UIGranularDelay()
        : NullKnobsUI({ "GRANULAR DELAY", "ADVANCED", 480, 120, nullptr, 2, nullgranular::kSliders, nullgranular::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // Hann window from t0 over len ms (0 to 3000 ms on the plot), height h (0..1)
    void grain(float t0, float len, float h, uint32_t rgb)
    {
        const float x0 = linX(t0, 0.0f, 3000.0f);
        const float w = std::fmax(3.0f, linX(t0 + len, 0.0f, 3000.0f) - x0);
        const float yb = fPY + fPH, hh = h * fPH * 0.9f;
        const int n = std::max(2, int(w));
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(x0, yb);
        for (int c = 0; c <= n; ++c)
        {
            const float k = float(c) / float(n);
            lineTo(x0 + k * w, yb - (0.5f - 0.5f * std::cos(2.0f * float(M_PI) * k)) * hh);
        }
        fillColor(col(rgb, 0.18f));
        fill();
        strokeColor(col(rgb));
        strokeWidth(1.0f);
        stroke();
        resetScissor();
    }

    // as in granular_delay_gfx.jsfx: the grain cloud in the delay line, 0 to 3 s ago; grains start
    // at Delay Time and scatter over Spray, reversed ones dimmer; the fed-back cloud repeats at
    // every multiple of Delay Time, each time Feedback lower
    void drawDisplay() override
    {
        setPlot(fVX + 6.0f, fVY + 4.0f, fVW - 12.0f, fVH - 20.0f);
        grid(6, 4, false);
        char buf[16];
        for (int i = 1; i <= 5; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%.1f s", i * 0.5);
            label(fPX + fPW * float(i) / 6.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float time = value(0), size = value(1), spray = value(4), rev = value(5), fb = value(6) / 100.0f;
        const int n = std::min(16, int(std::ceil(value(2) * size / 1000.0f)) + 1);
        float lvl = 1.0f;
        for (int e = 1; e <= 4 && lvl > 0.05f && time * float(e) < 3000.0f; ++e, lvl *= fb)
            for (int k = 0; k < n; ++k)
            {
                const float t0 = time * float(e) + spray * 10.0f * frac(k * 0.618034f);
                if (t0 < 3000.0f)
                    grain(t0, size, lvl * (0.55f + 0.45f * frac(k * 0.414214f + 0.3f)),
                          frac(k * 0.754878f + 0.1f) * 100.0f < rev ? theme::kMuted : theme::kAccent);
            }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2)
            std::snprintf(buf, n, "%d /s", int(std::lround(v)));
        else if (param == 3)
            std::snprintf(buf, n, "%+.1f st", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIGranularDelay();
}

END_NAMESPACE_DISTRHO
