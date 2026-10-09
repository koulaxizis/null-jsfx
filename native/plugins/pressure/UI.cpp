/*
 * NULL JSFX - Pressure (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PressureDSP.hpp"

START_NAMESPACE_DISTRHO

class UIPressure : public NullOneSliderUI
{
public:
    UIPressure()
        : NullOneSliderUI({ "PRESSURE", "AMOUNT", "LIGHT", "HEAVY", 0.0f, 100.0f, 50.0f, false, true }) {}

protected:
    // as in pressure_gfx.jsfx: left, the static curve (input -60..0 dB across, output -60..0 dB
    // up) with the detector level as a dot; right, input, gain-reduction and output meters
    void drawDisplay() override
    {
        setPlot(54.0f, 66.0f, 84.0f, 80.0f);
        grid(4, 4, false);
        for (int i = 0; i < 3; ++i)
        {
            const float y = std::floor(fPY + fPH * float(i) / 2.0f) + 0.5f;
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", -30 * i);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        const nullpressure::Params p = nullpressure::paramsFor(value());
        constexpr int n = 169;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double d = -60.0 + 60.0 * double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullpressure::curveDb(p, d)), -60.0f, 0.0f);
        }
        curve(xs, ys, n);

        float in = 0.0f, out = 0.0f, gr = 0.0f, db = -100.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
            db = t->extra.load(std::memory_order_relaxed);
        }
        if (db > -60.0f)
        {
            const double d = std::fmin(0.0, double(db));
            beginPath();
            circle(linX(float(d), -60.0f, 0.0f), linY(float(nullpressure::curveDb(p, d)), -60.0f, 0.0f), 2.5f);
            fillColor(col(theme::kText));
            fill();
        }
        meter(160.0f, 77.0f, 168.0f, 10.0f, "IN", nulljsfx::toDb(in), -140.0f, -60.0f);
        grMeter(160.0f, 101.0f, 168.0f, 10.0f, "GR", gr, 24.0f);
        meter(160.0f, 125.0f, 168.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const double s = v;
        if (std::fabs(s) < 0.00001)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "THR %.1f dB  RATIO %.2f:1", s * -0.3, 1 + s * 0.05);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIPressure)
};

UI* createUI()
{
    return new UIPressure();
}

END_NAMESPACE_DISTRHO
