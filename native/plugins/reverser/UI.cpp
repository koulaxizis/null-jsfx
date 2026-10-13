/*
 * NULL JSFX - Reverser (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ReverserDSP.hpp"
#include "ReverserSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "LENGTH", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "FILTER", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "FREQ", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIReverser : public NullKnobsUI
{
public:
    UIReverser()
        : NullKnobsUI({ "REVERSER", "ADVANCED", 480, 120, nullptr, 1, nullreverser::kSliders, nullreverser::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    void dashedSegment(float x, float y0, float y1)
    {
        x = std::round(x) + 0.5f;
        beginPath();
        for (float y = y0; y < y1; y += 6.0f)
        {
            moveTo(x, y);
            lineTo(x, std::fmin(y + 3.0f, y1));
        }
        strokeColor(col(theme::kText, 0.8f));
        strokeWidth(1.0f);
        stroke();
    }

    // as in reverser_gfx.jsfx: left the buffer, written forwards (solid head) while it plays
    // backwards (dashed head); right the response of the filter on the reversed signal
    void drawDisplay() override
    {
        const float bx = 34.0f, by = 84.0f, bw = 220.0f, bh = 40.0f;
        char buf[24];
        label(bx, by - 10.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "BUFFER");
        std::snprintf(buf, sizeof(buf), "%.1f s", value(0));
        label(bx + bw, by - 10.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        float w = 0.0f, r = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            w = clampf(t->extra.load(std::memory_order_relaxed), 0.0f, 1.0f);
            r = clampf(t->gr.load(std::memory_order_relaxed), 0.0f, 1.0f);
        }
        beginPath();
        rect(bx, by, bw, bh);
        fillColor(col(theme::kBackground));
        fill();
        beginPath();
        rect(bx, by, bw * w, bh);
        fillColor(col(theme::kAccent, 0.10f));
        fill();
        beginPath();
        rect(bx + 0.5f, by + 0.5f, bw - 1.0f, bh - 1.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        beginPath();
        rect(std::round(bx + bw * w), by, 1.0f, bh);
        fillColor(col(theme::kAccent));
        fill();
        dashedSegment(bx + bw * r, by, by + bh);

        const float ly = by + bh + 14.0f;
        beginPath();
        rect(bx, ly - 1.0f, 10.0f, 2.0f);
        fillColor(col(theme::kAccent));
        fill();
        label(bx + 14.0f, ly, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "WRITE");
        beginPath();
        rect(bx + 60.0f, ly - 1.0f, 3.0f, 2.0f);
        rect(bx + 66.0f, ly - 1.0f, 3.0f, 2.0f);
        fillColor(col(theme::kText, 0.8f));
        fill();
        label(bx + 74.0f, ly, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "READ (REVERSED)");

        setPlot(300.0f, 70.0f, 146.0f, 76.0f);
        gridFreq(24.0f);
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullreverser::Coeffs c = nullreverser::coeffs(int(std::lround(value(1))), value(2), sr);
        constexpr int n = 147;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullreverser::responseDb(c, f, sr)), -16.0f, 16.0f);
        }
        curve(xs, ys, n);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f s", v);
        else if (param == 2)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIReverser();
}

END_NAMESPACE_DISTRHO
