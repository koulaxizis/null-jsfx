/*
 * NULL JSFX - Resonance Suppressor (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ResonanceSuppressorDSP.hpp"
#include "ResonanceSuppressorSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SHARPNESS", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SELECTIVITY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "ATTACK", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "RELEASE", 0, -1, false, true },
    { 5, NullKnobsUI::kKnob, "LOW", 1, -1, false, true },
    { 6, NullKnobsUI::kKnob, "HIGH", 1, -1, false, true },
    { 7, NullKnobsUI::kSelect, "MODE", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "DELTA", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIResonanceSuppressor : public NullKnobsUI
{
public:
    UIResonanceSuppressor()
        : NullKnobsUI({ "RESONANCE SUPPRESSOR", "ADVANCED", 480, 160, nullptr, 2, nullrsp::kSliders, nullrsp::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true })
    {
        std::fill(fDisp, fDisp + nullrsp::kNB, -120.0f);
    }

protected:
    // the largest value of arr over the bins under column c of cols (20 Hz..20 kHz), or the two
    // nearest bins interpolated where a column holds none (rs_col in the GFX JSFX)
    static float column(const float* arr, int c, int cols, double sr)
    {
        const int n = nullrsp::kN;
        const double f0 = 20.0 * std::pow(1000.0, double(c) / cols), f1 = 20.0 * std::pow(1000.0, double(c + 1) / cols);
        const double k0 = f0 * n / sr, k1 = std::min(double(n / 2), f1 * n / sr);
        int a = int(std::ceil(k0));
        const int b = int(std::floor(k1));
        if (a <= b)
        {
            float v = arr[a];
            while (a < b)
                v = std::max(v, arr[++a]);
            return v;
        }
        const double kc = std::min(double(n / 2 - 1), (k0 + k1) / 2);
        const int ci = int(std::floor(kc));
        return float(arr[ci] + (arr[ci + 1] - arr[ci]) * (kc - ci));
    }

    // as in resonance_suppressor_gfx.jsfx: the live spectrum (muted, 0 to -96 dB, left scale) with
    // the reduction per frequency hanging from the top (accent, 0 to 12 dB, right scale); Low/High
    // Freq dashed, the most reduction at the top right
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 52.0f, fVH - 20.0f);

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
        char buf[32];
        for (int i = 0; i <= 4; ++i)
        {
            const float y = std::round(fPY + fPH * float(i) / 4.0f) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(col(theme::kBorder, 0.6f));
            stroke();
            std::snprintf(buf, sizeof(buf), "%d", -24 * i);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
            if (i > 0 && i < 4)
            {
                std::snprintf(buf, sizeof(buf), "-%d", 3 * i);
                label(fPX + fPW + 6.0f, y, ALIGN_LEFT, 8.0f, 0.0f, theme::kAccent, buf);
            }
        }
        const float capY = fPY + fPH + 11.0f;
        label(freqX(100.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");
        dashedV(freqX(value(5)));
        dashedV(freqX(value(6)));

        // the levels for the display: up at once, down by 20% of the way per frame
        static float zero[nullrsp::kNB] = {};
        const float* lev = zero;
        const float* gr = zero;
        if (nulljsfx::Taps* t = taps())
        {
            lev = t->scopeL;
            gr = t->scopeR;
        }
        for (int k = 0; k < nullrsp::kNB; ++k)
        {
            const float l = lev == zero ? -120.0f : lev[k];
            fDisp[k] += (l - fDisp[k]) * (l > fDisp[k] ? 1.0f : 0.2f);
        }

        constexpr int cols = 380;
        float xs[cols], ys[cols];
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(fPX, fPY + fPH);
        for (int c = 0; c < cols; ++c)
        {
            xs[c] = fPX + fPW * (float(c) + 0.5f) / float(cols);
            ys[c] = linY(column(fDisp, c, cols, sr), -96.0f, 0.0f);
            lineTo(xs[c], ys[c]);
        }
        lineTo(fPX + fPW, fPY + fPH);
        closePath();
        fillColor(col(theme::kMuted, 0.15f));
        fill();
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int c = 1; c < cols; ++c)
            lineTo(xs[c], ys[c]);
        strokeColor(col(theme::kMuted, 0.8f));
        strokeWidth(1.0f);
        stroke();
        resetScissor();

        for (int c = 0; c < cols; ++c)
            ys[c] = fPY + fPH * std::min(1.0f, column(gr, c, cols, sr) / 12.0f);
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(fPX, fPY);
        for (int c = 0; c < cols; ++c)
            lineTo(xs[c], ys[c]);
        lineTo(fPX + fPW, fPY);
        closePath();
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        resetScissor();
        curve(xs, ys, cols);

        float mx = 0.0f;
        for (int k = 1; k < nullrsp::kNB; ++k)
            mx = std::max(mx, gr[k]);
        std::snprintf(buf, sizeof(buf), "MAX GR %.1f dB", double(mx));
        label(fPX + fPW - 6.0f, fPY + 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        if (value(8) >= 0.5f)
            label(fPX + 6.0f, fPY + 8.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "DELTA");
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else if (param == 5 && v >= 1000.0f)
            std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
        else if (param == 6)
            std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    float fDisp[nullrsp::kNB];
};

UI* createUI()
{
    return new UIResonanceSuppressor();
}

END_NAMESPACE_DISTRHO
