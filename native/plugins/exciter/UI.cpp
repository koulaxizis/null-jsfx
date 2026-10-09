/*
 * NULL JSFX - Exciter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ExciterDSP.hpp"

START_NAMESPACE_DISTRHO

class UIExciter : public NullOneSliderUI
{
public:
    UIExciter()
        : NullOneSliderUI({ "EXCITER", "AMOUNT", "CLEAN", "EXCITED", 0.0f, 100.0f, 30.0f, false, false }) {}

protected:
    // transfer curve of the harmonic saturator for the high band at the current drive,
    // the straight line dimmed (as in exciter_gfx.jsfx)
    void drawDisplay() override
    {
        const nullexciter::Coeffs c = nullexciter::coeffsFor(value(), 48000.0);
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(8, 4, true);

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.3f));
        strokeWidth(1.0f);
        stroke();

        const float ly = fPY + fPH + 11.0f;
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+1");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");
        label(fPX, ly, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "-1");
        label(fPX + fPW * 0.5f, ly, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "IN");
        label(fPX + fPW, ly, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "+1");

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double x = -1.0 + 2.0 * double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullexciter::shape(x * c.drive)), -1.0f, 1.0f);
        }
        curve(xs, ys, n, linY(0.0f, -1.0f, 1.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const nullexciter::Coeffs c = nullexciter::coeffsFor(v, 48000.0);
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "DRIVE x%.1f  MIX %d%%", c.drive, int(std::floor(c.blend * 100.0 + 0.5)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIExciter)
};

UI* createUI()
{
    return new UIExciter();
}

END_NAMESPACE_DISTRHO
