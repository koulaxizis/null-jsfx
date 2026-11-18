/*
 * NULL JSFX - Bass Presence (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BassPresenceDSP.hpp"

START_NAMESPACE_DISTRHO

class UIBassPresence : public NullOneSliderUI
{
public:
    UIBassPresence()
        : NullOneSliderUI({ "BASS PRESENCE", "AMOUNT", "CLEAN", "PRESENT", 0.0f, 100.0f, 40.0f, false, false }) {}

protected:
    // as in bass_presence_gfx.jsfx: the harmonic curve, the low band in across, what it makes
    // up (before the band-pass and the level of Amount)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, std::round(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "1");
        label(fPX - 6.0f, std::round(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, std::round(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");
        const float capY = fPY + fPH + 11.0f;
        label(fPX + fPW * 0.25f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "-0.5");
        label(fPX + fPW * 0.50f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "LOWS IN");
        label(fPX + fPW * 0.75f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.5");

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kMuted, 0.6f));
        strokeWidth(1.0f);
        stroke();

        const nullbasspresence::Params p = nullbasspresence::paramsFor(value());
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullbasspresence::curve(p, 2.0 * double(i) / double(n - 1) - 1.0)), -1.0f, 1.0f);
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
        const nullbasspresence::Params p = nullbasspresence::paramsFor(v);
        std::snprintf(buf, n, "HARMONICS 180 HZ - 1.8 KHZ  %+.1f DB", 20.0 * std::log10(p.g));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIBassPresence)
};

UI* createUI()
{
    return new UIBassPresence();
}

END_NAMESPACE_DISTRHO
