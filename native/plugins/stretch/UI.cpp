/*
 * NULL JSFX - Stretch (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StretchDSP.hpp"

START_NAMESPACE_DISTRHO

class UIStretch : public NullOneSliderUI
{
public:
    UIStretch()
        : NullOneSliderUI({ "STRETCH", "AMOUNT", "DOWN", "UP", -100.0f, 100.0f, 0.0f, true, false }) {}

protected:
    // grain length (s), as set in the JSFX's @slider (before rounding to samples)
    static double grainSec(double v) { return v < 0.0 ? 0.04 - v * 0.2 : 0.04 - v * 0.015; }

    // as in stretch_gfx.jsfx: the two crossfading grain envelopes over 0.5 s (the second one
    // dim), scaled by the wet mix
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(10, 2, false);
        label(fPX - 6.0f, std::round(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "1");
        label(fPX - 6.0f, std::round(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        const float capY = fPY + fPH + 11.0f;
        label(fPX + fPW * 0.2f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.1 s");
        label(fPX + fPW * 0.4f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.2 s");
        label(fPX + fPW * 0.6f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.3 s");
        label(fPX + fPW * 0.8f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.4 s");

        const double v = value() / 100.0;
        if (std::lround(value()) == 0)
            return;
        const double period = grainSec(v) / std::fabs(1.0 - std::pow(2.0, v));
        const double wet = std::fmin(1.0, std::fabs(v) * 4.0);
        constexpr int n = 285;
        float xs[n], y1[n], y2[n];
        for (int i = 0; i < n; ++i)
        {
            const double t = 0.5 * double(i) / double(n - 1);
            double p = t / period;
            p -= std::floor(p);
            double p2 = p + 0.5;
            p2 -= std::floor(p2);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            y1[i] = linY(float((0.5 - 0.5 * std::cos(2.0 * nullstretch::kPi * p)) * wet), 0.0f, 1.0f);
            y2[i] = linY(float((0.5 - 0.5 * std::cos(2.0 * nullstretch::kPi * p2)) * wet), 0.0f, 1.0f);
        }
        beginPath();
        moveTo(xs[0], y2[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], y2[i]);
        strokeColor(col(theme::kAccent, 0.5f));
        strokeWidth(1.0f);
        stroke();
        curve(xs, y1, n, fPY + fPH);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "UNTOUCHED");
        else
            std::snprintf(buf, n, "PITCH %+.1f st  GRAIN %d ms", double(v) * 0.12,
                          int(std::floor(grainSec(v / 100.0) * 1000.0 + 0.5)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIStretch)
};

UI* createUI()
{
    return new UIStretch();
}

END_NAMESPACE_DISTRHO
