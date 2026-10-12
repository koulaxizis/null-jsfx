/*
 * NULL JSFX - Breath Control (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BreathControlSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "ATTACK", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "RANGE", 0, -1, false },
};

class UIBreathControl : public NullKnobsUI
{
public:
    UIBreathControl()
        : NullKnobsUI({ "BREATH CONTROL", "ESSENTIALS", 480, 120, nullptr, 1, nullbreath::kSliders, nullbreath::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260; // history entries, one per design pixel (7 ms each)

    void hline(float y, uint32_t rgb, float alpha)
    {
        y = std::floor(y) + 0.5f;
        beginPath();
        moveTo(fPX, y);
        lineTo(fPX + fPW, y);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    // as in breath_control_gfx.jsfx: left, the last 1.8 s of detector level (bars) and gain
    // (line), both -72..0 dB, with the threshold; right, detector, gain-reduction and output meters
    void drawDisplay() override
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 3, false);
        char buf[16];
        for (int i = 0; i < 4; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -24 * i);
            label(fPX - 6.0f, fPY + fPH * float(i) / 3.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float xs[kLen], ys[kLen], lv[kLen];
        for (int i = 0; i < kLen; ++i)
        {
            float det = 0.0f, g = 1.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                det = t->scopeL[k];
                g = t->scopeR[k];
            }
            lv[i] = fPH * dbNorm(nulljsfx::toDb(det), -72.0f);
            xs[i] = fPX + float(i) + 0.5f;
            ys[i] = linY(nulljsfx::toDb(g), -72.0f, 0.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        hline(linY(value(0), -72.0f, 0.0f), theme::kText, 0.35f);
        curve(xs, ys, kLen);

        float det = 0.0f, out = 0.0f, gr = 0.0f;
        if (t)
        {
            det = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(det), value(0), -72.0f);
        grMeter(336.0f, 106.0f, 112.0f, 10.0f, "GR", gr, 24.0f);
        meter(336.0f, 130.0f, 112.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -72.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIBreathControl();
}

END_NAMESPACE_DISTRHO
