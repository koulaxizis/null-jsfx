/*
 * NULL JSFX - De-Clip (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DeClipDSP.hpp"
#include "DeClipSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "CLIP LEVEL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "MIN LENGTH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
    { 3, NullKnobsUI::kSelect, "LISTEN", 0, -1, false },
};

class UIDeClip : public NullKnobsUI
{
public:
    UIDeClip()
        : NullKnobsUI({ "DE-CLIP", "ESSENTIALS", 480, 120, nullptr, 1, nulldeclip::kSliders, nulldeclip::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in de_clip_gfx.jsfx: the last 2048 samples, input (muted) under output (accent), mid of
    // L and R on a +-2 scale, the clip level dashed; the runs rebuilt and the latency
    void drawDisplay() override
    {
        setPlot(fVX + 4.0f, fVY + 4.0f, fVW - 8.0f, fVH - 8.0f);
        grid(8, 4, true);
        const float thr = float(std::pow(10.0, nulldeclip::sliderValue(0, value(0)) / 20.0));
        for (int side = 0; side < 2; ++side)
        {
            const float y = std::floor(fPY + fPH * (0.5f + (side ? thr : -thr) * 0.25f)) + 0.5f;
            beginPath();
            for (float x = fPX; x < fPX + fPW; x += 6.0f)
            {
                moveTo(x, y);
                lineTo(std::fmin(x + 3.0f, fPX + fPW), y);
            }
            strokeColor(col(theme::kText, 0.35f));
            strokeWidth(1.0f);
            stroke();
        }
        nulljsfx::Taps* t = taps();
        if (t)
        {
            wave(t, t->scopeL, col(theme::kMuted, 0.8f));
            wave(t, t->scopeR, col(theme::kAccent));
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "RUNS REBUILT %d", t ? int(t->extra.load(std::memory_order_relaxed)) : 0);
        label(fPX + 6.0f, fPY + 9.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        const int delay = nulldeclip::DeClipProcessor::latency();
        std::snprintf(buf, sizeof(buf), "LATENCY %d SAMPLES (%.2f ms)", delay, delay / sr * 1000.0);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
    }

    // min/max per pixel column of one tap channel on +-2 (dg_scope in the GFX JSFX)
    void wave(nulljsfx::Taps* t, const float* ch, const Color& c)
    {
        const uint32_t len = nulljsfx::Taps::kScopeLen, pos = t->scopePos.load(std::memory_order_acquire);
        const int n = int(fPW);
        const uint32_t per = std::max<uint32_t>(1, len / uint32_t(n));
        beginPath();
        for (int i = 0; i < n; ++i)
        {
            float mn = 2.0f, mx = -2.0f;
            for (uint32_t j = 0; j < per; ++j)
            {
                const float v = ch[(pos + uint32_t(i) * per + j) % len];
                mn = std::fmin(mn, v);
                mx = std::fmax(mx, v);
            }
            const float x = fPX + float(i) + 0.5f;
            moveTo(x, fPY + fPH * (0.5f - std::fmin(2.0f, mx) * 0.25f));
            lineTo(x, fPY + fPH * (0.5f - std::fmax(-2.0f, mn) * 0.25f) + 0.5f);
        }
        strokeColor(c);
        strokeWidth(1.0f);
        stroke();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else if (param == 1)
            std::snprintf(buf, n, "%d smp", int(std::lround(v)));
        else if (param == 2)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDeClip();
}

END_NAMESPACE_DISTRHO
