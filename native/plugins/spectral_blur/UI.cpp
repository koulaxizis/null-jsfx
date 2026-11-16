/*
 * NULL JSFX - Spectral Blur (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpectralBlurDSP.hpp"
#include "SpectralBlurSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SMEAR", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SHIFT", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "TILT", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "GATE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UISpectralBlur : public NullKnobsUI
{
public:
    UISpectralBlur()
        : NullKnobsUI({ "SPECTRAL BLUR", "ADVANCED", 480, 120, nullptr, 1, nullblur::kSliders, nullblur::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in spectral_blur_gfx.jsfx: the blurred spectrum (0 dB = a full-scale sine, to -84 dB)
    // on a log-frequency axis, with the Gate threshold as a line while it is on
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(1, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-42");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-84");
        label(freqX(100.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");

        const float gate = value(3);
        if (gate > -90.0f)
        {
            const float gy = linY(gate, -84.0f, 0.0f);
            beginPath();
            moveTo(fPX, gy);
            lineTo(fPX + fPW, gy);
            strokeColor(col(theme::kMuted, 0.6f));
            strokeWidth(1.0f);
            stroke();
        }

        nulljsfx::Taps* t = taps();
        if (!t)
            return;
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        const int nb = nullblur::kNB, n = nullblur::kN;
        beginPath();
        for (int j = 0; j <= 200; ++j)
        {
            const int b0 = std::min(nb - 1, int(std::floor(20.0 * std::pow(1000.0, j / 200.0) * n / sr)));
            const int b1 = std::max(b0, std::min(nb - 1, int(std::floor(20.0 * std::pow(1000.0, (j + 1) / 200.0) * n / sr))));
            float m = 0.0f;
            for (int b = b0; b <= b1; ++b)
                m = std::fmax(m, t->scopeL[b]);
            const float y = linY(20.0f * std::log10(std::fmax(m / (n / 4.0f), 0.0000001f)), -84.0f, 0.0f);
            const float x = fPX + fPW * float(j) / 200.0f;
            if (j == 0)
                moveTo(x, y);
            else
                lineTo(x, y);
        }
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
    }
};

UI* createUI()
{
    return new UISpectralBlur();
}

END_NAMESPACE_DISTRHO
