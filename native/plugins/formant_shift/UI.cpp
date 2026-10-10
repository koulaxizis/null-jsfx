/*
 * NULL JSFX - Formant Shift (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "FormantShiftDSP.hpp"

START_NAMESPACE_DISTRHO

class UIFormantShift : public NullOneSliderUI
{
public:
    UIFormantShift()
        : NullOneSliderUI({ "FORMANT SHIFT", "FORMANT", "DEEP", "HIGH", 0.0f, 100.0f, 50.0f, true, false }) {}

protected:
    double sampleRate() const
    {
        const double sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    // the magnitude response of the dry + formant blend, the three formants dashed
    // (as in formant_shift_gfx.jsfx)
    void drawDisplay() override
    {
        const double sr = sampleRate();
        const nullformant::Coeffs c = nullformant::coeffsFor(value(), sr);
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(12.0f);
        for (int i = 0; i < 3; ++i)
            dashedV(freqX(float(c.cf[i])));

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullformant::magnitudeDb(f, value(), sr)), -8.0f, 8.0f);
        }
        curve(xs, ys, n, linY(0.0f, -8.0f, 8.0f));
    }

    void formatValue(char* buf, size_t n, float v) override
    {
        std::snprintf(buf, n, "%d", int(std::lround(v)));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const nullformant::Coeffs c = nullformant::coeffsFor(v, sampleRate());
        std::snprintf(buf, n, "SHIFT x%.2f  WET %d%%", c.shift, int(std::floor(c.wet * 100.0 + 0.5)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIFormantShift)
};

UI* createUI()
{
    return new UIFormantShift();
}

END_NAMESPACE_DISTRHO
