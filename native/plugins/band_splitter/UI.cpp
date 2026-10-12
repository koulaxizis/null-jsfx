/*
 * NULL JSFX - Band Splitter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BandSplitterDSP.hpp"
#include "BandSplitterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "BANDS", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "XOVER 1", 0, -1, false, true },
    { 3, NullKnobsUI::kKnob, "XOVER 2", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "XOVER 3", 0, -1, false, true },
    { 5, NullKnobsUI::kSelect, "LISTEN", 0, -1, false },
};

class UIBandSplitter : public NullKnobsUI
{
public:
    UIBandSplitter()
        : NullKnobsUI({ "BAND SPLITTER", "ESSENTIALS", 480, 120, nullptr, 1, nullbandsplit::kSliders,
                        nullbandsplit::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    static void hz(double f, char* buf, size_t n)
    {
        if (f < 1000.0)
            std::snprintf(buf, n, "%d Hz", int(std::floor(f + 0.5)));
        else
            std::snprintf(buf, n, "%.1f kHz", f / 1000.0);
    }

    // as in band_splitter_gfx.jsfx: each band's response (+12 to -42 dB) over frequency, the
    // flat sum dashed at 0 dB, the crossovers in use; Listen lights its band; Merge dims it all
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * d;
                    if (f < 20.0f || f > 20000.0f || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::floor(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        char buf[24];
        for (int i = 0; i < 4; ++i)
        {
            const float y = std::floor(linY(-12.0f * float(i), -42.0f, 12.0f)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(col(theme::kBorder, 0.6f));
            stroke();
            std::snprintf(buf, sizeof(buf), "%d", -12 * i);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const bool merge = value(0) >= 0.5f;
        const int nb = std::max(2, std::min(4, int(std::lround(value(1)))));
        const int listen = int(std::lround(value(5)));
        const double in[3] = { std::round(value(2)), std::round(value(3)), std::round(value(4)) };
        double xo[3];
        nullbandsplit::effectiveXovers(in, sr, xo);

        // the sum of the bands: flat, dashed at 0 dB
        {
            const float y = std::floor(linY(0.0f, -42.0f, 12.0f)) + 0.5f;
            beginPath();
            for (float x = fPX; x < fPX + fPW; x += 6.0f)
            {
                moveTo(x, y);
                lineTo(std::fmin(x + 3.0f, fPX + fPW), y);
            }
            strokeColor(col(theme::kAccent, merge ? 0.2f : 0.5f));
            strokeWidth(1.0f);
            stroke();
        }

        // the bands: a fill under the band Listen picks, then each curve (stopping where it
        // leaves the bottom of the plot)
        constexpr int n = 241;
        for (int b = 0; b < nb; ++b)
        {
            float xs[n], ys[n];
            bool vis[n];
            for (int i = 0; i < n; ++i)
            {
                const float t = float(i) / float(n - 1);
                const double f = 20.0 * std::pow(1000.0, double(t));
                const float db = nulljsfx::toDb(float(nullbandsplit::bandMag(b, nb, f, xo, sr)));
                xs[i] = fPX + fPW * t;
                ys[i] = linY(db, -42.0f, 12.0f);
                vis[i] = db > -42.0f;
            }
            if (listen == b + 1 && !merge)
            {
                beginPath();
                moveTo(xs[0], fPY + fPH);
                for (int i = 0; i < n; ++i)
                    lineTo(xs[i], ys[i]);
                lineTo(xs[n - 1], fPY + fPH);
                closePath();
                fillColor(col(theme::kAccent, 0.10f));
                fill();
            }
            const bool on = listen == 0 || listen == b + 1;
            beginPath();
            bool drawing = false;
            for (int i = 0; i < n; ++i)
            {
                if (vis[i])
                {
                    if (drawing)
                        lineTo(xs[i], ys[i]);
                    else
                        moveTo(xs[i], ys[i]);
                    drawing = true;
                }
                else
                {
                    if (drawing)
                        lineTo(xs[i], ys[i]);
                    drawing = false;
                }
            }
            strokeColor(col(theme::kAccent, merge ? 0.25f : (on ? 1.0f : 0.35f)));
            strokeWidth(2.0f);
            lineJoin(ROUND);
            stroke();
        }

        // crossover markers and their frequencies; the band numbers at the top
        float x0 = fPX;
        for (int k = 0; k < nb; ++k)
        {
            const float x1 = k < nb - 1 ? freqX(float(xo[k])) : fPX + fPW;
            if (k < nb - 1)
            {
                const float x = std::floor(x1) + 0.5f;
                beginPath();
                moveTo(x, fPY);
                lineTo(x, fPY + fPH);
                strokeColor(col(theme::kAccent, merge ? 0.2f : 0.6f));
                strokeWidth(1.0f);
                stroke();
                hz(xo[k], buf, sizeof(buf));
                label(x, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
            }
            if (!merge)
            {
                std::snprintf(buf, sizeof(buf), x1 - x0 >= 44.0f ? "BAND %d" : "%d", k + 1);
                label((x0 + x1) * 0.5f, fPY + 8.0f, ALIGN_CENTER, 7.0f, 0.0f,
                      listen == k + 1 ? theme::kText : theme::kMuted, buf, true);
            }
            x0 = x1;
        }
        if (merge)
            label(fPX + fPW * 0.5f, fPY + fPH * 0.5f + 10.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kText,
                  "MERGE: CHANNELS 1-8 SUMMED TO 1/2", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param >= 2 && param <= 4)
            hz(std::round(double(v)), buf, n);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIBandSplitter();
}

END_NAMESPACE_DISTRHO
