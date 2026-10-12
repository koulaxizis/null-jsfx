/*
 * NULL JSFX - Pitch Shifter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PitchShifterDSP.hpp"
#include "PitchShifterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "OCTAVE", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "SEMITONES", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "FINE", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIPitchShifter : public NullKnobsUI
{
public:
    UIPitchShifter()
        : NullKnobsUI({ "PITCH SHIFTER", "ESSENTIALS", 480, 120, nullptr, 1, nullpitchshifter::kSliders, nullpitchshifter::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in pitch_shifter_gfx.jsfx: the delay of the two read taps over two sweeps (0 to 60 ms),
    // tap 1 dashed; the taps move up the line when the pitch goes down, down it when it goes up
    void drawDisplay() override
    {
        char buf[40];
        const double sh = nullpitchshifter::totalShift(value(0), value(1), value(2));
        const double pf = std::pow(2.0, sh / 12.0), inc = 1.0 - pf; // the sign of ph_inc
        if (sh == 0.0)
            std::snprintf(buf, sizeof(buf), "NO SHIFT");
        else
            std::snprintf(buf, sizeof(buf), sh > 0.0 ? "+%.2f ST, x%.3f" : "%.2f ST, x%.3f", sh, pf);
        modView("", buf);
        label(fPX, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kAccent, "TAP 1");
        label(fPX + 34.0f, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "TAP 2");
        label(fPX + 68.0f, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "(ms)");
        yLabels("60", "30", "0");
        if (inc != 0.0)
            xLabels(0.06 / std::fabs(inc));
        for (int ch = 1; ch >= 0; --ch)
            modCurve([&](double u) {
                double p = inc > 0.0 ? 2.0 * u - std::floor(2.0 * u) : inc < 0.0 ? 1.0 - (2.0 * u - std::floor(2.0 * u)) : 0.0;
                if (p >= 1.0)
                    p -= 1.0;
                if (ch)
                {
                    p += 0.5;
                    if (p >= 1.0)
                        p -= 1.0;
                }
                return linY(float(p * 60.0), 0.0f, 60.0f);
            }, ch ? theme::kMuted : theme::kAccent, false);
        if (inc != 0.0)
        {
            const double ph = tapExtra();
            playhead(inc > 0.0 ? ph / 2.0 : (1.0 - ph) / 2.0);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0 || param == 1)
            std::snprintf(buf, n, v > 0.0f ? "+%d %s" : "%d %s", int(std::lround(v)), param == 0 ? "oct" : "st");
        else if (param == 2)
            std::snprintf(buf, n, v > 0.0f ? "+%.1f ct" : "%.1f ct", v);
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
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIPitchShifter)
};

UI* createUI()
{
    return new UIPitchShifter();
}

END_NAMESPACE_DISTRHO
