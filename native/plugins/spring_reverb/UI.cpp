/*
 * NULL JSFX - Spring Reverb (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpringReverbDSP.hpp"
#include "SpringReverbSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TENSION", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "TONE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DRIP", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "PRE-DELAY", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UISpringReverb : public NullKnobsUI
{
public:
    UISpringReverb()
        : NullKnobsUI({ "SPRING REVERB", "ADVANCED", 480, 120, nullptr, 1, nullspring::kSliders, nullspring::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in spring_reverb_gfx.jsfx: the springs' dispersion, how long each frequency takes through
    // the 40 all-pass stages (0 to 6 ms, log frequency); Tone dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * d;
                    if (f < 20.0f || f > 20000.0f || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::round(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        beginPath();
        for (int k = 0; k <= 2; ++k)
        {
            const float y = std::round(fPY + fPH * float(k) * 0.5f) + 0.5f;
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
        }
        strokeColor(col(theme::kBorder, 0.6f));
        stroke();
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "6 ms");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "3 ms");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(freqX(100.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");
        dashedV(freqX(value(2)));

        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        const double a = nullspring::coefficient(value(1));
        constexpr int n = 400;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double t = double(i) / double(n - 1);
            const double w = 2.0 * M_PI * std::min(20.0 * std::pow(1000.0, t), 0.5 * sr) / sr;
            xs[i] = fPX + fPW * float(t);
            ys[i] = linY(float(nullspring::groupDelay(a, w) / sr * 1000.0), 0.0f, 6.0f);
        }
        curve(xs, ys, n, fPY + fPH);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f s", v);
        else if (param == 2)
            std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISpringReverb();
}

END_NAMESPACE_DISTRHO
