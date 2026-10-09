/*
 * NULL JSFX - Crunch (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "CrunchDSP.hpp"

START_NAMESPACE_DISTRHO

class UICrunch : public NullOneSliderUI
{
public:
    UICrunch()
        : NullOneSliderUI({ "CRUNCH", "AMOUNT", "CLEAN", "HARSH", 0.0f, 100.0f, 50.0f, false, false }) {}

protected:
    // the static transfer curve, input -1..+1 across, output -1..+1 up (as in crunch_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(8, 4, true);
        for (int i = 0; i < 3; ++i)
        {
            const float y = std::floor(fPY + fPH * float(i) / 2.0f) + 0.5f;
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, i == 0 ? "+1" : (i == 1 ? "0" : "-1"));
        }
        label(fPX + fPW * 0.5f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "IN");

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        const nullcrunch::Params p = nullcrunch::paramsFor(value());
        constexpr int n = 569;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double x = -1.0 + 2.0 * double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullcrunch::curve(p, x)), -1.0f, 1.0f);
        }
        curve(xs, ys, n, linY(0.0f, -1.0f, 1.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const double s = v;
        if (std::fabs(s) < 0.00001)
            std::snprintf(buf, n, "CLEAN");
        else
            std::snprintf(buf, n, "DRIVE %.1fx  KNEE %.1f", 1 + s * 0.19, 2 + s * 0.08);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UICrunch)
};

UI* createUI()
{
    return new UICrunch();
}

END_NAMESPACE_DISTRHO
