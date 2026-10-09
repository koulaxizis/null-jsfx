/*
 * NULL JSFX - Doubler (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DoublerDSP.hpp"

START_NAMESPACE_DISTRHO

class UIDoubler : public NullOneSliderUI
{
public:
    UIDoubler()
        : NullOneSliderUI({ "DOUBLER", "AMOUNT", "DRY", "DOUBLE", 0.0f, 100.0f, 40.0f, false, false }) {}

protected:
    // echogram, left channel up and right channel down: the dry signal at 0 ms and each double
    // at its average delay, the shaded band showing its slow drift (as in doubler_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(8, 2, false);
        const float cy = std::floor(fPY + fPH * 0.5f) + 0.5f;
        beginPath();
        moveTo(fPX, cy);
        lineTo(fPX + fPW, cy);
        strokeColor(col(theme::kTrackHover));
        strokeWidth(1.0f);
        stroke();
        label(fPX - 6.0f, fPY + fPH * 0.25f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "L");
        label(fPX - 6.0f, fPY + fPH * 0.75f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "R");
        for (int k = 1; k <= 3; ++k)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d ms", k * 10);
            label(linX(k * 10.0f, 0.0f, 40.0f), fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }

        const nulldoubler::Params p = nulldoubler::paramsForAmount(value());
        const float a = float(p.amount);
        bar(14.0f + a * 6.0f, 2.0f, float(p.level * p.dry), -1);
        bar(21.0f + a * 8.0f, 2.5f, float(p.level * p.dry), 1);
        bar(0.0f, 0.0f, float(p.dry), 0);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const double a = v / 100.0;
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "L %.1f ms  R %.1f ms", 14.0 + a * 6.0, 21.0 + a * 8.0);
    }

private:
    // one echogram bar at ms (0..40), height gain, +-drift band; dir -1 up, 1 down, 0 both
    void bar(float ms, float drift, float gain, int dir)
    {
        const float cy = fPY + fPH * 0.5f;
        const float hh = gain * fPH * 0.5f * 0.9f;
        if (!(hh > 0.0f))
            return;
        if (drift > 0.0f)
        {
            const float x0 = linX(ms - drift, 0.0f, 40.0f), x1 = linX(ms + drift, 0.0f, 40.0f);
            beginPath();
            rect(x0, dir < 0 ? cy - hh : cy, x1 - x0, hh);
            fillColor(col(theme::kAccent, 0.12f));
            fill();
        }
        const float x = std::fmax(fPX, linX(ms, 0.0f, 40.0f) - 1.5f);
        beginPath();
        if (dir <= 0)
            rect(x, cy - hh, 3.0f, hh);
        if (dir >= 0)
            rect(x, cy, 3.0f, hh);
        fillColor(col(theme::kAccent));
        fill();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIDoubler)
};

UI* createUI()
{
    return new UIDoubler();
}

END_NAMESPACE_DISTRHO
