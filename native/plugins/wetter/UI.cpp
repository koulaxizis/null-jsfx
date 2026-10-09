/*
 * NULL JSFX - Wetter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "WetterDSP.hpp"

START_NAMESPACE_DISTRHO

class UIWetter : public NullOneSliderUI
{
public:
    UIWetter()
        : NullOneSliderUI({ "WETTER", "AMOUNT", "DRY", "WET", 0.0f, 100.0f, 50.0f, false, false }) {}

protected:
    // as in wetter_gfx.jsfx: the dry impulse at 0 and the reverb tail over 2 s (0 to -80 dB),
    // level relative to the dry signal, falling 60 dB over the reverb time
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, std::round(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, std::round(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-40");
        label(fPX - 6.0f, std::round(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-80");
        const float capY = fPY + fPH + 11.0f;
        label(fPX + fPW * 0.25f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.5 s");
        label(fPX + fPW * 0.50f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1 s");
        label(fPX + fPW * 0.75f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1.5 s");

        beginPath();
        rect(fPX, fPY, 2.0f, fPH);
        fillColor(col(theme::kAccent));
        fill();

        if (value() <= 0.0f)
            return;
        const nullwetter::Params p = nullwetter::paramsFor(value());
        const double rt = nullwetter::rt60(p.fb);
        const double l0 = 20.0 * std::log10(p.wet / p.dry);
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double t = 2.0 * double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(l0 - 60.0 * t / rt), -80.0f, 0.0f);
        }
        curve(xs, ys, n, fPY + fPH);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "DRY");
        else
            std::snprintf(buf, n, "WET %d%%  DECAY %.1f s", int(std::floor(v * 0.45 + 0.5)),
                          nullwetter::rt60(nullwetter::paramsFor(v).fb));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIWetter)
};

UI* createUI()
{
    return new UIWetter();
}

END_NAMESPACE_DISTRHO
