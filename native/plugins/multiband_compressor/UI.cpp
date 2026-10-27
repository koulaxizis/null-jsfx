/*
 * NULL JSFX - Multiband Comp (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "MultibandCompressorDSP.hpp"
#include "MultibandCompressorSliders.hpp"

START_NAMESPACE_DISTRHO

// a tab per band (threshold, ratio, makeup, mode and its upper crossover), then the shared
// attack, release, knee and output on every tab, as in multiband_compressor_gfx.jsfx
#define NULL_MB_BAND(b) \
    { 3 + 4 * b, NullKnobsUI::kKnob, "THRESHOLD", 0, b, false }, \
    { 4 + 4 * b, NullKnobsUI::kKnob, "RATIO", 0, b, false }, \
    { 5 + 4 * b, NullKnobsUI::kKnob, "MAKEUP", 0, b, false }, \
    { 6 + 4 * b, NullKnobsUI::kSelect, "MODE", 0, b, false }

static const NullKnobsUI::Control kControls[] = {
    NULL_MB_BAND(0), { 0, NullKnobsUI::kKnob, "XOVER", 0, 0, false },
    NULL_MB_BAND(1), { 1, NullKnobsUI::kKnob, "XOVER", 0, 1, false },
    NULL_MB_BAND(2), { 2, NullKnobsUI::kKnob, "XOVER", 0, 2, false },
    NULL_MB_BAND(3),
    { 19, NullKnobsUI::kKnob, "ATTACK", 1, -1, false },
    { 20, NullKnobsUI::kKnob, "RELEASE", 1, -1, false },
    { 21, NullKnobsUI::kKnob, "KNEE", 1, -1, false },
    { 22, NullKnobsUI::kKnob, "OUTPUT", 1, -1, false },
    { 23, NullKnobsUI::kSelect, "CHANNELS", 1, -1, false },
    { 24, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
};

#undef NULL_MB_BAND

class UIMultibandCompressor : public NullKnobsUI
{
public:
    UIMultibandCompressor()
        : NullKnobsUI({ "MULTIBAND COMP", "ESSENTIALS", 480, 120, "LOW|LOW MID|HIGH MID|HIGH", 2, nullmultiband::kSliders,
                        nullmultiband::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static void hz(double f, char* buf, size_t n)
    {
        if (f < 1000.0)
            std::snprintf(buf, n, "%d Hz", int(std::floor(f + 0.5)));
        else
            std::snprintf(buf, n, "%.1f kHz", f / 1000.0);
    }

    // band b between f1 and f2 Hz (mb_band in the GFX)
    void band(int b, double f1, double f2, float gr, int mode, bool anySolo, const char* name)
    {
        const float x1 = freqX(float(f1)), x2 = freqX(float(f2));
        const bool heard = mode != 2 && (!anySolo || mode == 3);
        const bool sel = b == page();
        if (sel)
        {
            beginPath();
            rect(x1, fPY, x2 - x1, fPH);
            fillColor(col(theme::kAccent, 0.06f));
            fill();
        }
        if (mode == 1)
            gr = 0.0f;
        const float y = linY(-gr, -24.0f, 0.0f);
        if (y > fPY + 0.5f)
        {
            beginPath();
            rect(x1 + 1.0f, fPY, x2 - x1 - 2.0f, y - fPY);
            fillColor(col(theme::kAccent, !heard ? 0.2f : (sel ? 0.8f : 0.45f)));
            fill();
        }
        if (x2 - x1 >= 44.0f)
        {
            const float cx = (x1 + x2) * 0.5f;
            char n[32], g[32];
            if (mode == 3)
                std::snprintf(n, sizeof(n), "%s SOLO", name);
            else
                std::snprintf(n, sizeof(n), "%s", name);
            label(cx, fPY + fPH - 22.0f, ALIGN_CENTER, 7.0f, 0.0f, sel ? theme::kText : theme::kMuted, n, true);
            if (mode == 1)
                std::snprintf(g, sizeof(g), "BYPASS");
            else if (mode == 2)
                std::snprintf(g, sizeof(g), "MUTE");
            else if (gr > 0.05f)
                std::snprintf(g, sizeof(g), "-%.1f dB", double(gr));
            else
                std::snprintf(g, sizeof(g), "0.0 dB");
            label(cx, fPY + fPH - 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, g);
        }
    }

    // as in multiband_compressor_gfx.jsfx: the four bands across frequency (the crossovers in
    // use) with each band's gain reduction hanging from the top (0 to 24 dB)
    void drawDisplay() override
    {
        setPlot(50.0f, 66.0f, 402.0f, 84.0f);
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
                    const float x = std::round(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        beginPath();
        for (int i = 0; i <= 4; ++i)
        {
            const float y = std::round(fPY + fPH * float(i) / 4.0f) + 0.5f;
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
        }
        strokeColor(col(theme::kBorder, 0.6f));
        stroke();
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-12");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const double in[3] = { std::round(value(0)), std::round(value(1)), std::round(value(2)) };
        double xo[3];
        nullmultiband::effectiveXovers(in, sr, xo);
        int mode[4];
        bool anySolo = false;
        for (int b = 0; b < 4; ++b)
        {
            mode[b] = int(std::lround(value(6 + 4 * uint32_t(b))));
            anySolo = anySolo || mode[b] == 3;
        }
        float gr[4] = {};
        if (nulljsfx::Taps* t = taps())
            for (int b = 0; b < 4; ++b)
                gr[b] = t->scopeL[b];
        static const char* const kNames[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const double edges[5] = { 20.0, xo[0], xo[1], xo[2], 20000.0 };
        for (int b = 0; b < 4; ++b)
            band(b, edges[b], edges[b + 1], gr[b], mode[b], anySolo, kNames[b]);
        for (int k = 0; k < 3; ++k)
        {
            const float x = std::floor(freqX(float(xo[k]))) + 0.5f;
            beginPath();
            moveTo(x, fPY);
            lineTo(x, fPY + fPH);
            strokeColor(col(theme::kAccent, 0.6f));
            stroke();
            char buf[16];
            hz(xo[k], buf, sizeof(buf));
            label(x, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param >= 3 && param < 19 && (param - 3) % 4 == 1)
            std::snprintf(buf, n, "%.1f:1", nullmultiband::tenth(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIMultibandCompressor();
}

END_NAMESPACE_DISTRHO
