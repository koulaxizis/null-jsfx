/*
 * NULL JSFX - Tilt (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TiltDSP.hpp"

START_NAMESPACE_DISTRHO

class UITilt : public NullOneSliderUI
{
public:
    UITilt()
        : NullOneSliderUI({ "TILT", "AMOUNT", "DARK", "BRIGHT", -100.0f, 100.0f, 0.0f, true, false }) {}

protected:
    // the filter's magnitude response, 700 Hz pivot dashed (as in tilt_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(12.0f);
        dashedV(freqX(float(nulltilt::kPivotHz)));

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nulltilt::magnitudeDb(std::fmin(f, sr * 0.4999), value(), sr)), -8.0f, 8.0f);
        }
        curve(xs, ys, n, linY(0.0f, -8.0f, 8.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const double hi = v / 100.0 * nulltilt::kMaxDb;
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "FLAT");
        else
            std::snprintf(buf, n, "LOW %+.1f  HIGH %+.1f dB", -hi, hi);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UITilt)
};

UI* createUI()
{
    return new UITilt();
}

END_NAMESPACE_DISTRHO
