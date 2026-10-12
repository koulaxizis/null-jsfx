/*
 * NULL JSFX - Chorus (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ChorusDSP.hpp"
#include "ChorusSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIChorus : public NullKnobsUI
{
public:
    UIChorus()
        : NullKnobsUI({ "CHORUS", "ESSENTIALS", 480, 120, nullptr, 1, nullchorus::kSliders, nullchorus::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in chorus_gfx.jsfx: the delay time over two LFO cycles (0 to 16 ms), the LFO position dashed
    void drawDisplay() override
    {
        char buf[32];
        const double depth = value(1);
        std::snprintf(buf, sizeof(buf), "%.1f TO %.1f ms", 5.0, 5.0 + depth);
        modView("DELAY (ms)", buf);
        yLabels("16", "8", "0");
        xLabels(1.0 / value(0));
        modCurve([&](double u) {
            return linY(float(nullchorus::delayMsAt(depth, std::sin(4.0 * M_PI * u))), 0.0f, 16.0f);
        }, theme::kAccent, true);
        playhead(tapExtra() / 2.0);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            std::snprintf(buf, n, "%.1f Hz", v);
            return;
        }
        if (param == 1)
        {
            std::snprintf(buf, n, "%.1f ms", v);
            return;
        }
        NullKnobsUI::formatValue(param, v, buf, n);
    }

    // ---- the modulation view, as md_view / md_ylabels / md_xlabels / md_curve / md_playhead in
    // the GFX: an info line over a plot of two cycles
    void modView(const char* left, const char* right)
    {
        setPlot(fVX + 26.0f, fVY + 14.0f, fVW - 30.0f, fVH - 30.0f);
        grid(4, 4, false);
        label(fPX, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, left);
        label(fPX + fPW, fVY + 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, right);
    }

    // labels at the top, middle and bottom of the plot
    void yLabels(const char* top, const char* mid, const char* bot)
    {
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, top);
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, mid);
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, bot);
    }

    // times under the plot at 1/4, 1/2 and 3/4 of its width for a cycle of period seconds
    void xLabels(double period)
    {
        char buf[24];
        for (int i = 1; i <= 3; ++i)
        {
            const double t = period * i / 2.0;
            if (t < 1.0)
                std::snprintf(buf, sizeof(buf), "%d ms", int(std::floor(t * 1000.0 + 0.5)));
            else
                std::snprintf(buf, sizeof(buf), "%.2f s", t);
            label(fPX + fPW * float(i) / 4.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
    }

    // f(u) (a plot y for u = 0..1 across the plot) in colour rgb; the accent curve is shaded
    // down to the bottom when fillIt
    template <typename F>
    void modCurve(F f, uint32_t rgb, bool fillIt)
    {
        constexpr int n = 401;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double u = double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(u);
            ys[i] = float(f(u));
        }
        if (fillIt)
        {
            curve(xs, ys, n, fPY + fPH);
            return;
        }
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(rgb));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
    }

    // dashed playhead at u (0..1) of the plot width
    void playhead(double u) { dashedV(fPX + clampf(float(u), 0.0f, 1.0f) * fPW); }

    float tapExtra() const
    {
        nulljsfx::Taps* t = taps();
        return t ? t->extra.load(std::memory_order_relaxed) : 0.0f;
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIChorus)
};

UI* createUI()
{
    return new UIChorus();
}

END_NAMESPACE_DISTRHO
