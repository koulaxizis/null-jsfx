/*
 * NULL JSFX - Noise Generator (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "NoiseGeneratorDSP.hpp"
#include "NoiseGeneratorSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "TYPE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "LEVEL", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "WIDTH", 0, -1, false },
};

class UINoiseGenerator : public NullKnobsUI
{
public:
    UINoiseGenerator()
        : NullKnobsUI({ "NOISE GENERATOR", "ESSENTIALS", 480, 120, nullptr, 1, nullnoise::kSliders, nullnoise::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in noise_generator_gfx.jsfx: the spectrum's slope over 20 Hz..20 kHz, level in dB
    // (0 to -72), pivoting on Level at 1 kHz: white flat, pink -3 dB/oct, brown -6 dB/oct
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * d;
                    if (f < 20.0f || f > 20000.0f || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::floor(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        beginPath();
        for (int i = 0; i <= 3; ++i)
        {
            const float y = std::floor(fPY + fPH * float(i) / 3.0f) + 0.5f;
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
        }
        strokeColor(col(theme::kBorder, 0.6f));
        stroke();
        static const char* const kDb[] = { "0", "-24", "-48", "-72" };
        for (int i = 0; i <= 3; ++i)
            label(fPX - 6.0f, fPY + fPH * float(i) / 3.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, kDb[i]);
        const float capY = fPY + fPH + 10.0f;
        label(freqX(100.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");

        const int type = int(std::lround(value(0)));
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted,
              type == 0 ? "FLAT" : (type == 1 ? "-3 dB / OCT" : "-6 dB / OCT"));

        const float level = value(1);
        constexpr int n = 97;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const float t = float(i) / float(n - 1);
            const float f = 20.0f * std::pow(1000.0f, t);
            xs[i] = fPX + fPW * t;
            ys[i] = linY(level - float(type) * 3.0f * std::log2(f / 1000.0f), -72.0f, 0.0f);
        }
        curve(xs, ys, n, fPY + fPH);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UINoiseGenerator();
}

END_NAMESPACE_DISTRHO
