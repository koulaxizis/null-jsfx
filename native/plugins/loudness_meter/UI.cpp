/*
 * NULL JSFX - Loudness Meter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "LoudnessMeterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SCALE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "HISTORY", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "AUTO RESET", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "RESET", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "MONITOR", 0, -1, false },
};

class UILoudnessMeter : public NullKnobsUI
{
public:
    UILoudnessMeter()
        : NullKnobsUI({ "LOUDNESS METER", "ADVANCED", 600, 240, nullptr, 1, nullloud::kSliders, nullloud::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nullloud::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<LoudnessMeterBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    // as lm_fmt in loudness_meter_gfx.jsfx
    static void fmt(char* buf, size_t n, float v)
    {
        if (v <= -100.0f)
            std::snprintf(buf, n, "-INF");
        else if (v > 0.05f)
            std::snprintf(buf, n, "+%.1f", double(v));
        else
            std::snprintf(buf, n, "%.1f", double(v));
    }

    void tile(float x, float y, float w, float h, const char* name, const char* value, const char* unit, bool big)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 4.0f);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x + 8.0f, y + 11.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, name, true);
        label(x + w - 8.0f, y + 11.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, unit, true);
        label(x + 8.0f, y + (big ? 38.0f : 31.0f), ALIGN_LEFT, big ? 26.0f : 15.0f, 0.0f, theme::kText, value, true);
    }

    // as the @gfx of loudness_meter_gfx.jsfx: readouts on the left, loudness history on the right
    void drawDisplay() override
    {
        const nullloud::View* v = view();
        const float mI = v ? v->integrated.load(std::memory_order_relaxed) : -200.0f;
        const float mM = v ? v->m.load(std::memory_order_relaxed) : -200.0f;
        const float mS = v ? v->s.load(std::memory_order_relaxed) : -200.0f;
        const float lra = v ? v->lra.load(std::memory_order_relaxed) : 0.0f;
        const float tp = v ? v->tpMax.load(std::memory_order_relaxed) : 0.0f;
        const float mMax = v ? v->mMax.load(std::memory_order_relaxed) : -200.0f;
        const float sMax = v ? v->sMax.load(std::memory_order_relaxed) : -200.0f;
        const int target = nullloud::targetLufs(value(0));

        char buf[64];
        fmt(buf, sizeof(buf), mI);
        tile(24.0f, 62.0f, 180.0f, 72.0f, "INTEGRATED", buf, "LUFS", true);
        if (mI > -100.0f)
        {
            float d = mI - float(target);
            if (std::fabs(d) < 0.05f)
                d = 0.0f;
            std::snprintf(buf, sizeof(buf), d > 0.0f ? "TARGET %d   +%.1f LU" : "TARGET %d   %.1f LU", target, double(d));
        }
        else
            std::snprintf(buf, sizeof(buf), "TARGET %d LUFS", target);
        label(32.0f, 122.0f, ALIGN_LEFT, 7.5f, 0.5f, theme::kMuted, buf);
        fmt(buf, sizeof(buf), mM);
        tile(24.0f, 138.0f, 88.0f, 46.0f, "MOMENTARY", buf, "", false);
        fmt(buf, sizeof(buf), mS);
        tile(116.0f, 138.0f, 88.0f, 46.0f, "SHORT-TERM", buf, "", false);
        std::snprintf(buf, sizeof(buf), "%.1f", double(lra));
        tile(24.0f, 188.0f, 88.0f, 46.0f, "LRA", buf, "LU", false);
        fmt(buf, sizeof(buf), tp > 0.0f ? 20.0f * std::log10(tp) : -200.0f);
        tile(116.0f, 188.0f, 88.0f, 46.0f, "TP MAX", buf, "DBTP", false);
        fmt(buf, sizeof(buf), mMax);
        tile(24.0f, 238.0f, 88.0f, 46.0f, "MAX M", buf, "", false);
        fmt(buf, sizeof(buf), sMax);
        tile(116.0f, 238.0f, 88.0f, 46.0f, "MAX S", buf, "", false);

        // history on the EBU +9 / +18 scale around the target
        const bool wide = value(1) >= 0.5f;
        const float hi = float(target + (wide ? 18 : 9)), lo = float(target - (wide ? 36 : 18));
        const int st = wide ? 6 : 3;
        setPlot(240.0f, 68.0f, 330.0f, 196.0f);
        grid(5, 9, false);
        for (int i = 0; i < 10; ++i)
        {
            const int db = int(lo) + i * st;
            const float y = std::round(linY(float(db), lo, hi)) + 0.5f;
            if (db == target)
            {
                beginPath();
                moveTo(fPX, y);
                lineTo(fPX + fPW, y);
                strokeColor(col(theme::kTrackHover));
                strokeWidth(1.0f);
                stroke();
            }
            std::snprintf(buf, sizeof(buf), "%d", db);
            label(fPX - 5.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, db == target ? theme::kText : theme::kMuted, buf);
        }
        const int hsel = int(std::lround(value(2)));
        const int secs = hsel == 0 ? 10 : hsel == 1 ? 30 : hsel == 2 ? 60 : 300;
        for (int c = 0; c <= 5; ++c)
        {
            const int a = secs * (5 - c) / 5;
            if (a == 0)
                std::snprintf(buf, sizeof(buf), "NOW");
            else if (a >= 60)
                std::snprintf(buf, sizeof(buf), "-%d MIN", a / 60);
            else
                std::snprintf(buf, sizeof(buf), "-%d S", a);
            label(fPX + fPW * float(c) / 5.0f, fPY + fPH + 10.0f, c == 0 ? ALIGN_LEFT : c == 5 ? ALIGN_RIGHT : ALIGN_CENTER,
                  8.0f, 0.0f, theme::kMuted, buf);
        }
        if (mI > lo && mI < hi)
        {
            const float y = std::round(linY(mI, lo, hi)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(col(theme::kText, 0.45f));
            strokeWidth(1.0f);
            stroke();
        }
        if (!v)
            return;

        // n points over the width (oldest left, newest right); more points than columns are
        // reduced to the loudest per column. Pass 0 = momentary, 1 = short-term.
        const int np = secs * 10, cols = 330;
        const int hw = v->hw.load(std::memory_order_acquire), hn = v->hn.load(std::memory_order_relaxed);
        for (int pass = 0; pass < 2; ++pass)
        {
            const float* buf2 = pass ? v->histS : v->histM;
            auto at = [&](int i) { int k = hw - np + i; if (k < 0) k += nullloud::kHist; return buf2[k]; };
            beginPath();
            bool first = true;
            auto point = [&](float x, float val) {
                const float y = linY(val, lo, hi);
                if (first) moveTo(x, y); else lineTo(x, y);
                first = false;
            };
            if (np <= cols)
            {
                for (int i = std::max(0, np - hn); i < np; ++i)
                    point(fPX + fPW * float(i) / float(np - 1), at(i));
            }
            else
            {
                for (int c = 0; c < cols; ++c)
                {
                    int i = c * np / cols;
                    const int i1 = (c + 1) * np / cols;
                    i = std::max(i, np - hn);
                    if (i >= i1)
                        continue;
                    float m = -200.0f;
                    for (; i < i1; ++i)
                        m = std::fmax(m, at(i));
                    point(fPX + fPW * (float(c) + 0.5f) / float(cols), m);
                }
            }
            if (pass)
            {
                strokeColor(col(theme::kAccent));
                strokeWidth(2.0f);
            }
            else
            {
                strokeColor(col(theme::kMuted, 0.7f));
                strokeWidth(1.0f);
            }
            lineJoin(ROUND);
            stroke();
        }
    }
};

UI* createUI()
{
    return new UILoudnessMeter();
}

END_NAMESPACE_DISTRHO
