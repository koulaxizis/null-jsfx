/*
 * NULL JSFX - Stereo Width (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StereoWidthDSP.hpp"
#include "StereoWidthSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "WIDTH", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "CROSSOVER", 0, -1, false },
};

class UIStereoWidth : public NullKnobsUI
{
public:
    UIStereoWidth()
        : NullKnobsUI({ "STEREO WIDTH", "ESSENTIALS", 480, 120, nullptr, 1, nullstereowidth::kSliders,
                        nullstereowidth::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // log-frequency grid (20 Hz to 20 kHz) without dB lines (as sw_freq_grid in the GFX)
    void freqGrid()
    {
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
        const float capY = fPY + fPH + 11.0f;
        label(freqX(100.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");
    }

    // as in stereo_width_gfx.jsfx: left, vectorscope of the output; right, the side level
    // across frequency (0 to 200 % of the input's), the crossover dashed
    void drawDisplay() override
    {
        setPlot(32.0f, 66.0f, 96.0f, 96.0f);
        vectorscope(2.0f);

        setPlot(186.0f, 66.0f, 262.0f, 80.0f);
        freqGrid();
        for (int i = 0; i <= 4; ++i)
        {
            const float y = std::round(fPY + fPH * float(i) / 4.0f) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(i == 2 ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
        }
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "200%");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "100%");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0%");

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullstereowidth::Params p = nullstereowidth::paramsFor(value(0), value(1), value(2), sr);
        if (p.xover > 0.0)
            dashedV(freqX(float(p.xover)));

        constexpr int n = 263;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullstereowidth::sideGain(p, f, sr)), 0.0f, 2.0f);
        }
        curve(xs, ys, n, linY(0.0f, 0.0f, 2.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2 && std::lround(v) == 0)
            std::snprintf(buf, n, "OFF");
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIStereoWidth();
}

END_NAMESPACE_DISTRHO
