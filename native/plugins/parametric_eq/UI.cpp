/*
 * NULL JSFX - Parametric EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ParametricEqDSP.hpp"
#include "ParametricEqSliders.hpp"

START_NAMESPACE_DISTRHO

#define NULL_PEQ_CONTROLS(b)                                        \
    { b * 5 + 0, NullKnobsUI::kKnob, "FREQ", 0, b, false },         \
    { b * 5 + 1, NullKnobsUI::kKnob, "GAIN", 0, b, true },          \
    { b * 5 + 2, NullKnobsUI::kKnob, "Q", 0, b, false },            \
    { b * 5 + 3, NullKnobsUI::kSelect, "TYPE", 0, b, false },       \
    { b * 5 + 4, NullKnobsUI::kSelect, "MODE", 0, b, false }

// one tab per band, as in parametric_eq_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    NULL_PEQ_CONTROLS(0), NULL_PEQ_CONTROLS(1), NULL_PEQ_CONTROLS(2), NULL_PEQ_CONTROLS(3), NULL_PEQ_CONTROLS(4),
};

#undef NULL_PEQ_CONTROLS

class UIParametricEq : public NullKnobsUI
{
public:
    UIParametricEq()
        : NullKnobsUI({ "PARAMETRIC EQ", "ESSENTIALS", 480, 140, "LOW|LOW MID|MID HIGH|HIGH|ULTRA HIGH", 1,
                        nullpeq::kSliders, nullpeq::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in parametric_eq_gfx.jsfx: the response of the bands the audio runs (-24..+24 dB), the
    // selected band dashed, a dot per band (filled = selected, hollow = bypassed or not soloed)
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nullpeq::BandParams p[nullpeq::kBands];
        nullpeq::bandParams([this](uint32_t i) { return double(value(i)); }, p);
        nullpeq::EqSetup eq;
        eq.compute(p, sr);

        const int sel = page();
        dashedV(freqX(float(p[sel].freq)));

        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(eq.responseDb(f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));

        for (int b = 0; b < nullpeq::kBands; ++b)
        {
            const double f = std::min(20000.0, p[b].freq);
            const float x = freqX(float(f)), y = linY(float(eq.responseDb(f, sr)), -24.0f, 24.0f);
            beginPath();
            if (b == sel)
            {
                circle(x, y, 4.0f);
                fillColor(col(theme::kAccent));
                fill();
            }
            else if (eq.active[b])
            {
                circle(x, y, 3.0f);
                fillColor(col(theme::kAccent));
                fill();
            }
            else
            {
                circle(x, y, 3.0f);
                strokeColor(col(theme::kMuted));
                strokeWidth(1.0f);
                stroke();
            }
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        switch (param % 5)
        {
        case 0:
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
            break;
        case 1: std::snprintf(buf, n, "%+.1f dB", double(v)); break;
        case 2: std::snprintf(buf, n, "%.1f", double(v)); break;
        default: NullKnobsUI::formatValue(param, v, buf, n); break;
        }
    }
};

UI* createUI()
{
    return new UIParametricEq();
}

END_NAMESPACE_DISTRHO
