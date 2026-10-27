/*
 * NULL JSFX - Ladder Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "LadderFilterDSP.hpp"
#include "LadderFilterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "CUTOFF", 0, -1, false, true },
    { 1, NullKnobsUI::kKnob, "RESONANCE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "GAIN", 0, -1, true },
    { 5, NullKnobsUI::kSelect, "OVERSAMPLE", 0, -1, false },
};

class UILadderFilter : public NullKnobsUI
{
public:
    UILadderFilter()
        : NullKnobsUI({ "LADDER FILTER", "ADVANCED", 480, 120, nullptr, 1, nullladder::kSliders, nullladder::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in ladder_filter_gfx.jsfx: the small-signal response (the resonance peak at Cutoff),
    // with drive and Gain; the saturation is not drawn
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullladder::Coeffs c = nullladder::coeffs(sr, value(0), value(1), value(2), int(std::lround(value(3))),
                                                        value(4));
        dashedV(freqX(float(c.fc)));
        constexpr int n = 300;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullladder::responseDb(c, f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", v / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(v));
        }
        else if (param == 1)
            std::snprintf(buf, n, "%.1f", v);
        else if (param == 4)
            std::snprintf(buf, n, "%+.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UILadderFilter();
}

END_NAMESPACE_DISTRHO
