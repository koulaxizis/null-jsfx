/*
 * NULL JSFX - Spectral Freeze (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpectralFreezeDSP.hpp"
#include "SpectralFreezeSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "FREEZE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "FADE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SMEAR", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "LEVEL", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UISpectralFreeze : public NullKnobsUI
{
public:
    UISpectralFreeze()
        : NullKnobsUI({ "SPECTRAL FREEZE", "ADVANCED", 480, 120, nullptr, 1, nullfreeze::kSliders,
                        nullfreeze::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in spectral_freeze_gfx.jsfx: the held spectrum (0 dB = a full-scale sine, to -84 dB)
    // on a log-frequency axis, silver while frozen, grey while it follows the input
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

        nulljsfx::Taps* t = taps();
        const bool frozen = t && t->extra.load(std::memory_order_relaxed) > 0.5f;
        char buf[32];
        if (frozen)
            std::snprintf(buf, sizeof(buf), "FROZEN %d%%",
                          int(std::floor(t->gr.load(std::memory_order_relaxed) * 100.0f + 0.5f)));
        else
            std::snprintf(buf, sizeof(buf), "LIVE");
        label(fPX + fPW, fVY + 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        if (!t)
            return;

        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        const int nb = nullfreeze::kNB, n = nullfreeze::kN;
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
        strokeColor(col(frozen ? theme::kAccent : theme::kMuted));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
    }
};

UI* createUI()
{
    return new UISpectralFreeze();
}

END_NAMESPACE_DISTRHO
