/*
 * NULL JSFX - Auto-Leveler (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AutoLevelerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TARGET", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SPEED", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "MAX BOOST", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MAX CUT", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "GATE", 0, -1, false },
};

class UIAutoLeveler : public NullKnobsUI
{
public:
    UIAutoLeveler()
        : NullKnobsUI({ "AUTO-LEVELER", "ESSENTIALS", 480, 120, nullptr, 1, nullautoleveler::kSliders,
                        nullautoleveler::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260; // history entries, one per design pixel (20 ms each)

    void hline(float y, uint32_t rgb, float alpha)
    {
        y = std::floor(y) + 0.5f;
        beginPath();
        moveTo(fPX, y);
        lineTo(fPX + fPW, y);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    // as in auto_leveler_gfx.jsfx: left, the last 5.2 s of input RMS level (bars, -60..0 dB) and
    // gain (line, -24..+24 dB) between Max Boost and Max Cut; right, input and output RMS meters
    // with Target marked, and the gain now (HOLD below Gate)
    void drawDisplay() override
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 4, false);
        hline(linY(0.0f, -24.0f, 24.0f), theme::kTrackHover, 1.0f);
        char buf[24];
        for (int i = 0; i < 5; ++i)
        {
            const int db = 24 - 12 * i;
            std::snprintf(buf, sizeof(buf), db > 0 ? "+%d" : "%d", db);
            label(fPX - 6.0f, fPY + fPH * float(i) / 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float xs[kLen], ys[kLen], lv[kLen];
        for (int i = 0; i < kLen; ++i)
        {
            float lev = 0.0f, g = 0.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                lev = t->scopeL[k];
                g = t->scopeR[k];
            }
            lv[i] = fPH * dbNorm(nulljsfx::toDb(lev), -60.0f);
            xs[i] = fPX + float(i) + 0.5f;
            ys[i] = linY(g, -24.0f, 24.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        hline(linY(value(2), -24.0f, 24.0f), theme::kText, 0.35f);
        hline(linY(-value(3), -24.0f, 24.0f), theme::kText, 0.35f);
        curve(xs, ys, kLen);

        float in = 0.0f, out = 0.0f, gain = 0.0f;
        if (t)
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gain = t->extra.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(in), value(0), -60.0f);
        meter(336.0f, 106.0f, 112.0f, 10.0f, "OUT", nulljsfx::toDb(out), value(0), -60.0f);
        label(336.0f, 135.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, nulljsfx::toDb(in) > value(4) ? "GAIN" : "HOLD", true);
        std::snprintf(buf, sizeof(buf), "%+.1f dB", gain);
        label(448.0f, 135.0f, ALIGN_RIGHT, 8.5f, 0.0f, theme::kText, buf);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0 || param == 2 || param == 3)
            std::snprintf(buf, n, "%.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIAutoLeveler();
}

END_NAMESPACE_DISTRHO
