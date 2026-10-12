/*
 * NULL JSFX - Panner (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PannerDSP.hpp"
#include "PannerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "PAN", 0, -1, true },
    { 1, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "WIDTH", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "HAAS", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "OUTPUT", 0, -1, false },
};

class UIPanner : public NullKnobsUI
{
public:
    UIPanner()
        : NullKnobsUI({ "PANNER", "ESSENTIALS", 480, 130, nullptr, 1, nullpanner::kSliders, nullpanner::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    double v(uint32_t i) const { return nullpanner::sliderValue(i, value(i)); }

    // arc around (cx, cy) from a1 to a2 (radians, 0 = up, clockwise, as ng_arc)
    void arcStroke(float cx, float cy, float r, float a1, float a2, float th, uint32_t rgb, float alpha)
    {
        beginPath();
        arc(cx, cy, r, a1 - float(nullpanner::kPi) * 0.5f, a2 - float(nullpanner::kPi) * 0.5f, CW);
        strokeColor(col(rgb, alpha));
        strokeWidth(th);
        stroke();
    }

    void line(float x1, float y1, float x2, float y2, uint32_t rgb, float alpha)
    {
        beginPath();
        moveTo(x1, y1);
        lineTo(x2, y2);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    void disc(float x, float y, float r, uint32_t rgb)
    {
        beginPath();
        circle(x, y, r);
        fillColor(col(rgb));
        fill();
    }

    // dB text for a gain ("-INF" for silence), as pn_db in panner_gfx.jsfx
    static void dbText(char* buf, size_t n, double g)
    {
        if (g > 0.00001)
            std::snprintf(buf, n, "%+.1f dB", 20.0 * std::log10(g));
        else
            std::snprintf(buf, n, "-INF");
    }

    // as the @gfx of panner_gfx.jsfx: the stage seen from above (listener at the bottom, the
    // source on the arc at Pan with its spread lit), readouts, and a vectorscope of the output
    void drawDisplay() override
    {
        const float pi = float(nullpanner::kPi);
        const float cx = 177.0f, cy = 170.0f, r = 92.0f;
        arcStroke(cx, cy, r, -pi * 0.5f, pi * 0.5f, 1.0f, theme::kBorder, 1.0f);
        arcStroke(cx, cy, r * 0.55f, -pi * 0.5f, pi * 0.5f, 1.0f, theme::kBorder, 0.6f);
        for (int i = -2; i <= 2; ++i)
        {
            const float a = float(i) * pi / 4.0f;
            line(cx + std::sin(a) * 18.0f, cy - std::cos(a) * 18.0f, cx + std::sin(a) * r, cy - std::cos(a) * r,
                 i == 0 ? theme::kTrackHover : theme::kBorder, i == 0 ? 1.0f : 0.6f);
        }
        label(cx - r - 10.0f, cy, ALIGN_CENTER, 7.0f, 0.0f, theme::kMuted, "L", true);
        label(cx + r + 10.0f, cy, ALIGN_CENTER, 7.0f, 0.0f, theme::kMuted, "R", true);
        label(cx, cy - r - 13.0f, ALIGN_CENTER, 7.0f, 0.0f, theme::kMuted, "C", true);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const int mode = int(std::lround(v(1)));
        const double pan = v(0) / 100.0, a = std::fabs(pan), wid = v(2) / 100.0;
        const nullpanner::Targets t = nullpanner::targets(sr, v(0), mode, v(3), v(4));

        // the source: its spread (Width, narrower towards the sides) and its position
        const float pa = float(pan) * pi / 2.0f;
        const float sp = pi / 8.0f * float(wid) * (mode == 0 ? 1.0f : 1.0f - 0.5f * float(a));
        const float a1 = std::fmax(-pi * 0.5f, pa - sp), a2 = std::fmin(pi * 0.5f, pa + sp);
        if (a2 > a1)
            arcStroke(cx, cy, r, a1, a2, 5.0f, theme::kAccent, 0.45f);
        const float sx = cx + std::sin(pa) * r, sy = cy - std::cos(pa) * r;
        line(cx, cy, sx, sy, theme::kAccent, 0.18f);
        disc(sx, sy, 6.0f, theme::kAccent);
        disc(sx, sy, 2.5f, theme::kCard);

        // the listener: head, nose and ears
        beginPath();
        rect(cx - 13.0f, cy - 3.0f, 26.0f, 6.0f);
        moveTo(cx - 3.0f, cy - 9.0f);
        lineTo(cx + 3.0f, cy - 9.0f);
        lineTo(cx, cy - 14.0f);
        closePath();
        fillColor(col(theme::kMuted));
        fill();
        disc(cx, cy, 10.0f, theme::kMuted);
        disc(cx, cy, 8.5f, theme::kLogoTile);

        // readouts for the mode
        label(28.0f, 70.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, mode == 0 ? "BALANCE" : mode == 1 ? "NATURAL" : "HAAS", true);
        char d[32], buf[48];
        if (mode == 0)
        {
            dbText(d, sizeof(d), t.gl / t.outG);
            std::snprintf(buf, sizeof(buf), "L  %s", d);
            label(28.0f, 85.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);
            dbText(d, sizeof(d), t.gr / t.outG);
            std::snprintf(buf, sizeof(buf), "R  %s", d);
            label(28.0f, 98.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);
        }
        else if (mode == 1)
        {
            std::snprintf(buf, sizeof(buf), "ITD  %.2f ms", std::max(t.dl, t.dr) / sr * 1000.0);
            label(28.0f, 85.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);
            std::snprintf(buf, sizeof(buf), "ILD  %.1f dB", std::fabs(20.0 * std::log10(t.gl / t.gr)));
            label(28.0f, 98.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "DELAY  %.1f ms", std::max(t.dl, t.dr) / sr * 1000.0);
            label(28.0f, 85.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);
            dbText(d, sizeof(d), std::min(t.gl, t.gr) / t.outG);
            std::snprintf(buf, sizeof(buf), "FAR  %s", d);
            label(28.0f, 98.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);
        }

        setPlot(346.0f, 66.0f, 106.0f, 106.0f);
        vectorscope(2.0f);
    }

    void formatValue(uint32_t param, float val, char* buf, size_t n) override
    {
        const double x = nullpanner::sliderValue(param, val);
        if (param == 0)
        {
            const int p = int(std::lround(x));
            if (p == 0)
                std::snprintf(buf, n, "C");
            else
                std::snprintf(buf, n, p < 0 ? "L %d" : "R %d", std::abs(p));
        }
        else if (param == 3)
            std::snprintf(buf, n, "%.1f ms", x);
        else if (param == 4)
            std::snprintf(buf, n, "%+.1f dB", x);
        else
            NullKnobsUI::formatValue(param, val, buf, n);
    }
};

UI* createUI()
{
    return new UIPanner();
}

END_NAMESPACE_DISTRHO
