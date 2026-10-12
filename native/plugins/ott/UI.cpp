/*
 * NULL JSFX - OTT (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "OttDSP.hpp"
#include "OttSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TIME", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "UPWARD", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DOWNWARD", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "LOW", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "MID", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "HIGH", 1, -1, true },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIOtt : public NullKnobsUI
{
public:
    UIOtt()
        : NullKnobsUI({ "OTT", "ADVANCED", 480, 120, nullptr, 2, nullott::kSliders, nullott::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in ott_gfx.jsfx: one band's static curve on the left, the bands' gains on the right
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, 96.0f, 96.0f);
        grid(4, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-40");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-80");
        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        const double up = value(2) / 100.0, down = value(3) / 100.0;
        constexpr int n = 97;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double d = -80.0 + 80.0 * i / (n - 1);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(d + nullott::bandGainDb(d, up, down)), -80.0f, 0.0f);
        }
        curve(xs, ys, n);

        float g[3] = { 1.0f, 1.0f, 1.0f };
        if (nulljsfx::Taps* t = taps())
            for (int b = 0; b < 3; ++b)
                g[b] = t->scopeL[b];
        bar(72.0f, "LOW", g[0]);
        bar(101.0f, "MID", g[1]);
        bar(130.0f, "HIGH", g[2]);
    }

    // bipolar gain bar (-24..+24 dB) with label and value, like ott_bar in the GFX
    void bar(float y, const char* name, float gain)
    {
        const float db = 20.0f * std::log10(std::fmax(gain, 0.000001f));
        label(196.0f, y + 5.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted, name, true);
        const float x0 = 236.0f, x1 = float(kWidth) - 82.0f, xc = (x0 + x1) * 0.5f;
        beginPath();
        rect(x0, y, x1 - x0, 10.0f);
        fillColor(col(theme::kBorder));
        fill();
        const float xv = xc + (x1 - x0) * 0.5f * clampf(db / 24.0f, -1.0f, 1.0f);
        if (std::fabs(xv - xc) >= 0.5f)
        {
            beginPath();
            rect(std::fmin(xc, xv), y, std::fabs(xv - xc), 10.0f);
            fillColor(col(theme::kAccent));
            fill();
        }
        beginPath();
        rect(xc, y - 2.0f, 1.0f, 14.0f);
        fillColor(col(theme::kMuted));
        fill();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f dB", double(db));
        label(float(kWidth) - 24.0f, y + 5.0f, ALIGN_RIGHT, 8.5f, 0.0f, theme::kText, buf);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param >= 4)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIOtt();
}

END_NAMESPACE_DISTRHO
