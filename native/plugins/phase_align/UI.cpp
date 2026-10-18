/*
 * NULL JSFX - Phase Align (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PhaseAlignDSP.hpp"
#include "PhaseAlignSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "INVERT L", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "INVERT R", 0, -1, false },
    { 7, NullKnobsUI::kSelect, "LISTEN", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DELAY", 1, -1, true },
    { 2, NullKnobsUI::kKnob, "FINE", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "ROTATION", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "FOCUS", 1, -1, false, true },
};

class UIPhaseAlign : public NullKnobsUI
{
public:
    UIPhaseAlign()
        : NullKnobsUI({ "PHASE ALIGN", "ESSENTIALS", 480, 180, nullptr, 2, nullphase::kSliders, nullphase::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    double v(uint32_t i) const { return nullphase::sliderValue(i, value(i)); }

    nullphase::Params params() const
    {
        nullphase::Params p;
        p.sidechain = v(0) >= 0.5;
        p.delayMs = v(1);
        p.fine = v(2);
        p.invL = v(3) >= 0.5;
        p.invR = v(4) >= 0.5;
        p.rotation = v(5);
        p.focus = v(6);
        p.sum = v(7) >= 0.5;
        return p;
    }

    // as pg_bar in phase_align_gfx.jsfx: a -1..+1 bar filled from the centre, the value on the right
    void bar(float x, float y, float w, const char* name, float c)
    {
        label(x, y + 4.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, name, true);
        const float bx = x + 30.0f, bw = w - 64.0f, cx = bx + bw * 0.5f;
        beginPath();
        rect(bx, y, bw, 8.0f);
        fillColor(col(theme::kBorder));
        fill();
        if (c != 0.0f)
        {
            beginPath();
            if (c > 0.0f)
                rect(cx, y, bw * 0.5f * c, 8.0f);
            else
                rect(cx + bw * 0.5f * c, y, -bw * 0.5f * c, 8.0f);
            fillColor(col(theme::kAccent));
            fill();
        }
        beginPath();
        rect(cx - 0.5f, y - 2.0f, 1.0f, 12.0f);
        fillColor(col(theme::kText));
        fill();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%+.2f", double(c));
        label(x + w, y + 4.0f, ALIGN_RIGHT, 8.5f, 0.0f, theme::kText, buf);
    }

    // as the @gfx of phase_align_gfx.jsfx
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullphase::Params p = params();
        nullphase::Derived d;
        d.compute(p, sr);

        setPlot(58.0f, 70.0f, 252.0f, 132.0f);
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float dd = 10.0f; dd <= 10000.0f; dd *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * dd;
                    if (f < 20.0f || f > 20000.0f || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::round(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        for (int a = -180; a <= 180; a += 90)
        {
            const float y = std::round(linY(float(a), -180.0f, 180.0f)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(a == 0 ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
            if (a % 180 == 0)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), a > 0 ? "+%d" : "%d", a);
                label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
            }
        }
        const float capY = fPY + fPH + 11.0f;
        label(freqX(100.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");
        if (d.rot)
            dashedV(freqX(float(p.focus)));

        // the curve, broken where it wraps
        scissor(fPX, fPY, fPW, fPH + 1.0f);
        beginPath();
        const int n = int(fPW) + 1;
        double last = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double ph = nullphase::phaseDeg(p, d, sr, 20.0 * std::pow(1000.0, double(i) / double(fPW)));
            const float x = fPX + float(i), y = linY(float(ph), -180.0f, 180.0f);
            if (i == 0 || std::fabs(ph - last) > 180.0)
                moveTo(x, y);
            else
                lineTo(x, y);
            last = ph;
        }
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        resetScissor();

        // right column
        char buf[48];
        label(330.0f, 76.0f, ALIGN_LEFT, 7.0f, 1.5f, theme::kMuted, "TOTAL DELAY", true);
        label(330.0f, 150.0f, ALIGN_LEFT, 7.0f, 1.5f, theme::kMuted, "CORRELATION", true);
        std::snprintf(buf, sizeof(buf), "%+.2f ms", d.d * 1000.0 / sr);
        label(330.0f, 92.0f, ALIGN_LEFT, 10.0f, 0.5f, theme::kText, buf, true);
        std::snprintf(buf, sizeof(buf), "%+.2f samples", d.d);
        label(330.0f, 108.0f, ALIGN_LEFT, 7.5f, 0.0f, theme::kMuted, buf);
        if (p.sidechain)
            std::snprintf(buf, sizeof(buf), "Latency %d samples", d.lat);
        else
            std::snprintf(buf, sizeof(buf), "No latency");
        label(330.0f, 122.0f, ALIGN_LEFT, 7.5f, 0.0f, theme::kMuted, buf);
        float full = 0.0f, low = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            full = t->aux[0].load(std::memory_order_relaxed);
            low = t->aux[1].load(std::memory_order_relaxed);
        }
        bar(330.0f, 162.0f, 120.0f, "FULL", full);
        bar(330.0f, 182.0f, 120.0f, "LOW", low);
    }

    void formatValue(uint32_t param, float val, char* buf, size_t n) override
    {
        const double x = nullphase::sliderValue(param, val);
        switch (param)
        {
        case 1: std::snprintf(buf, n, "%+.2f ms", x); break;
        case 2: std::snprintf(buf, n, "%+.2f smp", x); break;
        case 5: std::snprintf(buf, n, "%d deg", int(std::lround(x))); break;
        case 6: std::snprintf(buf, n, "%d Hz", int(std::lround(x))); break;
        default: NullKnobsUI::formatValue(param, val, buf, n); break;
        }
    }
};

UI* createUI()
{
    return new UIPhaseAlign();
}

END_NAMESPACE_DISTRHO
