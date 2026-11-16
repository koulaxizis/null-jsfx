/*
 * NULL JSFX - Warmer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "WarmerDSP.hpp"

START_NAMESPACE_DISTRHO

class UIWarmer : public NullOneSliderUI
{
public:
    UIWarmer()
        : NullOneSliderUI({ "WARMER", "AMOUNT", "CLEAN", "WARM", 0.0f, 100.0f, 40.0f, false, false }) {}

protected:
    // as in warmer_gfx.jsfx: the curve of the lows and low mids, input -1 to 1 across, output
    // up (the straight line is the untouched signal)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, std::round(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "1");
        label(fPX - 6.0f, std::round(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, std::round(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");
        const float capY = fPY + fPH + 11.0f;
        label(fPX + fPW * 0.25f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "-0.5");
        label(fPX + fPW * 0.50f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "IN");
        label(fPX + fPW * 0.75f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.5");

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kMuted, 0.6f));
        strokeWidth(1.0f);
        stroke();

        const nullwarmer::Params p = nullwarmer::paramsFor(value(), 48000.0);
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullwarmer::curve(p, 2.0 * double(i) / double(n - 1) - 1.0)), -1.0f, 1.0f);
        }
        curve(xs, ys, n);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
        {
            std::snprintf(buf, n, "CLEAN");
            return;
        }
        const nullwarmer::Params p = nullwarmer::paramsFor(v, 48000.0);
        std::snprintf(buf, n, "LOWS %+.1f dB  TOP %d kHz", 20.0 * std::log10(p.lo),
                      int(std::floor((20000.0 - 11000.0 * p.a) / 1000.0 + 0.5)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIWarmer)
};

UI* createUI()
{
    return new UIWarmer();
}

END_NAMESPACE_DISTRHO
