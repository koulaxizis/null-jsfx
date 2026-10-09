/*
 * NULL JSFX - Spread (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpreadDSP.hpp"

START_NAMESPACE_DISTRHO

class UISpread : public NullOneSliderUI
{
public:
    UISpread()
        : NullOneSliderUI({ "SPREAD", "AMOUNT", "OFF", "WIDE", 0.0f, 100.0f, 50.0f, false, true }) {}

protected:
    // vectorscope of the output with mid and side level meters (as in spread_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(34.0f, 66.0f, 86.0f, 86.0f);
        vectorscope(2.0f);
        nulljsfx::Taps* t = taps();
        const float mid = t ? t->inLevel.load(std::memory_order_relaxed) : 0.0f;
        const float side = t ? t->outLevel.load(std::memory_order_relaxed) : 0.0f;
        meter(140.0f, 86.0f, 186.0f, 8.0f, "MID", nulljsfx::toDb(mid), -140.0f, -60.0f);
        meter(140.0f, 108.0f, 186.0f, 8.0f, "SIDE", nulljsfx::toDb(side), -140.0f, -60.0f);
        label(170.0f, 131.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "-60");
        label(248.0f, 131.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "-30");
        label(326.0f, 131.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0 dB");
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "WIDTH +%d%%  ABOVE 250 Hz", int(std::floor(v * 0.7 + 0.5)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UISpread)
};

UI* createUI()
{
    return new UISpread();
}

END_NAMESPACE_DISTRHO
