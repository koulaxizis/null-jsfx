/*
 * NULL JSFX - Choir (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As choir_gfx.jsfx: the display holds the formants of the vowel sung now (soprano, alto, tenor
 * and bass), the vowel between A and U, the keys sounding, the voices and the section, and a
 * keyboard from C2 to C7 with the keys sounding. Three rows of controls below.
 */

#include "NullUI.hpp"
#include "ChoirDSP.hpp"
#include "ChoirSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "VOWEL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "VOICES", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DETUNE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "BREATH", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "VIBRATO", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 6, NullKnobsUI::kSelect, "SECTION", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "ATTACK", 1, -1, false, true },
    { 8, NullKnobsUI::kKnob, "RELEASE", 1, -1, false, true },
    { 9, NullKnobsUI::kKnob, "BRIGHTNESS", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "VELOCITY", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "STEREO", 1, -1, false },
    { 12, NullKnobsUI::kSelect, "MPE", 2, -1, false },
    { 13, NullKnobsUI::kKnob, "BEND RANGE", 2, -1, false },
    { 14, NullKnobsUI::kKnob, "VOLUME", 2, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullchoir::kNumParams];
    for (uint32_t i = 0; i < nullchoir::kNumParams; ++i)
        s[i] = nullchoir::kSliders[i];
    s[6].options = nullchoir::kSectionShort;
    s[12].options = nullchoir::kMpeShort;
    return s;
}

static const char* const kVowels[] = { "A", "E", "I", "O", "U" };

class UIChoir : public NullKnobsUI
{
public:
    UIChoir()
        : NullKnobsUI({ "CHOIR", "INSTRUMENTS", 560, 156, nullptr, 3, uiSliders(), nullchoir::kNumParams, kControls, kNumControls, true })
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
        double vals[nullchoir::kNumParams];
        for (uint32_t i = 0; i < nullchoir::kNumParams; ++i)
            vals[i] = jsfxValue(nullchoir::kSliders[i], value(i));
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        bool on[128] = {};
        int keys = 0;
        double vowel = -1.0;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            keys = int(std::lround(t->extra.load(std::memory_order_relaxed)));
            vowel = t->aux[8].load(std::memory_order_relaxed);
        }
        if (vowel < 0.0)
            vowel = std::max(0.0, std::min(4.0, vals[0]));
        const int sect = int(std::max(0.0, std::min(4.0, std::floor(vals[6] + 0.5))));
        double co[nullchoir::kBanks * 40];
        nullchoir::makeCoefs(co, vowel, vals[11] / 100.0, sr);

        // the formants, 50 Hz to 10 kHz, -48 to +6 dB
        setPlot(24.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        float xs[101], ys[101];
        for (int s = 0; s < 4; ++s)
        {
            for (int j = 0; j <= 100; ++j)
            {
                const double f = 50.0 * std::pow(200.0, j / 100.0);
                const double w = 2.0 * M_PI * std::min(f, 0.4999 * sr) / sr;
                const double m = nullchoir::bankMag(co, s * 2, w);
                xs[j] = fPX + fPW * float(j) / 100.0f;
                ys[j] = linY(float(20.0 * std::log10(std::max(m, 0.0000001))), -48.0f, 6.0f);
            }
            line(xs, ys, 101, sect == 0 || sect == s + 1 ? 1.0f : 0.3f);
        }
        label(30.0f, 72.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "FORMANTS", true);

        // the vowel, A to U
        setPlot(284.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 1, false);
        box(fPX + fPW * float(vowel / 4.0) - 1.0f, fPY, 2.0f, fPH, theme::kAccent);
        for (int i = 0; i < 5; ++i)
        {
            const float x = fPX + fPW * (i == 0 ? 0.04f : i == 4 ? 0.96f : float(i) / 4.0f);
            label(x, 66.0f + 35.0f, ALIGN_CENTER, 12.0f, 0.5f, std::fabs(vowel - i) < 0.5 ? theme::kText : theme::kMuted, kVowels[i], true);
        }
        label(530.0f, 72.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "VOWEL", true);

        // the keys sounding, the voices and the section
        char info[48];
        std::snprintf(info, sizeof(info), "KEYS %d  VOICES %d", keys, int(std::lround(vals[1])));
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        label(536.0f, 146.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, nullchoir::kSectionShort[sect], true);

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
        if (param == 0)
        {
            const int l = int(std::floor(v + 0.0001f));
            const float f = v - float(l);
            if (f < 0.005f || l >= 4)
                std::snprintf(buf, n, "%s", kVowels[std::max(0, std::min(4, l))]);
            else
                std::snprintf(buf, n, "%s>%s %d%%", kVowels[l], kVowels[l + 1], int(std::floor(f * 100.0f + 0.5f)));
        }
        else if (param == 1)
            std::snprintf(buf, n, "%d", int(std::lround(v)));
        else if (param == 2)
            std::snprintf(buf, n, "%d ct", int(std::lround(v)));
        else if (param == 5)
            std::snprintf(buf, n, "%.1f Hz", v);
        else if (param == 13)
            std::snprintf(buf, n, "%d st", int(std::lround(v)));
        else if (param == 14)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 7 || param == 8)
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
    return new UIChoir();
}

END_NAMESPACE_DISTRHO
