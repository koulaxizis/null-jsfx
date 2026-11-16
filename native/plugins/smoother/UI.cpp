/*
 * NULL JSFX - Smoother (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SmootherDSP.hpp"

START_NAMESPACE_DISTRHO

class UISmoother : public NullOneSliderUI
{
public:
    UISmoother()
        : NullOneSliderUI({ "SMOOTHER", "AMOUNT", "OFF", "SMOOTH", 0.0f, 100.0f, 40.0f, false, false }) {}

protected:
    // as in smoother_gfx.jsfx: the four bands and the deepest they can cut together; they only
    // cut while a band pokes out of the mix
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(18.0f);
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        for (double f : nullsmoother::kFreq)
            dashedV(freqX(float(f)));
        const nullsmoother::Params p = nullsmoother::paramsFor(value());
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullsmoother::deepestDb(p, f, sr)), -12.0f, 12.0f);
        }
        curve(xs, ys, n, linY(0.0f, -12.0f, 12.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "4 BANDS  CUT UP TO %.1f dB", nullsmoother::paramsFor(v).max);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UISmoother)
};

UI* createUI()
{
    return new UISmoother();
}

END_NAMESPACE_DISTRHO
