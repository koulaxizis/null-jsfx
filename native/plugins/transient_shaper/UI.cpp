/*
 * NULL JSFX - Transient Shaper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TransientShaperSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "ATTACK", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "SUSTAIN", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "SPEED", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "FOCUS LOW", 1, -1, false, true },
    { 5, NullKnobsUI::kKnob, "FOCUS HIGH", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
    { 6, NullKnobsUI::kSelect, "CLIP", 1, -1, false },
};

class UITransientShaper : public NullKnobsUI
{
public:
    UITransientShaper()
        : NullKnobsUI({ "TRANSIENT SHAPER", "ESSENTIALS", 480, 120, nullptr, 2, nulltransient::kSliders, nulltransient::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260; // history entries, one per design pixel (7 ms each)

    // the gain history: output level as dim bars (-48..0 dB), the gain after Mix as the line
    // (-20..+20 dB)
    void history()
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 4, false);
        const float zy = std::floor(linY(0.0f, -20.0f, 20.0f)) + 0.5f;
        beginPath();
        moveTo(fPX, zy);
        lineTo(fPX + fPW, zy);
        strokeColor(col(theme::kTrackHover));
        strokeWidth(1.0f);
        stroke();
        char buf[16];
        for (int i = 0; i < 5; ++i)
        {
            const int db = 20 - 10 * i;
            std::snprintf(buf, sizeof(buf), db > 0 ? "+%d" : "%d", db);
            label(fPX - 6.0f, fPY + fPH * float(i) / 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float xs[kLen], ys[kLen], lv[kLen];
        for (int i = 0; i < kLen; ++i)
        {
            float level = 0.0f, g = 1.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                level = t->scopeL[k];
                g = t->scopeR[k];
            }
            lv[i] = fPH * dbNorm(nulljsfx::toDb(level), -48.0f);
            xs[i] = fPX + float(i) + 0.5f;
            ys[i] = linY(nulljsfx::toDb(g), -20.0f, 20.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        curve(xs, ys, kLen);
    }

    // as in transient_shaper_gfx.jsfx: left, the history (the chunk's gain furthest from 0 dB);
    // right, the speed and what is switched on, then input, transient-detector and output meters
    void drawDisplay() override
    {
        history();
        const float sp = value(3);
        label(336.0f, 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, sp < 0.5f ? "FAST" : sp > 1.5f ? "SLOW" : "MEDIUM", true);
        const bool focus = value(4) > 20.0f || value(5) < 20000.0f, clip = value(6) >= 0.5f;
        char st[32];
        std::snprintf(st, sizeof(st), "%s%s%s", focus ? "FOCUS" : "", focus && clip ? "   " : "", clip ? "CLIP" : "");
        label(448.0f, 66.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, st, true);
        float in = 0.0f, out = 0.0f, hit = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            hit = t->extra.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(in), -140.0f, -48.0f);
        meter(336.0f, 106.0f, 112.0f, 10.0f, "HIT", -48.0f + 48.0f * hit, -140.0f, -48.0f);
        meter(336.0f, 130.0f, 112.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -48.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param < 2 || param == 7)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if ((param == 4 && v <= 20.0f) || (param == 5 && v >= 20000.0f))
            std::snprintf(buf, n, "OFF");
        else if (param == 4 || param == 5)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UITransientShaper();
}

END_NAMESPACE_DISTRHO
