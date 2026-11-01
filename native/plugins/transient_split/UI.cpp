/*
 * NULL JSFX - Transient Split (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TransientSplitDSP.hpp"
#include "TransientSplitSliders.hpp"

START_NAMESPACE_DISTRHO

// one row, as in transient_split_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TRANSIENT", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "TONAL", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "SENSITIVITY", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UITransientSplit : public NullKnobsUI
{
public:
    UITransientSplit()
        : NullKnobsUI({ "TRANSIENT SPLIT", "ADVANCED", 480, 120, nullptr, 1, nulltransientsplit::kUiSliders,
                        nulltransientsplit::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260;

    // as in transient_split_gfx.jsfx: left, the last 2.6 s of input level (bars, -60..0 dB) with
    // the transient weight as the line (0..1); right, the input meter, the weight now and where
    // the two parts go
    void drawDisplay() override
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "T");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");

        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float xs[kLen], ys[kLen], lv[kLen];
        for (int i = 0; i < kLen; ++i)
        {
            float w = 0.0f, lev = 0.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                w = t->scopeL[k];
                lev = t->scopeR[k];
            }
            lv[i] = fPH * dbNorm(nulljsfx::toDb(lev), -60.0f);
            xs[i] = fPX + float(i) + 0.5f;
            ys[i] = linY(w, 0.0f, 1.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        curve(xs, ys, kLen);

        float in = 0.0f, w = 0.0f;
        if (t)
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            w = t->gr.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(in), -140.0f, -60.0f);
        // the weight as a meter: 0..1 across
        meter(336.0f, 106.0f, 112.0f, 10.0f, "TR", -60.0f + 60.0f * clampf(w, 0.0f, 1.0f), -140.0f, -60.0f);
        const bool split = nulltransientsplit::sliderValue(0, value(0)) >= 0.5;
        label(336.0f, 135.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, split ? "SPLIT  1/2 T  3/4 N" : "BALANCE  1/2", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nulltransientsplit::sliderValue(param, v);
        if (param == 1 || param == 2 || param == 5)
            std::snprintf(buf, n, "%+.1f dB", x);
        else if (param == 3)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 4)
            std::snprintf(buf, n, "%d ms", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UITransientSplit();
}

END_NAMESPACE_DISTRHO
