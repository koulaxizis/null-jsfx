/*
 * NULL JSFX - Sweeper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SweeperDSP.hpp"

START_NAMESPACE_DISTRHO

class UISweeper : public NullOneSliderUI
{
public:
    UISweeper()
        : NullOneSliderUI({ "SWEEPER", "AMOUNT", "LOW", "HIGH", 0.0f, 100.0f, 50.0f, true, false }) {}

protected:
    double sampleRate() const
    {
        const double sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    // the filter's magnitude response, cutoff dashed (as in sweeper_gfx.jsfx)
    void drawDisplay() override
    {
        const double sr = sampleRate();
        const nullsweeper::Coeffs c = nullsweeper::coeffsFor(value(), sr);
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(24.0f);
        if (c.mode != 0)
            dashedV(freqX(float(c.fc)));

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullsweeper::magnitudeDb(f, value(), sr)), -16.0f, 16.0f);
        }
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));
    }

    void formatValue(char* buf, size_t n, float v) override
    {
        std::snprintf(buf, n, "%d", int(std::lround(v)));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const nullsweeper::Coeffs c = nullsweeper::coeffsFor(v, sampleRate());
        const char* type = c.mode < 0 ? "LOW-PASS" : "HIGH-PASS";
        if (c.mode == 0)
            std::snprintf(buf, n, "OPEN");
        else if (c.fc >= 1000.0)
            std::snprintf(buf, n, "%s %.1f kHz  Q %.2f", type, c.fc / 1000.0, c.q);
        else
            std::snprintf(buf, n, "%s %.0f Hz  Q %.2f", type, c.fc, c.q);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UISweeper)
};

UI* createUI()
{
    return new UISweeper();
}

END_NAMESPACE_DISTRHO
