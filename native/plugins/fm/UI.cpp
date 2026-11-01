/*
 * NULL JSFX - FM (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As fm_gfx.jsfx: the display shows the algorithm (who modulates whom, the carriers on the
 * output line, the feedback loop on operator 4) and the four envelopes at their levels, the
 * selected operator's bright, with a dot per voice; a tab per operator and a MASTER tab, the
 * algorithm and the global controls on every tab.
 */

#include "NullUI.hpp"
#include "FmDSP.hpp"
#include "FmSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

using K = NullKnobsUI;

static const char* const kAlgNames[] = { "STACK", "DUAL MOD", "BRANCH", "PAIR STACK", "TWO PAIRS", "TRIPLE", "ONE PAIR", "ADDITIVE" };

static constexpr uint32_t kNumControls = 4 * 7 + 6 + 5;

static const NullKnobsUI::Control* controls()
{
    static NullKnobsUI::Control c[kNumControls];
    int n = 0;
    for (int k = 0; k < 4; ++k)
    {
        const uint32_t b = 2 + 7 * uint32_t(k);
        c[n++] = { b, K::kKnob, "RATIO", 0, k, false, true };
        c[n++] = { b + 1, K::kKnob, "LEVEL", 0, k, false };
        c[n++] = { b + 2, K::kKnob, "ATTACK", 0, k, false, true };
        c[n++] = { b + 3, K::kKnob, "DECAY", 0, k, false, true };
        c[n++] = { b + 4, K::kKnob, "SUSTAIN", 0, k, false };
        c[n++] = { b + 5, K::kKnob, "RELEASE", 0, k, false, true };
        c[n++] = { b + 6, K::kKnob, "VELOCITY", 1, k, false };
    }
    c[n++] = { 33, K::kKnob, "REVERB SIZE", 0, 4, false };
    c[n++] = { 34, K::kKnob, "OCTAVE", 0, 4, true };
    c[n++] = { 35, K::kKnob, "BEND", 0, 4, false };
    c[n++] = { 36, K::kKnob, "VOLUME", 0, 4, false };
    c[n++] = { 37, K::kSelect, "MPE", 0, 4, false };
    c[n++] = { 38, K::kKnob, "MPE BEND", 0, 4, false };
    c[n++] = { 0, K::kSelect, "ALGORITHM", 1, -1, false };
    c[n++] = { 1, K::kKnob, "FEEDBACK", 1, -1, false };
    c[n++] = { 30, K::kKnob, "BRIGHTNESS", 1, -1, true };
    c[n++] = { 31, K::kKnob, "CHORUS", 1, -1, false };
    c[n++] = { 32, K::kKnob, "REVERB", 1, -1, false };
    return c;
}

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullfm::kNumParams];
    for (uint32_t i = 0; i < nullfm::kNumParams; ++i)
        s[i] = nullfm::kSliders[i];
    s[0].options = nullfm::kAlgorithmShort;
    s[37].options = nullfm::kMpeShort;
    return s;
}

class UIFm : public NullKnobsUI
{
public:
    UIFm()
        : NullKnobsUI({ "FM", "INSTRUMENTS", 600, 150, "OP 1|OP 2|OP 3|OP 4|MASTER", 2,
                        uiSliders(), nullfm::kNumParams, controls(), kNumControls, true })
    {
    }

protected:
    void panel(float x, float w, const char* title)
    {
        beginPath();
        rect(x, 76.0f, w, 116.0f);
        fillColor(col(theme::kBackground));
        fill();
        beginPath();
        rect(x + 0.5f, 76.5f, w - 1.0f, 115.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x, 68.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, title, true);
    }

    void line(float x0, float y0, float x1, float y1, Color c, float w = 1.0f)
    {
        beginPath();
        moveTo(x0, y0);
        lineTo(x1, y1);
        strokeColor(c);
        strokeWidth(w);
        stroke();
    }

    static Color mix(uint32_t a, uint32_t b, float k)
    {
        auto ch = [k](uint32_t x, uint32_t y, int sh) { return int(float((x >> sh) & 255) * (1.0f - k) + float((y >> sh) & 255) * k); };
        return Color(ch(a, b, 16), ch(a, b, 8), ch(a, b, 0));
    }

    // where operator k sits in algorithm a's picture (fm_pos)
    static void pos(int a, int k, float& x, float& y)
    {
        const float c = a == 0 ? 1.5f :
                        a == 1 ? (k == 2 ? 1.0f : k == 3 ? 2.0f : 1.5f) :
                        a == 2 ? (k == 0 ? 1.5f : k == 3 ? 2.0f : 1.0f) :
                        a == 3 ? (k == 0 ? 1.5f : k == 1 ? 1.0f : 2.0f) :
                        a == 4 ? (k <= 1 ? 1.0f : 2.0f) :
                        a == 5 ? (k == 3 ? 1.5f : k + 0.5f) :
                        a == 6 ? (k == 0 ? 0.5f : k == 1 ? 1.5f : 2.5f) : float(k);
        const float r = a == 0 ? 3.0f - k :
                        a == 1 ? (k == 0 ? 3.0f : k == 1 ? 2.0f : 1.0f) :
                        a == 2 ? (k == 0 ? 3.0f : k == 2 ? 1.0f : 2.0f) :
                        a == 3 ? (k == 0 ? 3.0f : k == 3 ? 1.0f : 2.0f) :
                        a == 4 ? (k == 0 || k == 2 ? 3.0f : 2.0f) :
                        a == 5 ? (k == 3 ? 2.0f : 3.0f) :
                        a == 6 ? (k == 3 ? 2.0f : 3.0f) : 3.0f;
        x = 55.0f + c * 56.0f;
        y = 94.0f + r * 26.0f;
    }

