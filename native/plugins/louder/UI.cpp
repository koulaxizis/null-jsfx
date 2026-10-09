/*
 * NULL JSFX - Louder (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "LouderDSP.hpp"

START_NAMESPACE_DISTRHO

class UILouder : public NullOneSliderUI
{
public:
    UILouder()
        : NullOneSliderUI({ "LOUDER", "AMOUNT", "NATURAL", "LOUD", 0.0f, 100.0f, 50.0f, false, true }) {}

protected:
    // as in louder_gfx.jsfx: the static in/out curve on the left, IN / GR / OUT meters on the right
    void drawDisplay() override
    {
        setPlot(38.0f, 68.0f, 80.0f, 80.0f);
        grid(4, 4, false);

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        const float gainDb = value() / 100.0f * float(nulllouder::kMaxGainDb);
        constexpr int n = 81;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const float in = -36.0f + 36.0f * float(i) / float(n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(std::fmin(in + gainDb, float(nulllouder::kCeilingDb)), -36.0f, 0.0f);
        }
        curve(xs, ys, n, fPY + fPH);

        float in = 0.0f, out = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(138.0f, 74.0f, 186.0f, 10.0f, "IN", nulljsfx::toDb(in), -140.0f, -48.0f);
        grMeter(138.0f, 104.0f, 186.0f, 10.0f, "GR", gr, 12.0f);
        meter(138.0f, 134.0f, 186.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -48.0f);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        std::snprintf(buf, n, "GAIN +%.1f dB  CEILING -0.3 dB", double(v) * 0.12);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UILouder)
};

UI* createUI()
{
    return new UILouder();
}

END_NAMESPACE_DISTRHO
