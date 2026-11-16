/*
 * NULL JSFX - Echo One (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "EchoOneDSP.hpp"

START_NAMESPACE_DISTRHO

class UIEchoOne : public NullOneSliderUI
{
public:
    UIEchoOne()
        : NullOneSliderUI({ "ECHO ONE", "AMOUNT", "DRY", "LONG", 0.0f, 100.0f, 40.0f, false, false }) {}

protected:
    // as in echo_one_gfx.jsfx: the dry hit at 0 and the repeats over 4 beats (0 to -48 dB,
    // filters left out); with ping-pong the repeats alternate sides, the right ones dimmer
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, std::round(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, std::round(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX - 6.0f, std::round(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-48");
        const float capY = fPY + fPH + 11.0f;
        label(fPX + fPW * 0.25f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1 beat");
        label(fPX + fPW * 0.50f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "2 beats");
        label(fPX + fPW * 0.75f, capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "3 beats");

        const nullechoone::Params p = nullechoone::paramsFor(value(), 48000.0);
        const float bottom = fPY + fPH;
        const float y0 = linY(float(20.0 * std::log10(p.dry)), -48.0f, 0.0f);
        beginPath();
        rect(fPX, y0, 2.0f, bottom - y0);
        fillColor(col(theme::kAccent));
        fill();
        if (value() <= 0.0f)
            return;
        for (int k = 1; k <= 16; ++k)
        {
            const double db = 20.0 * std::log10(p.wet * std::pow(p.fb, double(k - 1)));
            if (k * p.note > 4.0 || db <= -48.0)
                continue;
            const float x = std::min(fPX + fPW * float(k * p.note / 4.0), fPX + fPW - 2.0f);
            const float y = linY(float(db), -48.0f, 0.0f);
            beginPath();
            rect(x, y, 2.0f, bottom - y);
            fillColor(col(theme::kAccent, p.pp > 0.5 && k % 2 == 0 ? 0.45f : 1.0f));
            fill();
        }
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
        {
            std::snprintf(buf, n, "DRY");
            return;
        }
        const nullechoone::Params p = nullechoone::paramsFor(v, 48000.0);
        std::snprintf(buf, n, "%s  FEEDBACK %d%%  PING-PONG %d%%", nullechoone::nameFor(v),
                      int(std::floor(p.fb * 100.0 + 0.5)), int(std::floor(p.pp * 100.0 + 0.5)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIEchoOne)
};

UI* createUI()
{
    return new UIEchoOne();
}

END_NAMESPACE_DISTRHO
