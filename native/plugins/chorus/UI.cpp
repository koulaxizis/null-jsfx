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
    { 3, NullKnobsUI::kKnob, "VOICES", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SPREAD", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "FEEDBACK", 0, -1, false },
    { 6, NullKnobsUI::kSelect, "SYNC", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "HIGH CUT", 1, -1, false, true },
    { 8, NullKnobsUI::kSelect, "MODE", 1, -1, false },
};

class UIChorus : public NullKnobsUI
{
public:
    UIChorus()
        : NullKnobsUI({ "CHORUS", "ESSENTIALS", 480, 120, nullptr, 2, nullchorus::kUiSliders, nullchorus::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in chorus_gfx.jsfx: the delay time of every voice over two LFO cycles (0 to 16 ms), the
    // LFO position dashed; Classic: the first voice shaded, the others thinner, the right
    // channel's first voice grey when spread; Dimension: the two opposite triangles
    void drawDisplay() override
    {
        char a[24] = "", b[16] = "", buf[80];
        const double depth = value(1);
        const int sync = int(std::lround(value(6)));
        const bool dim = value(8) >= 0.5f;
        const int nv = std::max(1, std::min(4, int(std::lround(value(3)))));
        const float spread = value(4);
        if (dim)
            std::snprintf(a, sizeof(a), "DIMENSION  ");
        else if (nv > 1)
            std::snprintf(a, sizeof(a), "%d VOICES  ", nv);
        if (sync > 0)
            std::snprintf(b, sizeof(b), "%s  ", nullchorus::kSyncShort[sync]);
        std::snprintf(buf, sizeof(buf), "%s%s%.1f TO %.1f ms", a, b, 5.0, 5.0 + depth);
        modView("DELAY (ms)", buf);
        yLabels("16", "8", "0");
        // the period from the plugin's actual rate (the host tempo when synced)
        const float rate = tapAux0();
        xLabels(sync > 0 && rate > 0.0f ? 1.0 / rate : 1.0 / value(0));
        if (dim)
        {
            auto tri = [&](double sgn) {
                return [&, sgn](double u) {
                    const double p = 2.0 * u - std::floor(2.0 * u);
                    const double l = sgn * (1.0 - 2.0 * std::fabs(p * 2.0 - 1.0));
                    return linY(float(nullchorus::delayMsAt(depth, l)), 0.0f, 16.0f);
                };
            };
            modCurve(tri(-1.0), theme::kAccent, false, 0.45f);
            modCurve(tri(1.0), theme::kAccent, true);
        }
        else
        {
            auto sine = [&](double off) {
                return [&, off](double u) {
                    return linY(float(nullchorus::delayMsAt(depth, std::sin(4.0 * M_PI * u + off))), 0.0f, 16.0f);
                };
            };
            if (spread > 0.0f)
                modCurve(sine(spread / 100.0 * M_PI / nv), theme::kMuted, false);
            for (int v = 1; v < nv; ++v)
                modCurve(sine(v * 2.0 * M_PI / nv), theme::kAccent, false, 0.45f);
            modCurve(sine(0.0), theme::kAccent, true);
        }
        playhead(tapExtra() / 2.0);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            const int sync = int(std::lround(value(6)));
            if (sync > 0)
                std::snprintf(buf, n, "%s", nullchorus::kSyncShort[sync]);
            else
                std::snprintf(buf, n, "%.1f Hz", v);
            return;
        }
        if (param == 1)
        {
            std::snprintf(buf, n, "%.1f ms", v);
            return;
        }
        if (param == 3)
        {
            std::snprintf(buf, n, "%d", int(std::lround(v)));
            return;
        }
        if (param == 7)
        {
            if (v >= 20000.0f)
                std::snprintf(buf, n, "OFF");
            else
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
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
    void modCurve(F f, uint32_t rgb, bool fillIt, float alpha = 1.0f)
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
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(rgb, alpha));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        resetScissor();
    }

    // dashed playhead at u (0..1) of the plot width
    void playhead(double u) { dashedV(fPX + clampf(float(u), 0.0f, 1.0f) * fPW); }

    float tapAux0() const
    {
        nulljsfx::Taps* t = taps();
        return t ? t->aux[0].load(std::memory_order_relaxed) : 0.0f;
    }

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
