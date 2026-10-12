/*
 * NULL JSFX - Sidechain Ducker (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SidechainDuckerDSP.hpp"
#include "SidechainDuckerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "KEY", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "KEY HP", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "ATTACK", 1, -1, false },
    { 3, NullKnobsUI::kKnob, "HOLD", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "RELEASE", 1, -1, false },
};

class UISidechainDucker : public NullKnobsUI
{
public:
    UISidechainDucker()
        : NullKnobsUI({ "SIDECHAIN DUCKER", "ESSENTIALS", 480, 120, nullptr, 2, nullducker::kSliders,
                        nullducker::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in sidechain_ducker_gfx.jsfx: the ducking shape of one key hit on the left, the key
    // level against Threshold and the gain reduction on the right
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, 210.0f, fVH - 20.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-20");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-40");
        const double depth = value(1), att = value(2), hold = value(3), rel = value(4);
        const double total = (5.0 * att + hold + 5.0 * rel) * 1.25;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d ms", int(std::floor(total + 0.5)));
        label(fPX + fPW, fPY + fPH + 10.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        label(fPX, fPY + fPH + 10.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "KEY");

        constexpr int n = 121;
        float xs[n], ys[n];
        nullducker::DuckShape shape;
        for (int i = 0; i < n; ++i)
        {
            const double db = i == 0 ? 0.0 : shape.step(total * (i - 1) / 120.0, total / 120.0, depth, att, hold, rel);
            xs[i] = fPX + fPW * float(i) / 120.0f;
            ys[i] = linY(float(db), -40.0f, 0.0f);
        }
        curve(xs, ys, n);

        float key = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            key = t->inLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(276.0f, 84.0f, 180.0f, 10.0f, "KEY", nulljsfx::toDb(key), value(0), -60.0f);
        grMeter(276.0f, 114.0f, 180.0f, 10.0f, "GR", gr, 40.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 6 && v <= 0.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 0 || param == 1 || param == 2)
            std::snprintf(buf, n, "%.1f %s", double(v), param == 2 ? "ms" : "dB");
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISidechainDucker();
}

END_NAMESPACE_DISTRHO
