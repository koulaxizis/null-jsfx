/*
 * NULL JSFX - Noise Generator (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "NoiseGeneratorDSP.hpp"
#include "NoiseGeneratorSliders.hpp"

START_NAMESPACE_DISTRHO

// as noise_generator_gfx.jsfx: type, level, width, mix mode / low cut, high cut, follow, ducking
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "TYPE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "LEVEL", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "WIDTH", 0, -1, false },
    { 7, NullKnobsUI::kSelect, "MIX MODE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "LOW CUT", 1, -1, false, true },
    { 4, NullKnobsUI::kKnob, "HIGH CUT", 1, -1, false, true },
    { 5, NullKnobsUI::kKnob, "FOLLOW", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "DUCKING", 1, -1, false },
};

class UINoiseGenerator : public NullKnobsUI
{
public:
    UINoiseGenerator()
        : NullKnobsUI({ "NOISE GENERATOR", "ESSENTIALS", 480, 120, nullptr, 2, nullnoise::kUiSliders, nullnoise::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in noise_generator_gfx.jsfx: the spectrum's slope over 20 Hz..20 kHz, level in dB
    // (0 to -72), pivoting on Level at 1 kHz: white flat, pink -3 dB/oct, brown -6 dB/oct,
    // with the Low Cut and High Cut roll-offs (12 dB/oct)
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

        const int lc = int(std::lround(value(3))), hc = int(std::lround(value(4)));
        const int fo = int(std::lround(value(5))), dk = int(std::lround(value(6)));
        char a[24] = "", b[24] = "", line[64];
        if (fo > 0)
            std::snprintf(a, sizeof(a), "FOLLOW %d%%", fo);
        if (dk > 0)
            std::snprintf(b, sizeof(b), "%sDUCK %d%%", fo > 0 ? "  " : "", dk);
        std::snprintf(line, sizeof(line), "%s%s%s", a, b, value(7) >= 0.5f ? (fo > 0 || dk > 0 ? "  ADD" : "ADD") : "");
        label(fPX + fPW - 6.0f, fPY + 21.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);

        const float level = value(1);
        constexpr int n = 97;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const float t = float(i) / float(n - 1);
            const float f = 20.0f * std::pow(1000.0f, t);
            xs[i] = fPX + fPW * t;
            float db = level - float(type) * 3.0f * std::log2(f / 1000.0f);
            if (lc > 20)
                db -= 10.0f * std::log10(1.0f + std::pow(float(lc) / f, 4.0f));
            if (hc < 20000)
                db -= 10.0f * std::log10(1.0f + std::pow(f / float(hc), 4.0f));
            ys[i] = linY(db, -72.0f, 0.0f);
        }
        curve(xs, ys, n, fPY + fPH);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 3 && v <= 20.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 4 && v >= 20000.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 3 || param == 4)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UINoiseGenerator();
}

END_NAMESPACE_DISTRHO
