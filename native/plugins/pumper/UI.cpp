/*
 * NULL JSFX - Pumper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PumperDSP.hpp"

START_NAMESPACE_DISTRHO

class UIPumper : public NullOneSliderUI
{
public:
    UIPumper()
        : NullOneSliderUI({ "PUMPER", "AMOUNT", "SUBTLE", "DEEP", 0.0f, 100.0f, 50.0f, false, true }) {}

protected:
    // as in pumper_gfx.jsfx: the gain envelope over two beats (0 to -24 dB), the playhead dashed
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(8, 4, false);
        label(fPX - 6.0f, std::round(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, std::round(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-12");
        label(fPX - 6.0f, std::round(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX + fPW * 0.25f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "BEAT 1");
        label(fPX + fPW * 0.75f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "BEAT 2");

        float ph = 0.0f;
        if (nulljsfx::Taps* t = taps())
            ph = t->extra.load(std::memory_order_relaxed);
        dashedV(fPX + clampf(ph, 0.0f, 1.0f) * fPW * 0.5f);

        const double depth = value() / 100.0;
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double p = 2.0 * double(i) / double(n - 1);
            const double g = nullpumper::gainAt(p - std::floor(p), depth);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(20.0 * std::log10(std::fmax(g, 1e-7))), -24.0f, 0.0f);
        }
        curve(xs, ys, n, fPY);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
            std::snprintf(buf, n, "NO DIP");
        else
            std::snprintf(buf, n, "DIP %.1f dB ON EVERY BEAT", 20.0 * std::log10(1.0 - double(v) * 0.009));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIPumper)
};

UI* createUI()
{
    return new UIPumper();
}

END_NAMESPACE_DISTRHO
