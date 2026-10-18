/*
 * NULL JSFX - Tremolo (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TremoloDSP.hpp"
#include "TremoloSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "SHAPE", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "SYNC", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "PHASE", 1, -1, false },
    { 6, NullKnobsUI::kSelect, "MODE", 1, -1, false },
};

class UITremolo : public NullKnobsUI
{
public:
    UITremolo()
        : NullKnobsUI({ "TREMOLO", "ESSENTIALS", 480, 120, nullptr, 2, nulltremolo::kUiSliders, nulltremolo::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the rate in use: the host's tempo when synced (from the plugin), else Rate
    double rateNow() const
    {
        const int sync = int(std::lround(value(4)));
        if (sync > 0)
        {
            if (nulljsfx::Taps* t = taps())
            {
                const float hz = t->gr.load(std::memory_order_relaxed);
                if (hz > 0.0f)
                    return hz;
            }
            return 120.0 / 60.0 / nulltremolo::syncBeats(sync);
        }
        return nulltremolo::onGrid(value(0), 0.1, 20.0, 0.1);
    }

    // as in tremolo_gfx.jsfx: the output level over one LFO cycle (0 to 100%), the LFO position
    // dashed; the right channel (Stereo Phase) or, in Harmonic, the highs as a second, muted curve
    void drawDisplay() override
    {
        char buf[32];
        const double depth = value(1), mix = value(2), dip = depth * mix / 10000.0;
        if (dip >= 1.0)
            std::snprintf(buf, sizeof(buf), "DIPS TO SILENCE");
        else if (dip > 0.0)
            std::snprintf(buf, sizeof(buf), "DIPS TO %.1f dB", 20.0 * std::log10(1.0 - dip));
        else
            std::snprintf(buf, sizeof(buf), "NO DIP");
        const int shape = int(std::lround(value(3)));
        const double deg = std::lround(value(5));
        const bool st = deg > 0.0, harm = value(6) >= 0.5f;
        const double off = deg / 360.0 * nulltremolo::kTwoPi;
        const double hz = rateNow();
        const nulltremolo::Edges e = nulltremolo::edgesFor(hz);
        modView(st ? (harm ? "LOWS   LEFT / RIGHT" : "LEFT / RIGHT") : (harm ? "LOWS / HIGHS" : "LEVEL"), buf);
        yLabels("100%", "50%", "0%");
        xLabels(1.0 / std::max(0.001, hz));
        auto level = [&](double u, bool right, bool hi) {
            double l = nulltremolo::lfoValue(shape, nulltremolo::kTwoPi * u + (right ? off : 0.0), e);
            if (hi)
                l = -l;
            return linY(float(nulltremolo::levelAt(depth, mix, l)), 0.0f, 1.0f);
        };
        if (st || harm)
            modCurve([&](double u) { return level(u, st, !st); }, theme::kMuted, false);
        modCurve([&](double u) { return level(u, false, false); }, theme::kAccent, true);
        playhead(tapExtra());
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            std::snprintf(buf, n, "%.1f Hz", value(4) >= 0.5f ? rateNow() : double(v));
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
            const double t = period * i / 4.0;
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
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UITremolo)
};

UI* createUI()
{
    return new UITremolo();
}

END_NAMESPACE_DISTRHO
