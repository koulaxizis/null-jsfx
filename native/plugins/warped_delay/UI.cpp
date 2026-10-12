/*
 * NULL JSFX - Warped Delay (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "WarpedDelaySliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIME", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "FEEDBACK", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DRIVE", 1, -1, false },
    { 5, NullKnobsUI::kSelect, "FILTER", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "FREQ", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIWarpedDelay : public NullKnobsUI
{
public:
    UIWarpedDelay()
        : NullKnobsUI({ "WARPED DELAY", "ADVANCED", 480, 120, nullptr, 2, nullwarped::kSliders, nullwarped::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in warped_delay_gfx.jsfx: the delay-time warble over 1 s as an offset from Time
    // (-50 to +50 ms): left channel (sine) bright, right channel (cosine) dim
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
            std::snprintf(buf, sizeof(buf), "%.2f s", i * 0.25);
            label(fPX + fPW * float(i) / 4.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float rate = value(1), depth = std::fmin(value(2), value(0) - 0.5f);
        const int n = int(fPW) + 1;
        float xs[512], ys[512];
        const int m = std::min(n, 512);
        for (int i = 0; i < m; ++i)
        {
            const float t = float(i) / float(m - 1);
            xs[i] = fPX + fPW * t;
            ys[i] = linY(std::cos(2.0f * float(M_PI) * rate * t) * depth, -50.0f, 50.0f);
        }
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < m; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kMuted, 0.8f));
        strokeWidth(1.0f);
        stroke();
        for (int i = 0; i < m; ++i)
            ys[i] = linY(std::sin(2.0f * float(M_PI) * rate * float(i) / float(m - 1)) * depth, -50.0f, 50.0f);
        curve(xs, ys, m);
        label(fVX + 34.0f, fVY + 12.0f, ALIGN_LEFT, 7.5f, 0.0f, theme::kAccent, "L", true);
        label(fVX + 46.0f, fVY + 12.0f, ALIGN_LEFT, 7.5f, 0.0f, theme::kMuted, "R", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%.1f Hz", v);
        else if (param == 2)
            std::snprintf(buf, n, "%.1f ms", v);
        else if (param == 6)
        {
            if (v < 1000.0f)
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
            else
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIWarpedDelay();
}

END_NAMESPACE_DISTRHO
