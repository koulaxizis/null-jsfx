/*
 * NULL JSFX - Null Test (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "NullTestDSP.hpp"
#include "NullTestSliders.hpp"

START_NAMESPACE_DISTRHO

// one row, as in null_test_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LISTEN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "ALIGN", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RANGE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "OFFSET", 0, -1, true },
    { 4, NullKnobsUI::kKnob, "TRIM", 0, -1, true },
    { 5, NullKnobsUI::kSelect, "POLARITY", 0, -1, false },
};

class UINullTest : public NullKnobsUI
{
public:
    UINullTest()
        : NullKnobsUI({ "NULL TEST", "ADVANCED", 480, 200, nullptr, 1, nullnulltest::kUiSliders,
                        nullnulltest::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in null_test_gfx.jsfx: the null depth, the delay, the gain and the match; the difference
    // per octave band
    void drawDisplay() override
    {
        float depth = -400.0f, delay = 0.0f, gain = 0.0f, conf = 0.0f, bands[10];
        bool same = false;
        for (float& b : bands)
            b = -150.0f;
        if (nulljsfx::Taps* t = taps())
        {
            depth = t->extra.load(std::memory_order_relaxed);
            delay = t->gr.load(std::memory_order_relaxed);
            for (int i = 0; i < 10; ++i)
                bands[i] = t->aux[i].load(std::memory_order_relaxed);
            gain = t->aux[10].load(std::memory_order_relaxed);
            conf = t->aux[11].load(std::memory_order_relaxed);
            same = t->aux[12].load(std::memory_order_relaxed) > 0.5f;
        }
        const int listen = int(nullnulltest::sliderValue(0, value(0)));
        const bool autoAlign = nullnulltest::sliderValue(1, value(1)) >= 0.5;
        const float x0 = fVX + 8.0f;
        char buf[96];
        label(x0, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, "NULL DEPTH", true);
        if (same)
            label(x0, fVY + 38.0f, ALIGN_LEFT, 22.0f, 1.0f, theme::kText, "BIT-IDENTICAL", true);
        else if (depth <= -400.0f)
            label(x0, fVY + 38.0f, ALIGN_LEFT, 22.0f, 1.0f, theme::kSlogan, "NO SIGNAL", true);
        else
        {
            if (depth < -150.0f)
                std::snprintf(buf, sizeof(buf), "< -150 dB");
            else
                std::snprintf(buf, sizeof(buf), "%.1f dB", depth);
            label(x0, fVY + 38.0f, ALIGN_LEFT, 22.0f, 1.0f, theme::kText, buf, true);
        }
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        std::snprintf(buf, sizeof(buf), "DELAY %d SAMPLES  %.2f MS", int(delay), delay * 1000.0 / sr);
        label(x0, fVY + 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        const float g = std::fabs(gain);
        if (g > 0.0f)
            std::snprintf(buf, sizeof(buf), "GAIN %+.2f dB%s", 20.0 * std::log10(g), gain < 0.0f ? "  INVERTED" : "");
        else
            std::snprintf(buf, sizeof(buf), "GAIN -");
        label(x0, fVY + 82.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        if (autoAlign)
            std::snprintf(buf, sizeof(buf), "MATCH %d%%", int(std::floor(conf * 100.0f + 0.5f)));
        else
            std::snprintf(buf, sizeof(buf), "MANUAL");
        label(x0, fVY + 98.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        label(x0, fVY + 126.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kSlogan, "MAIN ON 1/2, THE OTHER ON 3/4", true);
        label(x0, fVY + 140.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kText,
              listen == 0 ? "LISTEN: MAIN" : listen == 1 ? "LISTEN: SIDECHAIN" : "LISTEN: DIFFERENCE", true);

        // the difference per octave band: 0 dB at the top, -120 dB at the bottom
        static const char* const kNames[] = { "31", "63", "125", "250", "500", "1K", "2K", "4K", "8K", "16K" };
        const float bx = fVX + 216.0f, bw = fVX + fVW - 8.0f - bx, by = fVY + 20.0f, bh = fVH - 40.0f;
        for (int i = 0; i < 10; ++i)
        {
            const float w = bw / 10.0f, x = bx + i * w + 2.0f;
            const float v = std::fmax(0.0f, std::fmin(1.0f, (bands[i] + 120.0f) / 120.0f));
            beginPath();
            rect(x, by, w - 4.0f, bh);
            fillColor(col(theme::kBorder, 0.35f));
            fill();
            beginPath();
            rect(x, by + bh * (1.0f - v), w - 4.0f, bh * v);
            fillColor(col(theme::kAccent, 0.4f + 0.6f * v));
            fill();
            label(x + (w - 4.0f) * 0.5f, by + bh + 10.0f, ALIGN_CENTER, 7.0f, 0.0f, theme::kMuted, kNames[i]);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullnulltest::sliderValue(param, v);
        if (param == 2)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param == 3)
            std::snprintf(buf, n, "%d", int(x));
        else if (param == 4)
            std::snprintf(buf, n, "%+.2f dB", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UINullTest();
}

END_NAMESPACE_DISTRHO
