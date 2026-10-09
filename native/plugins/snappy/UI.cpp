/*
 * NULL JSFX - Snappy (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SnappyDSP.hpp"

START_NAMESPACE_DISTRHO

class UISnappy : public NullOneSliderUI
{
public:
    UISnappy()
        : NullOneSliderUI({ "SNAPPY", "AMOUNT", "SOFT", "PUNCH", 0.0f, 100.0f, 50.0f, false, true }) {}

protected:
    static constexpr int kLen = 284; // history entries, one per design pixel (7 ms each)

    // as in snappy_gfx.jsfx: the last 2 s of gain (line, -3..+9 dB) over the output level
    // (dim bars, -48..0 dB), read from the Taps ring the DSP fills (see Plugin.cpp)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(8, 4, false);
        const float zy = std::floor(linY(0.0f, -3.0f, 9.0f)) + 0.5f;
        beginPath();
        moveTo(fPX, zy);
        lineTo(fPX + fPW, zy);
        strokeColor(col(theme::kTrackHover));
        strokeWidth(1.0f);
        stroke();
        for (int i = 0; i < 5; ++i)
        {
            const float y = std::floor(fPY + fPH * float(i) / 4.0f) + 0.5f;
            const int db = 9 - 3 * i;
            char buf[8];
            std::snprintf(buf, sizeof(buf), db > 0 ? "+%d" : "%d", db);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        float xs[kLen], ys[kLen];
        float lv[kLen];
        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
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
            ys[i] = linY(20.0f * std::log10(std::fmax(g, 1e-7f)), -3.0f, 9.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.08f));
        fill();
        curve(xs, ys, kLen);
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const double s = v;
        if (std::fabs(s) < 0.00001)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "HITS %+.1f  SUSTAIN %+.1f dB", s * 0.09, s * -0.03);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UISnappy)
};

UI* createUI()
{
    return new UISnappy();
}

END_NAMESPACE_DISTRHO