    static float seg(float ms) { return 0.25f * (0.1f + 0.9f * std::log10(std::max(1.0f, ms)) / 4.0f); }

    void env(int k, Color c)
    {
        const uint32_t b = 2 + 7 * uint32_t(k);
        const float a = value(b + 2), d = value(b + 3), s = value(b + 4) / 100.0f, r = value(b + 5), lv = value(b + 1) / 100.0f;
        const float x0 = 268.0f, w = 304.0f;
        const float xa = x0 + w * seg(a), xd = xa + w * seg(d), xh = xd + w * 0.2f, xr = xh + w * seg(r);
        beginPath();
        moveTo(x0, 186.0f);
        lineTo(xa, 186.0f - 100.0f * lv);
        for (int i = 1; i <= 16; ++i)
        {
            const float u = i / 16.0f, e = s + (1 - s) * std::exp(-4.6f * u);
            lineTo(xa + (xd - xa) * u, 186.0f - 100.0f * lv * e);
        }
        lineTo(xh, 186.0f - 100.0f * lv * s);
        for (int i = 1; i <= 16; ++i)
        {
            const float u = i / 16.0f, e = s * std::exp(-4.6f * u);
            lineTo(xh + (xr - xh) * u, 186.0f - 100.0f * lv * e);
        }
        strokeColor(c);
        strokeWidth(1.5f);
        lineJoin(ROUND);
        stroke();
    }

    void drawDisplay() override
    {
        const int a = std::max(0, std::min(7, int(std::floor(value(0) + 0.5f))));
        const int tab = page();
        double MM[16], CR[4];
        nullfm::routing(a, MM, CR);
        char title[48];
        std::snprintf(title, sizeof(title), "ALGORITHM  %s", kAlgNames[a]);
        panel(24.0f, 230.0f, title);

        float x, y, x0 = 1000.0f, x1 = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            pos(a, k, x, y);
            if (CR[k] > 0)
            {
                x0 = std::min(x0, x);
                x1 = std::max(x1, x);
                line(x + 0.5f, y + 8.0f, x + 0.5f, 184.0f, col(theme::kMuted));
            }
        }
        beginPath();
        rect(x0 - 1.0f, 184.0f, x1 - x0 + 2.0f, 1.5f);
        fillColor(col(theme::kMuted));
        fill();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (MM[i * 4 + j] > 0)
                {
                    float sx, sy;
                    pos(a, j, sx, sy);
                    pos(a, i, x, y);
                    line(sx, sy + 8.0f, x, y - 8.0f, col(theme::kAccent, 0.8f), 1.5f);
                }
        pos(a, 3, x, y);
        const Color fb = col(value(1) > 0 ? theme::kAccent : theme::kBorder);
        beginPath();
        moveTo(x + 14.0f, y + 0.5f);
        lineTo(x + 20.5f, y + 0.5f);
        lineTo(x + 20.5f, y - 11.5f);
        lineTo(x + 0.5f, y - 11.5f);
        lineTo(x + 0.5f, y - 8.0f);
        strokeColor(fb);
        strokeWidth(1.0f);
        stroke();
        for (int k = 0; k < 4; ++k)
        {
            pos(a, k, x, y);
            const float lv = value(3 + 7 * uint32_t(k)) / 100.0f;
            beginPath();
            rect(x - 14.0f, y - 8.0f, 28.0f, 16.0f);
            fillColor(col(theme::kCard));
            fill();
            beginPath();
            rect(x - 13.5f, y - 7.5f, 27.0f, 15.0f);
            strokeColor(mix(theme::kBorder, theme::kAccent, k == tab ? 1.0f : 0.35f + 0.65f * lv));
            strokeWidth(1.0f);
            stroke();
            char num[4];
            std::snprintf(num, sizeof(num), "%d", k + 1);
            fontFace("noto-bold");
            fontSize(8.0f * kNotoLineToEm);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(k == tab ? col(theme::kText) : mix(theme::kMuted, theme::kText, lv));
            text(x, y, num, nullptr);
            fontFace("noto");
        }

        panel(264.0f, 312.0f, "ENVELOPES");
        for (int k = 0; k < 4; ++k)
            if (k != tab)
                env(k, col(theme::kMuted, tab < 4 ? 0.45f : 0.9f));
        if (tab < 4)
            env(tab, col(theme::kAccent));

        nulljsfx::Taps* tp = taps();
        for (int k = 0; k < 8; ++k)
        {
            const float e = tp ? std::max(0.0f, std::min(1.0f, tp->aux[k].load(std::memory_order_relaxed))) : 0.0f;
            beginPath();
            circle(510.0f + 9.0f * k, 68.0f, 2.5f);
            fillColor(mix(theme::kBorder, theme::kAccent, e));
            fill();
        }
    }

    void formatValue(uint32_t p, float v, char* buf, size_t n) override
    {
        const int k = p >= 2 && p < 30 ? int(p - 2) % 7 : -1;
        if (k == 0)
            std::snprintf(buf, n, "x%.2f", double(v));
        else if (k == 2 || k == 3 || k == 5)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f s", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        }
        else if (p == 30)
            std::snprintf(buf, n, "%+d%%", int(std::lround(v)));
        else if (p == 34)
            std::snprintf(buf, n, "%+d", int(std::lround(v)));
        else if (p == 35 || p == 38)
            std::snprintf(buf, n, "%d st", int(std::lround(v)));
        else if (p == 36)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(p, v, buf, n);
    }
};

UI* createUI()
{
    return new UIFm();
}

END_NAMESPACE_DISTRHO
