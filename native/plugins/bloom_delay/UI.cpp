/*
 * NULL JSFX - Bloom Delay (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BloomDelaySliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIME", 0, -1, false, true },
    { 1, NullKnobsUI::kSelect, "SYNC", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "FEEDBACK", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "BLOOM", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MOD", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "DAMPING", 1, -1, false, true },
    { 6, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIBloomDelay : public NullKnobsUI
{
public:
    UIBloomDelay()
        : NullKnobsUI({ "BLOOM DELAY", "ADVANCED", 480, 120, nullptr, 2, nullbloom::kSliders, nullbloom::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in bloom_delay_gfx.jsfx: the input (dim) and twelve repeats at Feedback ^ (k - 1) down
    // to -48 dB, each wider as Bloom smears it (Damping, Mod and Width left out)
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(13, 2, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-48");
        label(fPX, fPY + fPH + 10.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "REPEATS");

        static const char* const kSync[] = { "", "1/16", "1/8", "1/8 DOTTED", "1/4", "1/4 DOTTED", "1/2", "1/1" };
        const int sync = std::max(0, std::min(7, int(std::lround(value(1)))));
        const double ms = jsfxValue(nullbloom::kSliders[0], value(0));
        char buf[32];
        if (sync > 0)
            std::snprintf(buf, sizeof(buf), "SYNC %s", kSync[sync]);
        else if (ms >= 1000.0)
            std::snprintf(buf, sizeof(buf), "%.2f s", ms / 1000.0);
        else
            std::snprintf(buf, sizeof(buf), "%d ms", int(ms));
        label(fPX + fPW, fVY + 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        const double fb = jsfxValue(nullbloom::kSliders[2], value(2)) / 100.0;
        const double bloom = jsfxValue(nullbloom::kSliders[3], value(3)) / 100.0;
        for (int k = 0; k < 13; ++k)
        {
            const float x = fPX + fPW * (k + 0.5f) / 13.0f;
            const double a = k == 0 ? 1.0 : std::pow(fb, k - 1);
            if (!(a > 0.004))
                continue;
            const float y = linY(float(20.0 * std::log10(std::max(a, 0.000001))), -48.0f, 0.0f);
            const float hw = float(1.0 + bloom * std::min(k, 6) * 2.5);
            beginPath();
            moveTo(x - hw, fPY + fPH);
            lineTo(x, y);
            lineTo(x + hw, fPY + fPH);
            strokeColor(k == 0 ? col(theme::kMuted, 0.6f) : col(theme::kAccent));
            strokeWidth(2.0f);
            lineJoin(ROUND);
            stroke();
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f s", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        }
        else if (param == 5)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 8)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIBloomDelay();
}

END_NAMESPACE_DISTRHO
