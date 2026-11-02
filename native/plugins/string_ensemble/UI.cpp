/*
 * NULL JSFX - String Ensemble (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As string_ensemble_gfx.jsfx: the display holds the tone (the low-pass of Brightness and the
 * body), the ensemble's three delays over 2 s, the keys sounding and the mode, and a keyboard
 * from C2 to C7 with the keys sounding. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "StringEnsembleDSP.hpp"
#include "StringEnsembleSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 2, NullKnobsUI::kKnob, "CELLOS 16'", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "VIOLAS 8'", 0, -1, false },
    { 0, NullKnobsUI::kKnob, "VIOLINS 4'", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "ATTACK", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "RELEASE", 0, -1, false, true },
    { 5, NullKnobsUI::kKnob, "BRIGHTNESS", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "ENSEMBLE", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "MODE", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullstrings::kNumParams];
    for (uint32_t i = 0; i < nullstrings::kNumParams; ++i)
        s[i] = nullstrings::kSliders[i];
    s[7].options = nullstrings::kModeShort;
    return s;
}

class UIStringEnsemble : public NullKnobsUI
{
public:
    UIStringEnsemble()
        : NullKnobsUI({ "STRING ENSEMBLE", "INSTRUMENTS", 560, 156, nullptr, 2, uiSliders(), nullstrings::kNumParams, kControls, kNumControls, true })
    {
    }

protected:
    static bool white(int pc) { return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11; }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    void line(const float* xs, const float* ys, int n, float alpha)
    {
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kAccent, alpha));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
    }

    void drawDisplay() override
    {
        double vals[nullstrings::kNumParams];
        for (uint32_t i = 0; i < nullstrings::kNumParams; ++i)
            vals[i] = jsfxValue(nullstrings::kSliders[i], value(i));
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nullstrings::View vw;
        vw.set(vals, sr);

        // the tone, 20 Hz to 20 kHz, -24 to +12 dB
        setPlot(24.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        float xs[101], ys[101];
        for (int j = 0; j <= 100; ++j)
        {
            const double f = 20.0 * std::pow(1000.0, j / 100.0);
            const double w = 2.0 * M_PI * std::min(f, 0.4999 * sr) / sr;
            const double t = std::tan(w / 2.0) / vw.sg;
            const double m = 1.0 / std::sqrt(1.0 + t * t * t * t) * vw.pk1.mag(w) * vw.pk2.mag(w);
            xs[j] = fPX + fPW * float(j) / 100.0f;
            ys[j] = linY(float(20.0 * std::log10(std::max(m, 0.0000001))), -24.0f, 12.0f);
        }
        line(xs, ys, 101, 1.0f);
        label(30.0f, 72.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "TONE", true);

        // the ensemble's three delays over 2 s (2 to 14 ms)
        setPlot(284.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        for (int k = 0; k < 3; ++k)
        {
            for (int j = 0; j <= 100; ++j)
            {
                const double t = j / 50.0;
                const double d = 8.0 + vw.eDsMs * std::sin(2.0 * M_PI * (t * vw.eFs + k / 3.0)) +
                                 vw.eDfMs * std::sin(2.0 * M_PI * (t * vw.eFf + k / 3.0));
                xs[j] = fPX + fPW * float(j) / 100.0f;
                ys[j] = linY(float(d), 2.0f, 14.0f);
            }
            line(xs, ys, 101, k == 1 ? 1.0f : 0.55f);
        }
        label(530.0f, 72.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "ENSEMBLE DELAYS, 2 S", true);

        // the keys sounding and the mode
        bool on[128] = {};
        int keys = 0;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            keys = int(std::lround(t->extra.load(std::memory_order_relaxed)));
        }
        char info[32];
        std::snprintf(info, sizeof(info), "KEYS %d", keys);
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        const int mode = int(std::max(0.0, std::min(2.0, std::floor(vals[7] + 0.5))));
        label(536.0f, 146.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, nullstrings::kModeShort[mode], true);

        // the keyboard from C2 to C7
        const float ky = 156.0f, kh = 44.0f, ww = 512.0f / 36.0f, bw = ww * 0.62f, bh = kh * 0.6f;
        int i = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (!white(n % 12))
                continue;
            const float x = 24.0f + float(i) * ww;
            box(x + 0.5f, ky, ww - 1.0f, kh, on[n] ? theme::kAccent : 0x2d2b28);
            if (n % 12 == 0)
            {
                char b[8];
                std::snprintf(b, sizeof(b), "C%d", n / 12 - 1);
                label(x + ww * 0.5f, ky + kh - 5.0f, ALIGN_CENTER, 6.0f, 0.0f, on[n] ? theme::kBackground : theme::kMuted, b, true);
            }
            ++i;
        }
        i = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (white(n % 12))
            {
                ++i;
                continue;
            }
            const float x = 24.0f + float(i) * ww - bw * 0.5f;
            box(x - 1.0f, ky, bw + 2.0f, bh + 1.0f, theme::kCard);
            box(x, ky, bw, bh, on[n] ? theme::kAccent : 0x131211);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 8)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 3 || param == 4)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f s", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIStringEnsemble();
}

END_NAMESPACE_DISTRHO
