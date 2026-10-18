/*
 * NULL JSFX - Vinyl Age (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "VinylAgeDSP.hpp"
#include "VinylAgeSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "WOW", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "FLUTTER", 0, -1, false },
    { 8, NullKnobsUI::kSelect, "RPM", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "CRACKLE", 0, -1, false },
    { 9, NullKnobsUI::kKnob, "SCRATCHES", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "NOISE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "BITS", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "RATE", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "LOWPASS", 1, -1, false },
    { 10, NullKnobsUI::kSelect, "MONO", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIVinylAge : public NullKnobsUI
{
public:
    UIVinylAge()
        : NullKnobsUI({ "VINYL AGE", "ADVANCED", 480, 120, nullptr, 2, nullvinyl::kSliders, nullvinyl::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in vinyl_age_gfx.jsfx: the wow + flutter drift over 2 s as the delay offset around the
    // 50 ms centre (-50 to +50 ms), with the crackle clicks behind it
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(4, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+50");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-50");
        char buf[16];
        for (int i = 1; i <= 3; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%.1f s", i * 0.5);
            label(fPX + fPW * float(i) / 4.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float crackle = value(2) / 100.0f;
        if (crackle > 0.0f)
        {
            const float dt = 1.0f / (5.0f + crackle * 150.0f), cy = fPY + fPH * 0.5f;
            beginPath();
            int k = 0;
            for (float t = dt; t < 2.0f; t += dt, ++k)
            {
                const float x = std::round(linX(t, 0.0f, 2.0f)) + 0.5f;
                const float h = k * 0.618034f + 0.2f;
                moveTo(x, cy);
                lineTo(x, cy - ((h - std::floor(h)) * 2.0f - 1.0f) * crackle * fPH * 0.5f);
            }
            strokeColor(col(theme::kMuted, 0.6f));
            strokeWidth(1.0f);
            stroke();
        }
        const int rpm = std::max(0, std::min(2, int(std::lround(value(8)))));
        static const char* const kRpmText[] = { "33 RPM", "45 RPM", "78 RPM" };
        label(fPX + fPW - 6.0f, fPY + 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, kRpmText[rpm]);
        constexpr int n = 400;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double t = 2.0 * double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(t * 0.5);
            ys[i] = linY(float(nullvinyl::modulationMs(t, value(0), value(1), rpm)), -50.0f, 50.0f);
        }
        curve(xs, ys, n);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4)
            std::snprintf(buf, n, "%d bit", int(std::lround(v)));
        else if (param >= 5 && param <= 6)
            std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIVinylAge();
}

END_NAMESPACE_DISTRHO
