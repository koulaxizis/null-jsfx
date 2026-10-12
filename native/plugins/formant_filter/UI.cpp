/*
 * NULL JSFX - Formant Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "FormantFilterDSP.hpp"
#include "FormantFilterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "VOWEL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "GAIN", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "Q", 0, -1, false },
};

class UIFormantFilter : public NullKnobsUI
{
public:
    UIFormantFilter()
        : NullKnobsUI({ "FORMANT FILTER", "ADVANCED", 480, 120, nullptr, 1, nullformant::kSliders,
                        nullformant::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in formant_filter_gfx.jsfx: the response of the three band-passes for the selected
    // vowel, with Gain; F1..F3 dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);
        const int vowel = int(std::lround(value(0)));
        double t[3];
        nullformant::vowelFormants(vowel, t[0], t[1], t[2]);
        char buf[8];
        for (int k = 0; k < 3; ++k)
        {
            dashedV(freqX(float(t[k])));
            std::snprintf(buf, sizeof(buf), "F%d", k + 1);
            label(freqX(float(t[k])) + 4.0f, fPY + 7.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        constexpr int n = 200;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullformant::responseDb(f, vowel, value(2), value(1), sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%+.1f dB", v);
        else
            std::snprintf(buf, n, "%d", int(std::lround(v)));
    }
};

UI* createUI()
{
    return new UIFormantFilter();
}

END_NAMESPACE_DISTRHO
