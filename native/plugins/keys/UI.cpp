/*
 * NULL JSFX - Keys (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As keys_gfx.jsfx: the display holds the spectrum at the strike (the pitch, its FM harmonics
 * and the model's partials), the decay at C4 over 3 s, the voices and the tremolo, and a
 * keyboard from C2 to C7 with the keys sounding. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "KeysDSP.hpp"
#include "KeysSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODEL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TONE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BELL", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "RELEASE", 0, -1, false, true },
    { 5, NullKnobsUI::kKnob, "VELOCITY", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "TREM RATE", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "TREM DEPTH", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "TREMOLO", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "DRIVE", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "CHORUS", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
    { 12, NullKnobsUI::kSelect, "MPE", 2, -1, false },
    { 13, NullKnobsUI::kKnob, "MPE BEND", 2, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullkeys::kNumParams];
    for (uint32_t i = 0; i < nullkeys::kNumParams; ++i)
        s[i] = nullkeys::kSliders[i];
    s[0].options = nullkeys::kModelShort;
    s[8].options = nullkeys::kTremShort;
    s[12].options = nullkeys::kMpeShort;
    return s;
}

class UIKeys : public NullKnobsUI
{
public:
    UIKeys()
        : NullKnobsUI({ "KEYS", "INSTRUMENTS", 560, 156, nullptr, 3, uiSliders(), nullkeys::kNumParams, kControls, kNumControls, true })
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

    // a bar of the strike spectrum at a frequency ratio (0.5 to 20) and level (dB)
    void bar(double r, double db, float alpha)
    {
        const float x = fBarX + fBarW * float(std::log(r / 0.5) / std::log(40.0));
        const float y = linY(float(db), -48.0f, 14.0f);
        box(x - 1.5f, y, 3.0f, fBarB - y, theme::kAccent, alpha);
    }

    void drawDisplay() override
    {
        const int model = std::max(0, std::min(8, int(std::lround(value(0)))));
        const double* mb = nullkeys::kModels[model];
        const double tone = std::round(value(1)), bell = std::round(value(2)), dsc = std::round(value(3)) / 100;
        const double toneK = std::pow(2.0, (tone - 50) / 25), toneP = 0.4 + 0.012 * tone, bellK = bell / 50;

        // the spectrum at the strike
        setPlot(24.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        fBarX = 24.0f; fBarW = 252.0f; fBarB = 136.0f;
        const double ix = mb[1] * toneK;
        double a = 1;
        for (int k = 1; k <= 8; ++k)
        {
            const double db = 20 * std::log10(std::max(a, 0.000001));
            if (db > -48)
                bar(k, db, 0.55f);
            a *= ix / 2 / k;
        }
        for (int j = 0; j < 3; ++j)
        {
            const double r = mb[7 + j * 3], l = mb[8 + j * 3] * bellK * toneP;
            if (r > 0 && l > 0.004)
                bar(r, 20 * std::log10(l), 1.0f);
        }
        label(30.0f, 72.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "STRIKE SPECTRUM", true);

        // the decay at C4
        setPlot(284.0f, 66.0f, 252.0f, 70.0f);
        grid(6, 3, false);
        float xs[61], ys[61];
        const double t1 = mb[3] * dsc, t2 = mb[4] * dsc, ks = mb[5];
        for (int i = 0; i <= 60; ++i)
        {
            const double t = i / 20.0;
            const double e = (1 - ks) * std::exp(-6.907755 * t / t1) + ks * std::exp(-6.907755 * t / t2);
            xs[i] = linX(float(t), 0.0f, 3.0f);
            ys[i] = linY(float(20 * std::log10(std::max(e, 0.000001))), -60.0f, 0.0f);
        }
        curve(xs, ys, 61);
        label(530.0f, 72.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "DECAY AT C4, 3 S", true);

        // the voices and the tremolo
        bool on[128] = {};
        int voices = 0;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            voices = int(std::lround(t->extra.load(std::memory_order_relaxed)));
        }
        char info[64];
        std::snprintf(info, sizeof(info), "VOICES %d / 16", voices);
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        if (value(7) > 0.0f)
            std::snprintf(info, sizeof(info), "TREMOLO %.1f HZ %s %d%%", value(6), value(8) >= 0.5f ? "STEREO" : "MONO", int(std::lround(value(7))));
        else
            std::snprintf(info, sizeof(info), "TREMOLO OFF");
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
        if (param == 11)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 6)
            std::snprintf(buf, n, "%.1f Hz", v);
        else if (param == 13)
            std::snprintf(buf, n, "%d st", int(std::lround(v)));
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

private:
    float fBarX = 0.0f, fBarW = 0.0f, fBarB = 0.0f;
};

UI* createUI()
{
    return new UIKeys();
}

END_NAMESPACE_DISTRHO
