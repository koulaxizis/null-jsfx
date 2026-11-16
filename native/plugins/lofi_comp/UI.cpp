/*
 * NULL JSFX - Lo-Fi Comp (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "LofiCompDSP.hpp"
#include "LofiCompSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SQUASH", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "BITS", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "WOW", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UILofiComp : public NullKnobsUI
{
public:
    UILofiComp()
        : NullKnobsUI({ "LO-FI COMP", "ESSENTIALS", 480, 120, nullptr, 2, nulllofi::kSliders, nulllofi::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in lofi_comp_gfx.jsfx: the compressor's curve on the left (-60 to 0 dB, make-up
    // included), the input level against the threshold and the gain reduction on the right
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, 210.0f, fVH - 20.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-30");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-60");
        label(fPX, fPY + fPH + 10.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "IN");
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        double v[nulllofi::kNumSliders];
        for (uint32_t i = 0; i < nulllofi::kNumSliders; ++i)
            v[i] = jsfxValue(nulllofi::kSliders[i], value(i));
        const nulllofi::Params p = nulllofi::paramsFor(v, sr);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d:1", int(std::floor(p.ratio + 0.5)));
        label(fPX + fPW, fPY + fPH + 10.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        constexpr int n = 211;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nulllofi::curveDb(p, -60.0 + 60.0 * double(i) / double(n - 1))), -60.0f, 0.0f);
        }
        curve(xs, ys, n);

        float in = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(276.0f, 84.0f, 180.0f, 10.0f, "IN", nulljsfx::toDb(in), float(p.thr), -60.0f);
        grMeter(276.0f, 114.0f, 180.0f, 10.0f, "GR", gr, 40.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        if (param == 2)
        {
            if (double(v) >= sr)
                std::snprintf(buf, n, "OFF");
            else
                std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
        }
        else if (param == 6)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UILofiComp();
}

END_NAMESPACE_DISTRHO
