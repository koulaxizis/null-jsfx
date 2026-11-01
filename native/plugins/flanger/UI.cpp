/*
 * NULL JSFX - Flanger (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "FlangerDSP.hpp"
#include "FlangerSliders.hpp"

#include <cstring>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "FEEDBACK", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MANUAL", 1, -1, false },
    { 5, NullKnobsUI::kSelect, "SYNC", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "PHASE", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "THRU-ZERO", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "BARBER", 1, -1, false },
};

class UIFlanger : public NullKnobsUI
{
public:
    UIFlanger()
        : NullKnobsUI({ "FLANGER", "ESSENTIALS", 480, 120, nullptr, 2, nullflanger::kUiSliders, nullflanger::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in flanger_gfx.jsfx: the delay time over two LFO cycles (0 to 32 ms), the LFO position
    // dashed; the right channel grey when Stereo Phase shifts it, the dry path's delay dashed
    // in Thru Zero
    void drawDisplay() override
    {
        char a[32] = "", b[16] = "", buf[80];
        const double depth = value(1), manual = value(4);
        const int sync = int(std::lround(value(5)));
        const float ph = value(6);
        const bool tz = value(7) >= 0.5f;
        if (tz)
            std::snprintf(a, sizeof(a), "THRU-ZERO  ");
        const int barber = int(std::lround(value(8)));
        if (barber)
            std::snprintf(a + std::strlen(a), sizeof(a) - std::strlen(a), "%s", barber == 2 ? "BARBER DOWN  " : "BARBER UP  ");
        if (sync > 0)
            std::snprintf(b, sizeof(b), "%s  ", nullflanger::kSyncShort[sync]);
        std::snprintf(buf, sizeof(buf), "%s%s%.1f TO %.1f ms", a, b, manual, manual + depth);
        modView("DELAY (ms)", buf);
        yLabels("32", "16", "0");
        // the period from the plugin's actual rate (the host tempo when synced)
        const float rate = tapAux(0);
        xLabels(sync > 0 && rate > 0.0f ? 1.0 / rate : 1.0 / value(0));
        auto sweep = [&](double off) {
            return [&, off](double u) {
                return linY(float(nullflanger::delayMsAt(depth, std::sin(4.0 * M_PI * u + off), manual)), 0.0f, 32.0f);
            };
        };
        if (ph > 0.0f)
            modCurve(sweep(ph * M_PI / 180.0), theme::kMuted, false);
        modCurve(sweep(0.0), theme::kAccent, true);
        if (tz)
        {
            // the dry delay in whole samples from the plugin, else from the settings at 48 kHz
            float ms = tapAux(1);
            if (ms <= 0.0f)
                ms = float(std::floor((manual + depth * 0.5) * 48.0 + 0.5) / 48.0);
            dashedH(linY(ms, 0.0f, 32.0f));
        }
        playhead(tapExtra() / 2.0);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            const int sync = int(std::lround(value(5)));
            if (sync > 0)
                std::snprintf(buf, n, "%s", nullflanger::kSyncShort[sync]);
            else
                std::snprintf(buf, n, "%.1f Hz", v);
            return;
        }
        if (param == 1 || param == 4)
        {
            std::snprintf(buf, n, "%.1f ms", v);
            return;
        }
        NullKnobsUI::formatValue(param, v, buf, n);
    }

    // dashed horizontal line at y, as md_hline in the GFX
    void dashedH(float y)
    {
        y = std::floor(y) + 0.5f;
        beginPath();
        for (float x = fPX; x < fPX + fPW; x += 6.0f)
        {
            moveTo(x, y);
            lineTo(std::fmin(x + 3.0f, fPX + fPW), y);
        }
        strokeColor(col(theme::kText, 0.6f));
        strokeWidth(1.0f);
        stroke();
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

    float tapAux(int i) const
    {
        nulljsfx::Taps* t = taps();
        return t ? t->aux[i].load(std::memory_order_relaxed) : 0.0f;
    }

    float tapExtra() const
    {
        nulljsfx::Taps* t = taps();
        return t ? t->extra.load(std::memory_order_relaxed) : 0.0f;
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIFlanger)
};

UI* createUI()
{
    return new UIFlanger();
}

END_NAMESPACE_DISTRHO
