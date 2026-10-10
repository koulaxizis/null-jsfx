/*
 * NULL JSFX - Mono Maker (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "MonoMakerDSP.hpp"

START_NAMESPACE_DISTRHO

class UIMonoMaker : public NullOneSliderUI
{
public:
    UIMonoMaker()
        : NullOneSliderUI({ "MONO MAKER", "AMOUNT", "OFF", "400 HZ", 0.0f, 100.0f, 50.0f, false, false }) {}

protected:
    // the side (L-R) response, mid untouched at 0 dB, crossover dashed (as in mono_maker_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(24.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullmonomaker::Coeffs c = nullmonomaker::coeffsFor(value(), sr);
        if (c.amount > 0.0)
            dashedV(freqX(float(c.fc)));

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullmonomaker::sideDb(c, f, sr)), -16.0f, 16.0f);
        }
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (v > 0.0f)
            std::snprintf(buf, n, "SIDE MONO BELOW %d Hz", int(std::floor(20.0 * std::pow(20.0, v / 100.0) + 0.5)));
        else
            std::snprintf(buf, n, "OFF");
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIMonoMaker)
};

UI* createUI()
{
    return new UIMonoMaker();
}

END_NAMESPACE_DISTRHO
