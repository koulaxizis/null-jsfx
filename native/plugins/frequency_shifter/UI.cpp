/*
 * NULL JSFX - Frequency Shifter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "FrequencyShifterDSP.hpp"
#include "FrequencyShifterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SHIFT", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "OFFSET", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "FEEDBACK", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "FB DELAY", 0, -1, false },
};

class UIFrequencyShifter : public NullKnobsUI
{
public:
    UIFrequencyShifter()
        : NullKnobsUI({ "FREQUENCY SHIFTER", "ADVANCED", 480, 120, nullptr, 1, nullfreqshift::kSliders,
                        nullfreqshift::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // a partial at f Hz (folded at 0 Hz) with amplitude a, w design units wide (fs_bar)
    void bar(float f, float a, float w)
    {
        f = std::fabs(f);
        if (f > 5000.0f || a <= 0.003f)
            return;
        const float x = linX(f, 0.0f, 5000.0f), y = linY(a, 0.0f, 0.6f);
        rect(x - w * 0.5f, y, w, fPY + fPH - y);
    }

    // as in frequency_shifter_gfx.jsfx: the partials of a 220 Hz tone (thin, dry level) and
    // where the shifter moves them (L bright, R dim, the feedback spiral fainter), 0 to 5 kHz
    void drawDisplay() override
    {
        setPlot(fVX + 4.0f, fVY + 16.0f, fVW - 8.0f, fVH - 32.0f);
        grid(5, 4, false);
        char buf[32];
        for (int i = 1; i <= 4; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%dk", i);
            label(fPX + fPW * float(i) / 5.0f, fPY + fPH + 8.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float shift = value(0), off = value(1), fb = value(2) / 100.0f, mix = value(3) / 100.0f;
        std::snprintf(buf, sizeof(buf), "L %+.1f Hz", shift);
        label(fPX, fPY - 8.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        std::snprintf(buf, sizeof(buf), "R %+.1f Hz", shift + off);
        label(fPX + fPW, fPY - 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        beginPath();
        for (int n = 1; n <= 8; ++n)
            bar(220.0f * n, 0.9f / std::sqrt(float(n)) * (1.0f - mix), 1.0f);
        fillColor(col(theme::kMuted));
        fill();
        beginPath();
        for (int n = 1; n <= 8; ++n)
            for (int k = 1; k <= 3; ++k)
                bar(220.0f * n + float(k + 1) * shift, 0.9f / std::sqrt(float(n)) * mix * std::pow(fb, float(k)), 2.0f);
        fillColor(col(theme::kAccent, 0.35f));
        fill();
        if (off != 0.0f)
        {
            beginPath();
            for (int n = 1; n <= 8; ++n)
                bar(220.0f * n + shift + off, 0.9f / std::sqrt(float(n)) * mix, 2.0f);
            fillColor(col(theme::kAccent, 0.45f));
            fill();
        }
        beginPath();
        for (int n = 1; n <= 8; ++n)
            bar(220.0f * n + shift, 0.9f / std::sqrt(float(n)) * mix, 2.0f);
        fillColor(col(theme::kAccent));
        fill();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param <= 1)
            std::snprintf(buf, n, "%+.1f Hz", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIFrequencyShifter();
}

END_NAMESPACE_DISTRHO
