/*
 * NULL JSFX - Passive EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PassiveEqDSP.hpp"
#include "PassiveEqSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in passive_eq_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LOW FREQ", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "LOW BOOST", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "LOW ATTEN", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "HIGH FREQ", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "HIGH BOOST", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "BANDWIDTH", 0, -1, false },
    { 6, NullKnobsUI::kSelect, "ATTEN FREQ", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "HIGH ATTEN", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "TUBE", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIPassiveEq : public NullKnobsUI
{
public:
    UIPassiveEq()
        : NullKnobsUI({ "PASSIVE EQ", "ESSENTIALS", 480, 120, nullptr, 2, nullpassiveeq::kUiSliders,
                        nullpassiveeq::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in passive_eq_gfx.jsfx: the response from 20 Hz to 20 kHz (Output included), the three
    // frequencies dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullpassiveeq::Params p
            = nullpassiveeq::paramsFrom([this](uint32_t i) { return nullpassiveeq::sliderValue(i, value(i)); });
        const nullpassiveeq::Coeffs c = nullpassiveeq::coeffsFor(p, sr);
        dashedV(freqX(float(nullpassiveeq::lowFreqHz(p.lowFreq))));
        dashedV(freqX(float(nullpassiveeq::highFreqHz(p.highFreq))));
        dashedV(freqX(float(nullpassiveeq::attenFreqHz(p.attenFreq))));

        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ys[512];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + float(i);
            ys[i] = linY(float(nullpassiveeq::magnitudeDb(c, 20.0 * std::pow(1000.0, double(i) / double(fPW)), sr)),
                         -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullpassiveeq::sliderValue(param, v);
        if (param == 8)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 9)
            std::snprintf(buf, n, "%+.1f dB", x);
        else if (param == 1 || param == 2 || param == 4 || param == 5 || param == 7)
            std::snprintf(buf, n, "%.1f", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIPassiveEq();
}

END_NAMESPACE_DISTRHO
