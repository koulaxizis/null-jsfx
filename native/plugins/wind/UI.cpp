/*
 * NULL JSFX - Wind (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As wind_gfx.jsfx: the display holds the breath over the last 100 frames, the vibrato after a
 * note over 2 s, the note sounding, the model and its range, and a keyboard from C2 to C7 with the
 * note sounding lit and the keys outside the model's range dim. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "WindDSP.hpp"
#include "WindSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODEL", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BREATH", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "NOISE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "TONE", 0, -1, false },
    { 11, NullKnobsUI::kKnob, "VOLUME", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "VIBRATO", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "RATE", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "DELAY", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "ATTACK", 1, -1, false, true },
    { 9, NullKnobsUI::kKnob, "RELEASE", 1, -1, false, true },
    { 10, NullKnobsUI::kKnob, "GLIDE", 1, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullwind::kNumParams];
    for (uint32_t i = 0; i < nullwind::kNumParams; ++i)
        s[i] = nullwind::kSliders[i];
    s[0].options = nullwind::kModelShort;
    s[1].options = nullwind::kSourceShort;
    return s;
}

static const char* const kRanges[] = { "FLUTE  C4-C7", "CLARINET  D3-G6", "SAXOPHONE  C#3-A5", "BRASS  E2-E6" };
static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class UIWind : public NullKnobsUI
{
public:
    UIWind()
        : NullKnobsUI({ "WIND", "INSTRUMENTS", 560, 156, nullptr, 2, uiSliders(), nullwind::kNumParams, kControls, kNumControls, true })
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
        double vals[nullwind::kNumParams];
        for (uint32_t i = 0; i < nullwind::kNumParams; ++i)
            vals[i] = jsfxValue(nullwind::kSliders[i], value(i));
        int note = -1;
        float breath = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            note = int(std::lround(t->aux[0].load(std::memory_order_relaxed)));
            breath = t->aux[1].load(std::memory_order_relaxed);
        }
        const int model = int(std::max(0.0, std::min(3.0, std::floor(vals[0] + 0.5))));
        int lo, hi;
        nullwind::modelRange(model, lo, hi);

        // the breath of the last 100 frames
        fHist[fHp] = breath;
        fHp = (fHp + 1) % 100;
        setPlot(24.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        float xs[201], ys[201];
        for (int j = 0; j < 100; ++j)
        {
            xs[j] = fPX + fPW * float(j) / 99.0f;
            ys[j] = linY(fHist[(fHp + j) % 100], 0.0f, 1.0f);
        }
        line(xs, ys, 100, 1.0f);
        label(30.0f, 72.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "BREATH", true);

        // the vibrato after a note, 2 s
        setPlot(284.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 2, false);
        const double vib = vals[5] / 40.0, rate = vals[6], del = vals[7] * 0.001;
        for (int j = 0; j <= 200; ++j)
        {
            const double t = j / 100.0;
            const double v = vib * std::max(0.0, std::min(1.0, (t - del) / 0.3)) * std::sin(2.0 * M_PI * rate * t);
            xs[j] = fPX + fPW * float(j) / 200.0f;
            ys[j] = linY(float(v), -2.5f, 2.5f);
        }
        line(xs, ys, 201, 1.0f);
        label(530.0f, 72.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "VIBRATO, 2 S", true);

        // the note sounding, the model and its range
        char info[32];
        if (note >= 0)
            std::snprintf(info, sizeof(info), "NOTE %s%d", kNames[note % 12], note / 12 - 1);
        else
            std::snprintf(info, sizeof(info), "NOTE -");
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        label(536.0f, 146.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, kRanges[model], true);

        // the keyboard from C2 to C7
        const float ky = 156.0f, kh = 44.0f, ww = 512.0f / 36.0f, bw = ww * 0.62f, bh = kh * 0.6f;
        int i = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (!white(n % 12))
                continue;
            const float x = 24.0f + float(i) * ww;
            const bool in = n >= lo && n <= hi;
            box(x + 0.5f, ky, ww - 1.0f, kh, n == note ? theme::kAccent : in ? 0x2d2b28 : 0x1c1b19);
            if (n % 12 == 0)
            {
                char b[8];
                std::snprintf(b, sizeof(b), "C%d", n / 12 - 1);
                label(x + ww * 0.5f, ky + kh - 5.0f, ALIGN_CENTER, 6.0f, 0.0f, n == note ? theme::kBackground : theme::kMuted, b, true);
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
            box(x, ky, bw, bh, n == note ? theme::kAccent : 0x131211);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 11)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 6)
            std::snprintf(buf, n, "%.1f Hz", v);
        else if (param == 7 || param == 10)
            std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        else if (param == 8 || param == 9)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f s", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    float fHist[100] = {};
    int fHp = 0;
};

UI* createUI()
{
    return new UIWind();
}

END_NAMESPACE_DISTRHO
