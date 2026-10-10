/*
 * NULL JSFX - Phatter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PhatterDSP.hpp"

START_NAMESPACE_DISTRHO

class UIPhatter : public NullOneSliderUI
{
public:
    UIPhatter()
        : NullOneSliderUI({ "PHATTER", "AMOUNT", "FLAT", "PHAT", 0.0f, 100.0f, 50.0f, false, false }) {}

protected:
    // the low shelf's magnitude response, 200 Hz corner dashed (as in phatter_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(20.0f);
        dashedV(freqX(float(nullphatter::kCornerHz)));

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullphatter::magnitudeDb(f, value(), sr)), -40.0f / 3.0f, 40.0f / 3.0f);
        }
        curve(xs, ys, n, linY(0.0f, -40.0f / 3.0f, 40.0f / 3.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "FLAT");
        else
            std::snprintf(buf, n, "BASS +%.1f dB", v * 0.12);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIPhatter)
};

UI* createUI()
{
    return new UIPhatter();
}

END_NAMESPACE_DISTRHO
