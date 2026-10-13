/*
 * NULL JSFX - Amp Simulator (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AmpSimulatorDSP.hpp"
#include "AmpSimulatorSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODEL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "GAIN", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BASS", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MID", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "TREBLE", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "PRESENCE", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "MASTER", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "BRIGHT", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "SAG", 1, -1, false },
    { 9, NullKnobsUI::kSelect, "OVERSAMPLE", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIAmpSimulator : public NullKnobsUI
{
public:
    UIAmpSimulator()
        : NullKnobsUI({ "AMP SIMULATOR", "ADVANCED", 480, 140, nullptr, 2, nullamp::kSliders, nullamp::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in amp_simulator_gfx.jsfx: left the tone shaping (tone stack, resonance, presence, Bright)
    // from -16 to +16 dB with the model at the top right; right the preamp's transfer curve with
    // the input peak as a dot and the Gain in dB below
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        using nullamp::snap;
        const int model = std::max(0, std::min(3, int(std::lround(value(0)))));
        nullamp::Setup s;
        s.compute(model, snap(value(1), 0.0), snap(value(2), 0.0), snap(value(3), 0.0), snap(value(4), 0.0),
                  snap(value(5), 0.0), snap(value(6), 0.0), value(7) >= 0.5f, value(8), value(9) >= 0.5f,
                  snap(value(10), -24.0), sr);

        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 158.0f, fVH - 20.0f);
        gridFreq(24.0f);
        constexpr int n = 280;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(s.responseDb(f, sr)), -16.0f, 16.0f);
        }
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));
        label(fPX + fPW - 6.0f, fPY + 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, nullamp::kModelNames[model]);

        setPlot(fVX + fVW - 104.0f, fVY + 4.0f, 104.0f, fVH - 20.0f);
        grid(4, 4, true);
        const double sg = s.ns == 3 ? -1.0 : 1.0;
        const double nm = std::max(std::fabs(s.xfer(1.0)), std::fabs(s.xfer(-1.0)));
        constexpr int m = 105;
        float tx[m], ty[m];
        for (int i = 0; i < m; ++i)
        {
            const double x = double(i) / double(m - 1) * 2.0 - 1.0;
            tx[i] = fPX + fPW * float(i) / float(m - 1);
            ty[i] = fPY + fPH * float(0.5 - 0.5 * sg * s.xfer(x) / nm);
        }
        curve(tx, ty, m);
        double pk = 0.0;
        if (nulljsfx::Taps* t = taps())
            pk = std::min(1.0, double(t->inLevel.load(std::memory_order_relaxed)));
        beginPath();
        circle(fPX + fPW * float(pk + 1.0) * 0.5f, fPY + fPH * float(0.5 - 0.5 * sg * s.xfer(pk) / nm), 3.0f);
        fillColor(col(theme::kText));
        fill();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "DRIVE %+.0f dB", s.preDb);
        label(fPX + fPW * 0.5f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param >= 1 && param <= 6)
            std::snprintf(buf, n, "%.1f", double(v));
        else if (param == 10)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIAmpSimulator();
}

END_NAMESPACE_DISTRHO
