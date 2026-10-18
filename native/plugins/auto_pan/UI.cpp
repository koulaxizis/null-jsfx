/*
 * NULL JSFX - Auto Pan (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AutoPanDSP.hpp"
#include "AutoPanSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "SYNC", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "NOTE", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "MODE", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "SMOOTHING", 1, -1, false },
};

class UIAutoPan : public NullKnobsUI
{
public:
    UIAutoPan()
        : NullKnobsUI({ "AUTO PAN", "ESSENTIALS", 480, 120, nullptr, 2, nullautopan::kSliders, nullautopan::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the LFO rate actually used: from the plugin (the host's tempo when synced), else Rate
    double rate() const
    {
        if (nulljsfx::Taps* t = taps())
        {
            const float r = t->gr.load(std::memory_order_relaxed);
            if (r > 0.0f)
                return r;
        }
        return value(0);
    }

    // as in auto_pan_gfx.jsfx: the left and right levels over two LFO cycles (step modes: the
    // whole pattern, one step per cycle) from 0 to 100%, the LFO position dashed
    void drawDisplay() override
    {
        char buf[40];
        const double r = rate();
        const int mode = int(std::lround(value(5)));
        const int n = nullautopan::stepCount(mode);
        if (mode != 0)
            std::snprintf(buf, sizeof(buf), "%d STEPS AT %.2f Hz", n, r);
        else
            std::snprintf(buf, sizeof(buf), "%.2f Hz", r);
        modView("", buf);
        label(fPX, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kAccent, "LEFT");
        label(fPX + 34.0f, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "RIGHT");
        yLabels("100%", "50%", "0%");
        if (r > 0.0)
            xLabels(mode != 0 ? double(n) / 2.0 / r : 1.0 / r);
        const double depth = value(1) / 100.0, mix = value(2) / 100.0;
        const double smooth = std::max(std::round(value(6)) / 100.0, 0.001 * r);
        for (int ch = 1; ch >= 0; --ch)
            modCurve([&](double u) {
                double lfo;
                if (mode != 0)
                {
                    const double p = std::min(u * n, n - 0.000001);
                    const double k = std::floor(p);
                    const double t = (p - k) / smooth;
                    const double c = nullautopan::stepValue(mode, k);
                    const double q = nullautopan::stepValue(mode, k > 0.0 ? k - 1.0 : double(n - 1));
                    lfo = t < 1.0 ? q + (c - q) * (0.5 - 0.5 * std::cos(M_PI * t)) : c;
                }
                else
                    lfo = std::sin(4.0 * M_PI * u);
                return linY(float(1.0 - mix + mix * (0.5 + (ch ? -0.5 : 0.5) * depth * lfo)), 0.0f, 1.0f);
            }, ch ? theme::kMuted : theme::kAccent, ch == 0);
        if (mode != 0)
        {
            nulljsfx::Taps* t = taps();
            const float step = t ? t->aux[0].load(std::memory_order_relaxed) : 0.0f;
            playhead((double(step) + tapExtra()) / double(n));
        }
        else
            playhead(tapExtra() / 2.0);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (value(3) >= 0.5f)
                std::snprintf(buf, n, "%.2f Hz", rate());
            else
                std::snprintf(buf, n, "%.1f Hz", v);
        }
        else
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
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIAutoPan)
};

UI* createUI()
{
    return new UIAutoPan();
}

END_NAMESPACE_DISTRHO
