/*
 * NULL JSFX - Humanize (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "HumanizeSliders.hpp"

START_NAMESPACE_DISTRHO

// as humanize_gfx.jsfx: timing, velocity, groove, amount, seed
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIMING", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "VELOCITY", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "GROOVE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SEED", 0, -1, false },
};

class UIHumanize : public NullKnobsUI
{
public:
    UIHumanize()
        : NullKnobsUI({ "HUMANIZE", "MIDI", 480, 150, nullptr, 1, nullhumanize::kUiSliders, nullhumanize::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nullhumanize::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<HumanizeBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    // the groove's shift (beats) of a note at beat b, as g_shift in humanize_gfx.jsfx
    double shift(double b, double tempo) const
    {
        const int grv = iv(2);
        const double ga = iv(3) / 100.0;
        if (grv == 1 || grv == 2)
        {
            const double p = grv == 1 ? 1.0 : 0.5;
            const double x = b / p - std::floor(b / p);
            const double r = 0.5 + ga / 6.0;
            const double y = x < 0.5 ? x * 2.0 * r : r + (x - 0.5) * 2.0 * (1.0 - r);
            return (y - x) * p;
        }
        const double lb = 25.0 * ga / 1000.0 * tempo / 60.0;
        return grv == 3 ? lb : grv == 4 ? -lb : 0.0;
    }

    void line(float x0, float y0, float x1, float y1, uint32_t rgb, float a)
    {
        beginPath();
        moveTo(x0, y0);
        lineTo(x1, y1);
        strokeColor(col(rgb, a));
        strokeWidth(1.0f);
        stroke();
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float a)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, a));
        fill();
    }

    // as in humanize_gfx.jsfx: the latency and the settings; one bar of 16ths with the groove's
    // template (the straight 16ths above, where they go below, the random range around them) and
    // the last 16 notes (where they came in, where they go out, by velocity, the newest
    // brightest); the last note's move and velocity change
    void drawDisplay() override
    {
        static const char* const grooves[] = { "STRAIGHT", "SWING 8THS", "SWING 16THS", "LAID BACK", "PUSH" };
        const nullhumanize::View* v = view();
        const double tempo = v && v->tempo.load(std::memory_order_relaxed) > 0.0f ? double(v->tempo.load(std::memory_order_relaxed)) : 120.0;
        const float latMs = v ? v->latencyMs.load(std::memory_order_relaxed) : 0.0f;

        char l[64], r[96];
        std::snprintf(l, sizeof(l), "LATENCY %.1f MS", double(latMs));
        std::snprintf(r, sizeof(r), "%s %d%%   +/-%d MS   +/-%d%% VEL", grooves[std::max(0, std::min(4, iv(2)))], iv(3), iv(0), iv(1));
        label(24.0f, 70.0f, ALIGN_LEFT, 7.5f, 1.2f, theme::kMuted, l, true);
        label(456.0f, 70.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, r, true);

        for (int j = 0; j <= 16; ++j)
            box(30.0f + float(j) * 26.25f, 80.0f, 1.0f, 96.0f, j % 4 == 0 ? theme::kBorder : 0x2a2927, 1.0f);

        const double rb = iv(0) / 1000.0 * tempo / 60.0;
        for (int j = 0; j < 16; ++j)
        {
            const double b = j / 4.0, g = b + shift(b, tempo);
            const float xs = 30.0f + float(b * 105.0), xg = 30.0f + float(g * 105.0);
            box(xg - float(rb * 105.0), 90.0f, std::max(1.0f, float(2.0 * rb * 105.0)), 6.0f, theme::kAccent, 0.18f);
            line(xs, 84.0f, xg, 90.0f, theme::kMuted, 1.0f);
            beginPath();
            circle(xs, 83.0f, 1.5f);
            fillColor(col(theme::kMuted));
            fill();
            box(xg - 0.75f, 89.0f, 1.5f, 8.0f, theme::kAccent, 1.0f);
        }

        const int n = v ? std::min(nullhumanize::View::kNotes, v->count.load(std::memory_order_relaxed)) : 0;
        for (int k = 0; k < n; ++k)
        {
            const float in = v->in[k].load(std::memory_order_relaxed), out = v->out[k].load(std::memory_order_relaxed);
            const float vi = v->vel[k].load(std::memory_order_relaxed), vo = v->velOut[k].load(std::memory_order_relaxed);
            const float xi = 30.0f + std::min(4.2f, in) * 105.0f, xo = 30.0f + std::min(4.2f, out) * 105.0f;
            const float yi = 172.0f - vi / 127.0f * 66.0f, yo = 172.0f - vo / 127.0f * 66.0f;
            const float a = 0.3f + 0.7f * float(k + 1) / float(n);
            line(xi, yi, xo, yo, theme::kMuted, a);
            beginPath();
            circle(xi, yi, 2.0f);
            strokeColor(col(theme::kMuted, a));
            strokeWidth(1.0f);
            stroke();
            beginPath();
            circle(xo, yo, 2.5f);
            fillColor(col(theme::kAccent, a));
            fill();
        }
        char b2[96];
        if (n > 0)
        {
            const float in = v->in[n - 1].load(std::memory_order_relaxed), out = v->out[n - 1].load(std::memory_order_relaxed);
            const double ms = double(out - in) * 60000.0 / tempo - double(latMs);
            std::snprintf(b2, sizeof(b2), "LAST NOTE  %s%.1f MS   VELOCITY %d TO %d", ms >= 0.0 ? "+" : "", ms,
                          int(v->vel[n - 1].load(std::memory_order_relaxed)), int(v->velOut[n - 1].load(std::memory_order_relaxed)));
        }
        else
            std::snprintf(b2, sizeof(b2), "PLAY SOME NOTES");
        label(24.0f, 187.0f, ALIGN_LEFT, 7.5f, 1.2f, theme::kMuted, b2, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4)
            std::snprintf(buf, n, "%d", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIHumanize();
}

END_NAMESPACE_DISTRHO
