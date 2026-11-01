/*
 * NULL JSFX - Speaker Sim (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpeakerSimDSP.hpp"
#include "SpeakerSimSliders.hpp"

START_NAMESPACE_DISTRHO

// one row, as in speaker_sim_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "DEVICE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UISpeakerSim : public NullKnobsUI
{
public:
    UISpeakerSim()
        : NullKnobsUI({ "SPEAKER SIM", "ESSENTIALS", 480, 120, nullptr, 1, nullspeakersim::kUiSliders,
                        nullspeakersim::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in speaker_sim_gfx.jsfx: the device's response from 20 Hz to 20 kHz, its name and
    // whether it is mono
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullspeakersim::Params p
            = nullspeakersim::paramsFrom([this](uint32_t i) { return nullspeakersim::sliderValue(i, value(i)); });
        const nullspeakersim::Coeffs c = nullspeakersim::coeffsFor(p, sr);
        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ys[512];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + float(i);
            ys[i] = linY(float(nullspeakersim::magnitudeDb(c, 20.0 * std::pow(1000.0, double(i) / double(fPW)), sr)),
                         -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(-24.0f, -24.0f, 24.0f));
        const int d = std::min(nullspeakersim::kDevices - 1, std::max(0, p.device));
        label(fPX + fPW - 4.0f, fPY + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kAccent, nullspeakersim::kUi0[d], true);
        label(fPX + fPW - 4.0f, fPY + 20.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, c.mono ? "MONO" : "STEREO", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullspeakersim::sliderValue(param, v);
        if (param == 1 || param == 2)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 3)
            std::snprintf(buf, n, "%+.1f dB", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISpeakerSim();
}

END_NAMESPACE_DISTRHO
