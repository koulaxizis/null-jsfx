/*
 * NULL JSFX - Click Repair (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ClickRepairSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "WINDOW", 0, -1, false },
};

class UIClickRepair : public NullKnobsUI
{
public:
    UIClickRepair()
        : NullKnobsUI({ "CLICK REPAIR", "ESSENTIALS", 480, 120, nullptr, 1, nullclick::kSliders, nullclick::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in click_repair_gfx.jsfx: the last 2048 samples, unrepaired (scopeL, muted) under
    // repaired (scopeR, accent), mid of L and R; the click count and the latency
    void drawDisplay() override
    {
        setPlot(fVX + 4.0f, fVY + 4.0f, fVW - 8.0f, fVH - 8.0f);
        grid(8, 2, true);
        nulljsfx::Taps* t = taps();
        if (t)
        {
            wave(t, t->scopeL, col(theme::kMuted, 0.8f));
            wave(t, t->scopeR, col(theme::kAccent));
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "CLICKS %d", t ? int(t->extra.load(std::memory_order_relaxed)) : 0);
        label(fPX + 6.0f, fPY + 9.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        const int delay = 2 * std::max(1, std::min(50, int(std::floor(value(1)))));
        std::snprintf(buf, sizeof(buf), "LATENCY %d SAMPLES (%.2f ms)", delay, delay / sr * 1000.0);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
    }

    // min/max per pixel column of one tap channel (cr_scope in the GFX JSFX)
    void wave(nulljsfx::Taps* t, const float* ch, const Color& c)
    {
        const uint32_t len = nulljsfx::Taps::kScopeLen, pos = t->scopePos.load(std::memory_order_acquire);
        const int n = int(fPW);
        const uint32_t per = std::max<uint32_t>(1, len / uint32_t(n));
        beginPath();
        for (int i = 0; i < n; ++i)
        {
            float mn = 1.0f, mx = -1.0f;
            for (uint32_t j = 0; j < per; ++j)
            {
                const float v = ch[(pos + uint32_t(i) * per + j) % len];
                mn = std::fmin(mn, v);
                mx = std::fmax(mx, v);
            }
            const float x = fPX + float(i) + 0.5f;
            moveTo(x, fPY + fPH * (0.5f - std::fmin(1.0f, mx) * 0.5f));
            lineTo(x, fPY + fPH * (0.5f - std::fmax(-1.0f, mn) * 0.5f) + 0.5f);
        }
        strokeColor(c);
        strokeWidth(1.0f);
        stroke();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%d smp", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIClickRepair();
}

END_NAMESPACE_DISTRHO
