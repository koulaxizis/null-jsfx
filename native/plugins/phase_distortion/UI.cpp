/*
 * NULL JSFX - Phase Distortion (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PhaseDistortionDSP.hpp"
#include "PhaseDistortionSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TONE", 0, -1, false, true },
    { 2, NullKnobsUI::kKnob, "BIAS", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "SPREAD", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "SOURCE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIPhaseDistortion : public NullKnobsUI
{
public:
    UIPhaseDistortion()
        : NullKnobsUI({ "PHASE DISTORTION", "ADVANCED", 480, 120, nullptr, 2, nullphasedist::kSliders,
                        nullphasedist::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in phase_distortion_gfx.jsfx: two cycles of a 440 Hz sine at -6 dB (dim) and what the
    // effect makes of it (Tone, Spread and Mix left out)
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 40.0f, fVH - 20.0f);
        grid(8, 2, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+1");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");
        const bool keyed = value(4) >= 0.5f;
        label(fPX, fPY + fPH + 10.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted,
              keyed ? "SINE IN, KEYED BY THE SIDECHAIN" : "SINE IN, MOVED BY ITSELF");

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        using nullphasedist::kSliders;
        const nullphasedist::Params p = nullphasedist::paramsFor(
            jsfxValue(kSliders[0], value(0)), jsfxValue(kSliders[1], value(1)), jsfxValue(kSliders[2], value(2)),
            jsfxValue(kSliders[3], value(3)), jsfxValue(kSliders[4], value(4)), jsfxValue(kSliders[5], value(5)),
            jsfxValue(kSliders[6], value(6)), sr);
        constexpr int n = 289;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double ph = 4.0 * M_PI * double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(std::sin(ph)), -1.0f, 1.0f);
        }
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kMuted, 0.6f));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        for (int i = 0; i < n; ++i)
        {
            const double ph = 4.0 * M_PI * double(i) / double(n - 1);
            ys[i] = linY(float(keyed ? std::sin(ph) : nullphasedist::shape(p, ph, sr)), -1.0f, 1.0f);
        }
        curve(xs, ys, n);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 6)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIPhaseDistortion();
}

END_NAMESPACE_DISTRHO
