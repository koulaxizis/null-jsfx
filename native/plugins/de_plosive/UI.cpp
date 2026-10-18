/*
 * NULL JSFX - De-Plosive (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DePlosiveSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SENSITIVITY", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "FREQUENCY", 0, -1, false, true },
    { 2, NullKnobsUI::kKnob, "REDUCTION", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "LISTEN", 0, -1, false },
};

class UIDePlosive : public NullKnobsUI
{
public:
    UIDePlosive()
        : NullKnobsUI({ "DE-PLOSIVE", "ESSENTIALS", 480, 120, nullptr, 1, nulldeplosive::kSliders,
                        nulldeplosive::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260; // history entries, one per design pixel (7 ms each)

    // the history: the low band as bars (-48..0 dB, brighter where a plosive was cut), its gain
    // as the line (-32..0 dB)
    void history()
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 4, false);
        char buf[16];
        for (int i = 0; i < 5; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -8 * i);
            label(fPX - 6.0f, fPY + fPH * float(i) / 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float xs[kLen], ys[kLen], lv[kLen];
        bool cut[kLen];
        for (int i = 0; i < kLen; ++i)
        {
            float level = 0.0f, g = 1.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                level = t->scopeL[k];
                g = t->scopeR[k];
            }
            lv[i] = fPH * dbNorm(nulljsfx::toDb(level), -48.0f);
            cut[i] = g < 0.891f;
            xs[i] = fPX + float(i) + 0.5f;
            ys[i] = linY(nulljsfx::toDb(g), -32.0f, 0.0f);
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (int i = 0; i < kLen; ++i)
                if (lv[i] > 0.0f && cut[i] == (pass == 1))
                    rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
            fillColor(col(theme::kAccent, pass == 1 ? 0.45f : 0.15f));
            fill();
        }
        curve(xs, ys, kLen);
    }

    // as in de_plosive_gfx.jsfx: left, the history; right, the low-band level and gain-reduction
    // meters and the plosive light
    void drawDisplay() override
    {
        history();
        float low = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            low = t->inLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(336.0f, 76.0f, 112.0f, 10.0f, "LOW", nulljsfx::toDb(low), -140.0f, -48.0f);
        grMeter(336.0f, 100.0f, 112.0f, 10.0f, "GR", gr, 30.0f);
        const bool on = gr > 1.0f;
        beginPath();
        circle(340.0f, 129.0f, 4.0f);
        fillColor(col(on ? theme::kAccent : theme::kBorder));
        fill();
        label(350.0f, 129.0f, ALIGN_LEFT, 7.5f, 1.0f, on ? theme::kText : theme::kMuted, "PLOSIVE", true);
    }
};

UI* createUI()
{
    return new UIDePlosive();
}

END_NAMESPACE_DISTRHO
