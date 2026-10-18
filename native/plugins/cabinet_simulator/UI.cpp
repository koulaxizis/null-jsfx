/*
 * NULL JSFX - Cabinet Simulator (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "CabinetSimulatorDSP.hpp"
#include "CabinetSimulatorSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "CABINET", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "MIC POS", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "MIC 2 POS", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "MIC BLEND", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "LOW CUT", 1, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UICabinetSimulator : public NullKnobsUI
{
public:
    UICabinetSimulator()
        : NullKnobsUI({ "CABINET SIMULATOR", "ESSENTIALS", 480, 120, nullptr, 2, nullcab::kSliders, nullcab::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in cabinet_simulator_gfx.jsfx: the cabinet's magnitude response with Mix and Output
    // (-24..+24 dB), the low cut dashed, the cabinet's name at the top right
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const int cab = std::max(0, std::min(7, int(std::lround(value(0)))));
        nullcab::Setup s;
        s.compute(cab, value(1), value(2), value(3), value(4), sr, value(5), value(6));
        dashedV(freqX(float(s.lowCut)));

        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(s.responseDb(f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
        label(fPX + fPW - 6.0f, fPY + 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, nullcab::kCabNames[cab]);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UICabinetSimulator();
}

END_NAMESPACE_DISTRHO
