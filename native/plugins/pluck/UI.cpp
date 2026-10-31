/*
 * NULL JSFX - Pluck (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As pluck_gfx.jsfx: the display holds the strum pattern (8 steps, the one playing outlined),
 * the body's response from 50 Hz to 5 kHz, the strings and the strum, and a keyboard from C2
 * to C7 with the keys held (marked) and the strings sounding (lit). Two rows of controls below.
 */

#include "NullUI.hpp"
#include "PluckDSP.hpp"
#include "PluckSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODEL", 0, -1, false },
    { 12, NullKnobsUI::kSelect, "PIZZICATO", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "PICK POSITION", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BRIGHTNESS", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "BODY", 0, -1, false },
    { 7, NullKnobsUI::kKnob, "VELOCITY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DECAY", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "RELEASE", 1, -1, false, true },
    { 6, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "STRUM", 2, -1, false },
    { 9, NullKnobsUI::kSelect, "STRUM RATE", 2, -1, false },
    { 10, NullKnobsUI::kKnob, "SPREAD", 2, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullpluck::kNumParams];
    for (uint32_t i = 0; i < nullpluck::kNumParams; ++i)
        s[i] = nullpluck::kSliders[i];
    s[0].options = nullpluck::kModelShort;
    s[8].options = nullpluck::kStrumShort;
    s[9].options = nullpluck::kRateShort;
    s[12].options = nullpluck::kPizzShort;
    return s;
}

class UIPluck : public NullKnobsUI
{
public:
    UIPluck()
        : NullKnobsUI({ "PLUCK", "INSTRUMENTS", 560, 156, nullptr, 3, uiSliders(), nullpluck::kNumParams, kControls, kNumControls, true })
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

    void outline(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f);
        strokeColor(col(rgb));
        strokeWidth(1.0f);
        stroke();
    }

    void tri(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t rgb, float alpha)
    {
        beginPath();
        moveTo(x1, y1);
        lineTo(x2, y2);
        lineTo(x3, y3);
        closePath();
        fillColor(col(rgb, alpha));
        fill();
    }

    void drawDisplay() override
    {
        const int pz = std::max(0, std::min(2, int(std::lround(value(12)))));
        const int model = pz > 0 ? 7 + pz : std::max(0, std::min(7, int(std::lround(value(0)))));
        const double* mb = nullpluck::kModels[model];
        const double bm = std::round(value(5)) / 100;
        const int mode = std::max(0, std::min(5, int(std::lround(value(8)))));
        const int rate = std::max(0, std::min(3, int(std::lround(value(9)))));

        bool on[128] = {}, held[128] = {};
        int step = -1, strings = 0;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
            {
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
                held[n] = (uint32_t(t->aux[8 + n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            }
            const int e = int(std::lround(t->extra.load(std::memory_order_relaxed)));
            strings = e / 16;
            step = e % 16 - 1;
        }

        // the strum pattern
        if (mode > 0)
        {
            for (int i = 0; i < 8; ++i)
            {
                const float x = 24.0f + float(i) * 64.57f;
                const int c = nullpluck::kPatterns[mode][i];
                box(x, 66.0f, 60.0f, 30.0f, theme::kBackground);
                outline(x, 66.0f, 60.0f, 30.0f, i == step ? theme::kText : theme::kBorder);
                if (i == step)
                    outline(x + 1.0f, 67.0f, 58.0f, 28.0f, theme::kText);
                if (c == 1)
                    tri(x + 22.0f, 75.0f, x + 38.0f, 75.0f, x + 30.0f, 88.0f, theme::kAccent, 1.0f);
                else if (c == 2)
                    tri(x + 22.0f, 88.0f, x + 38.0f, 88.0f, x + 30.0f, 75.0f, theme::kAccent, 0.75f);
                else
                    box(x + 25.0f, 80.0f, 10.0f, 2.0f, theme::kMuted, 0.6f);
                char b[4];
                std::snprintf(b, sizeof(b), "%d", i + 1);
                label(x + 5.0f, 72.0f, ALIGN_LEFT, 6.0f, 0.0f, theme::kMuted, b, true);
            }
        }
        else
        {
            box(24.0f, 66.0f, 512.0f, 30.0f, theme::kBackground);
            outline(24.0f, 66.0f, 512.0f, 30.0f, theme::kBorder);
            label(280.0f, 81.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted, "STRUM OFF: EVERY KEY PLUCKS ITS STRING", true);
        }

        // the body: three analog band-passes and the dry sound
        setPlot(24.0f, 102.0f, 512.0f, 36.0f);
        grid(8, 2, false);
        const double bDry = 1 - 0.5 * bm, bWet = 1.5 * bm;
        float xs[97], ys[97];
        for (int i = 0; i <= 96; ++i)
        {
            const double f = 50 * std::pow(100.0, i / 96.0);
            double re = bDry, im = 0;
            for (int j = 0; j < 3; ++j)
            {
                const double q = f / mb[6 + j * 3], a = q / mb[7 + j * 3];
                const double d = (1 - q * q) * (1 - q * q) + a * a;
                re += bWet * mb[8 + j * 3] * a * a / d;
                im += bWet * mb[8 + j * 3] * a * (1 - q * q) / d;
            }
            xs[i] = 24.0f + 512.0f * float(i) / 96.0f;
            ys[i] = linY(float(10 * std::log10(std::max(re * re + im * im, 0.000001))), -12.0f, 12.0f);
        }
        curve(xs, ys, 97);
        label(30.0f, 108.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "BODY", true);
        label(530.0f, 108.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "50 HZ - 5 KHZ", true);

        // the strings and the strum
        char info[64];
        std::snprintf(info, sizeof(info), "STRINGS %d / 12", strings);
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        if (mode > 0)
            std::snprintf(info, sizeof(info), "STRUM %s %s, %d MS", nullpluck::kStrumShort[mode], nullpluck::kRateShort[rate], int(std::lround(value(10))));
        else
            std::snprintf(info, sizeof(info), "STRUM OFF");
        label(536.0f, 146.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, info, true);

        // the keyboard from C2 to C7
        const float ky = 156.0f, kh = 44.0f, ww = 512.0f / 36.0f, bw = ww * 0.62f, bh = kh * 0.6f;
        int i = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (!white(n % 12))
                continue;
            const float x = 24.0f + float(i) * ww;
            box(x + 0.5f, ky, ww - 1.0f, kh, on[n] ? theme::kAccent : 0x2d2b28);
            if (held[n])
                box(x + 2.5f, ky + 2.0f, ww - 5.0f, 3.0f, on[n] ? theme::kBackground : theme::kText);
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
            if (held[n])
                box(x + 1.5f, ky + 2.0f, bw - 3.0f, 3.0f, on[n] ? theme::kBackground : theme::kText);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 11)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 10)
            std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        else if (param == 4)
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
    return new UIPluck();
}

END_NAMESPACE_DISTRHO
