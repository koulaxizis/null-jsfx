/*
 * NULL JSFX - Disperser (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DisperserDSP.hpp"
#include "DisperserSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "FREQ", 0, -1, false, true },
    { 1, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "PINCH", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "MODE", 0, -1, false },
};

class UIDisperser : public NullKnobsUI
{
public:
    UIDisperser()
        : NullKnobsUI({ "DISPERSER", "ESSENTIALS", 480, 120, nullptr, 1, nulldisperser::kSliders,
                        nulldisperser::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // delay label for v ms (1, 2, 5 x 10^e or half of one), as dp_label in disperser_gfx.jsfx
    static void delayLabel(char* buf, size_t n, double v)
    {
        if (v >= 1000.0)
        {
            if (v / 1000.0 == std::floor(v / 1000.0))
                std::snprintf(buf, n, "%ds", int(v / 1000.0));
            else
                std::snprintf(buf, n, "%.1fs", v / 1000.0);
        }
        else if (v == std::floor(v))
            std::snprintf(buf, n, "%dms", int(v));
        else
            std::snprintf(buf, n, "%.1fms", v);
    }

    // as in disperser_gfx.jsfx: the group delay of the chain from 20 Hz to 20 kHz on a scale that
    // fits its peak (Stereo: the right channel dimmer), Frequency dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nulldisperser::Params p;
        p.freq = nulldisperser::sliderValue(0, value(0));
        p.amount = nulldisperser::sliderValue(1, value(1));
        p.pinch = nulldisperser::sliderValue(2, value(2));
        p.mix = nulldisperser::sliderValue(3, value(3));
        p.mode = int(nulldisperser::sliderValue(4, value(4)));
        nulldisperser::Targets t;
        t.compute(p, sr);

        double pk = std::max(t.delayMs(std::exp(t.tl), t.tl, sr), t.delayMs(std::exp(t.tr), t.tr, sr));
        for (int j = 0; j < 200; ++j)
        {
            const double f = 20.0 * std::pow(1000.0, j / 199.0);
            pk = std::max(pk, std::max(t.delayMs(f, t.tl, sr), t.delayMs(f, t.tr, sr)));
        }
        double top = 1.0;
        int m = 1;
        while (top < pk * 1.05)
        {
            top *= m == 2 ? 2.5 : 2.0;
            m = m == 1 ? 2 : m == 2 ? 5 : 1;
        }

        // grid: frequency lines (decades brighter), 0 / half / top, frequency labels
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
                for (int mm = 1; mm <= 9; ++mm)
                {
                    const float f = float(mm) * d;
                    if (f < 20.0f || f > 20000.0f || (mm == 1) != (pass == 1))
                        continue;
                    const float x = std::round(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        const float ftop = float(top);
        for (int j = 0; j < 3; ++j)
        {
            const float y = std::round(linY(ftop * float(j) / 2.0f, 0.0f, ftop)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(j == 0 ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
            char buf[16];
            if (j == 0)
                std::snprintf(buf, sizeof(buf), "0");
            else
                delayLabel(buf, sizeof(buf), top * j / 2.0);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float capY = fPY + fPH + 11.0f;
        label(freqX(100.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");

        dashedV(freqX(float(p.freq)));

        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ys[512];
        if (p.mode > 0)
        {
            for (int i = 0; i < n; ++i)
            {
                xs[i] = fPX + float(i);
                ys[i] = linY(float(t.delayMs(20.0 * std::pow(1000.0, double(i) / double(fPW)), t.tr, sr)), 0.0f, ftop);
            }
            scissor(fPX, fPY, fPW, fPH);
            beginPath();
            moveTo(xs[0], ys[0]);
            for (int i = 1; i < n; ++i)
                lineTo(xs[i], ys[i]);
            strokeColor(col(theme::kAccent, 0.45f));
            strokeWidth(2.0f);
            lineJoin(ROUND);
            stroke();
            resetScissor();
        }
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + float(i);
            ys[i] = linY(float(t.delayMs(20.0 * std::pow(1000.0, double(i) / double(fPW)), t.tl, sr)), 0.0f, ftop);
        }
        curve(xs, ys, n, fPY + fPH);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            const double f = nulldisperser::sliderValue(0, v);
            if (f >= 1000.0)
                std::snprintf(buf, n, "%.2f kHz", f / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(f));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDisperser();
}

END_NAMESPACE_DISTRHO
