/*
 * NULL JSFX - DJ Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DjFilterDSP.hpp"

START_NAMESPACE_DISTRHO

class UIDjFilter : public NullOneSliderUI
{
public:
    UIDjFilter()
        : NullOneSliderUI({ "DJ FILTER", "FILTER", "LO PASS", "HI PASS", -100.0f, 100.0f, 0.0f, true, false }) {}

protected:
    // the filter's magnitude response at the knob position, cutoff dashed (as in dj_filter_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(36.0f);
        const double sr = rate();
        const double p = value() / 100.0;
        if (p != 0.0)
            dashedV(freqX(float(nulldjfilter::cutoff(p, sr))));

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nulldjfilter::responseDb(f, p, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const double f = std::floor(nulldjfilter::cutoff(v / 100.0, rate()) + 0.5);
        char fq[32];
        if (f >= 1000.0)
            std::snprintf(fq, sizeof(fq), "%.2f kHz", f / 1000.0);
        else
            std::snprintf(fq, sizeof(fq), "%d Hz", int(f));
        if (v == 0.0f)
            std::snprintf(buf, n, "DRY");
        else
            std::snprintf(buf, n, "%s %s", v < 0.0f ? "LOW PASS" : "HIGH PASS", fq);
    }

private:
    double rate() const
    {
        const double sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIDjFilter)
};

UI* createUI()
{
    return new UIDjFilter();
}

END_NAMESPACE_DISTRHO
