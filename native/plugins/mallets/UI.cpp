/*
 * NULL JSFX - Mallets (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As mallets_gfx.jsfx: the display holds the modes at the strike (C4, velocity 100), the decay
 * at C4 over 3 s, the voices, the key up and the motor, and a keyboard from C2 to C7 with the
 * keys sounding. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "MalletsDSP.hpp"
#include "MalletsSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODEL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "MALLET", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "STRIKE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "RESONATOR", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 8, NullKnobsUI::kKnob, "VELOCITY", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "KEY UP", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "DAMP TIME", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "MOTOR", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullmallets::kNumParams];
    for (uint32_t i = 0; i < nullmallets::kNumParams; ++i)
        s[i] = nullmallets::kSliders[i];
    s[0].options = nullmallets::kModelShort;
    s[5].options = nullmallets::kKeyUpShort;
    return s;
}

class UIMallets : public NullKnobsUI
{
public:
    UIMallets()
        : NullKnobsUI({ "MALLETS", "INSTRUMENTS", 560, 156, nullptr, 2, uiSliders(), nullmallets::kNumParams, kControls, kNumControls, true })
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

    // a bar of the modes at a frequency ratio (0.5 to 20) and level (dB)
    void bar(double r, double db, float alpha)
    {
        const float x = fBarX + fBarW * float(std::log(r / 0.5) / std::log(40.0));
        const float y = linY(float(db), -48.0f, 14.0f);
        box(x - 1.5f, y, 3.0f, fBarB - y, theme::kAccent, alpha);
    }

    void drawDisplay() override
    {
        double vals[nullmallets::kNumParams];
        for (uint32_t i = 0; i < nullmallets::kNumParams; ++i)
            vals[i] = std::round(value(i));
        nullmallets::View vw;
        vw.set(vals);

        // the modes at the strike
        setPlot(24.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        fBarX = 24.0f; fBarW = 252.0f; fBarB = 136.0f;
        for (int k = 0; k < nullmallets::NM; ++k)
        {
            const double l = vw.level(k);
            if (vw.mb[k] > 0 && l > 0.004)
                bar(vw.mb[k], 20 * std::log10(l), 1.0f);
        }
        label(30.0f, 72.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "MODES AT THE STRIKE", true);

        // the decay at C4
        setPlot(284.0f, 66.0f, 252.0f, 70.0f);
        grid(6, 3, false);
        double s0 = 0;
        for (int k = 0; k < nullmallets::NM; ++k)
            s0 += vw.level(k);
        float xs[61], ys[61];
        for (int i = 0; i <= 60; ++i)
        {
            const double t = i / 20.0;
            double a = 0;
            for (int k = 0; k < nullmallets::NM; ++k)
                if (vw.mb[k] > 0)
                    a += vw.level(k) * std::exp(-6.907755 * t / vw.decay(k));
            xs[i] = linX(float(t), 0.0f, 3.0f);
            ys[i] = linY(float(20 * std::log10(std::max(a / std::max(s0, 0.000001), 0.000001))), -60.0f, 0.0f);
        }
        curve(xs, ys, 61);
        label(530.0f, 72.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "DECAY AT C4, 3 S", true);

        // the voices, the key up and the motor
        bool on[128] = {};
        int voices = 0;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            voices = int(std::lround(t->extra.load(std::memory_order_relaxed)));
        }
        char info[64];
        std::snprintf(info, sizeof(info), "VOICES %d / 24", voices);
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        const char* ku = value(5) >= 0.5f ? "DAMP" : "RING";
        if (value(7) > 0.0f)
            std::snprintf(info, sizeof(info), "%s   MOTOR %.1f HZ", ku, value(7));
        else
            std::snprintf(info, sizeof(info), "%s   MOTOR OFF", ku);
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
        if (param == 10)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 7)
        {
            if (v > 0.0f)
                std::snprintf(buf, n, "%.1f Hz", v);
            else
                std::snprintf(buf, n, "OFF");
        }
        else if (param == 6)
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
    float fBarX = 0.0f, fBarW = 0.0f, fBarB = 0.0f;
};

UI* createUI()
{
    return new UIMallets();
}

END_NAMESPACE_DISTRHO
