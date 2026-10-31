/*
 * NULL JSFX - Air (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AirDSP.hpp"

START_NAMESPACE_DISTRHO

class UIAir : public NullOneSliderUI
{
public:
    UIAir()
        : NullOneSliderUI({ "AIR", "AMOUNT", "FLAT", "AIRY", 0.0f, 100.0f, 0.0f, false, false }) {}

protected:
    // the shelf's magnitude response, its corner dashed (as in air_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(12.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullair::Coeffs c = nullair::coeffsFor(value(), sr);
        dashedV(freqX(float(c.f)));

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullair::magnitudeDb(c, f, sr)), -2.0f, 10.0f);
        }
        curve(xs, ys, n, linY(0.0f, -2.0f, 10.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullair::Coeffs c = nullair::coeffsFor(v, sr);
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "FLAT");
        else
            std::snprintf(buf, n, "+%.1f dB ABOVE %.1f kHz", c.gdb, c.f / 1000.0);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIAir)
};

UI* createUI()
{
    return new UIAir();
}

END_NAMESPACE_DISTRHO
