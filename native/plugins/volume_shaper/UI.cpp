/*
 * NULL JSFX - Volume Shaper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "VolumeShaperDSP.hpp"
#include "VolumeShaperSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SHAPE", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "RATE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SMOOTH", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "PHASE", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIVolumeShaper : public NullKnobsUI
{
public:
    UIVolumeShaper()
        : NullKnobsUI({ "VOLUME SHAPER", "ESSENTIALS", 480, 120, nullptr, 1, nullvolshaper::kSliders,
                        nullvolshaper::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in volume_shaper_gfx.jsfx: the gain over one cycle (0 to 100%), the playhead dashed
    // with a dot at the current gain; below, the cycle length and the current gain in dB
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(8, 4, false);
        label(fPX - 6.0f, std::floor(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "100%");
        label(fPX - 6.0f, std::floor(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "50%");
        label(fPX - 6.0f, std::floor(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");

        const nullvolshaper::Params p = nullvolshaper::paramsFrom([this](uint32_t i) { return value(i); });
        char buf[40];
        const int rate = std::max(0, std::min(6, p.rate));
        if (p.shape == nullvolshaper::kHalfTime)
            std::snprintf(buf, sizeof(buf), "ONE CYCLE = 2 x %s", nullvolshaper::kRateOptions[rate]);
        else
            std::snprintf(buf, sizeof(buf), "ONE CYCLE = %s", nullvolshaper::kRateOptions[rate]);
        label(fPX, fPY + fPH + 11.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);

        float pos = 0.0f, g = 1.0f;
        if (nulljsfx::Taps* t = taps())
        {
            pos = clampf(t->extra.load(std::memory_order_relaxed), 0.0f, 1.0f);
            g = t->gr.load(std::memory_order_relaxed);
        }
        const double mix = p.mixPct / 100.0;
        const double db = 20.0 * std::log10(std::fmax(double(g) * mix + 1.0 - mix, 0.0000001));
        if (db < -60.0)
            std::snprintf(buf, sizeof(buf), "GAIN -INF dB");
        else
            std::snprintf(buf, sizeof(buf), "GAIN %.1f dB", db);
        label(fPX + fPW, fPY + fPH + 11.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        // the curve: Depth and Phase applied, two points per design unit so the gates stay sharp
        const nullvolshaper::Shapes& sh = nullvolshaper::shapes();
        const double depth = p.depthPct / 100.0, phase = p.phasePct / 100.0;
        constexpr int kMaxN = 1024;
        const int n = std::min(kMaxN, int(fPW * 2.0f) + 1);
        float xs[kMaxN], ys[kMaxN];
        for (int i = 0; i < n; ++i)
        {
            const double x = double(i) / double(n - 1) * 0.999999;
            double q = x + phase;
            if (q >= 1.0)
                q -= 1.0;
            const double gain = 1.0 - depth * (1.0 - sh.eval(p.shape, q));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(gain), 0.0f, 1.0f);
        }
        curve(xs, ys, n, fPY + fPH);

        // playhead and the current (smoothed) gain
        const float x = std::floor(fPX + pos * fPW) + 0.5f;
        dashedV(x);
        beginPath();
        circle(x, linY(g, 0.0f, 1.0f), 3.0f);
        fillColor(col(theme::kText));
        fill();
    }
};

UI* createUI()
{
    return new UIVolumeShaper();
}

END_NAMESPACE_DISTRHO
