/*
 * NULL JSFX - Tone (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ToneDSP.hpp"
#include "ToneSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "BASS", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "TREBLE", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "BASS FREQ", 0, -1, false, true },
    { 3, NullKnobsUI::kKnob, "TREBLE FREQ", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UITone : public NullKnobsUI
{
public:
    UITone()
        : NullKnobsUI({ "TONE", "ESSENTIALS", 480, 120, nullptr, 1, nulltone::kSliders, nulltone::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in tone_gfx.jsfx: the response from 20 Hz to 20 kHz (Output included), both corners dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(24.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nulltone::Params p;
        p.bass = nulltone::sliderValue(0, value(0));
        p.treble = nulltone::sliderValue(1, value(1));
        p.bassFreq = nulltone::sliderValue(2, value(2));
        p.trebleFreq = nulltone::sliderValue(3, value(3));
        p.output = nulltone::sliderValue(4, value(4));
        const nulltone::Coeffs c = nulltone::coeffsFor(p, sr);
        dashedV(freqX(float(p.bassFreq)));
        dashedV(freqX(float(p.trebleFreq)));

        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ys[512];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + float(i);
            ys[i] = linY(float(nulltone::magnitudeDb(c, 20.0 * std::pow(1000.0, double(i) / double(fPW)), sr)),
                         -16.0f, 16.0f);
        }
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nulltone::sliderValue(param, v);
        if (param == 2)
            std::snprintf(buf, n, "%d Hz", int(x));
        else if (param == 3)
            std::snprintf(buf, n, "%.1f kHz", x / 1000.0);
        else
            std::snprintf(buf, n, "%+.1f dB", x);
    }
};

UI* createUI()
{
    return new UITone();
}

END_NAMESPACE_DISTRHO
