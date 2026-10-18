/*
 * NULL JSFX - Maximizer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "MaximizerSliders.hpp"

#include <string>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "INPUT", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "CEILING", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "LOOKAHEAD", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "OUTPUT", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "LINK", 0, -1, false },
    { 6, NullKnobsUI::kSelect, "DITHER", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "SHAPING", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "CHARACTER", 1, -1, false },
    { 9, NullKnobsUI::kSelect, "MULTIBAND", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "LOW X", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "HIGH X", 1, -1, false },
};

class UIMaximizer : public NullKnobsUI
{
public:
    UIMaximizer()
        : NullKnobsUI({ "MAXIMIZER", "ESSENTIALS", 480, 120, nullptr, 2, nullmaximizer::kUiSliders, nullmaximizer::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260; // history entries, one per design pixel (7 ms each)

    // the gain history: output level as dim bars (-48..0 dB), gain as the line (-24..0 dB)
    void history()
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 4, false);
        char buf[16];
        for (int i = 0; i < 5; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -6 * i);
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
            ys[i] = linY(nulljsfx::toDb(g), -24.0f, 0.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        curve(xs, ys, kLen);
    }

    // as in maximizer_gfx.jsfx: left, the history (the gain of the lower channel when unlinked);
    // right, input (limiting point marked), gain-reduction and output (final ceiling marked) meters
    void drawDisplay() override
    {
        history();
        float in = 0.0f, out = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(in), value(1) - value(0), -48.0f);
        grMeter(336.0f, 106.0f, 112.0f, 10.0f, "GR", gr, 24.0f);
        meter(336.0f, 130.0f, 112.0f, 10.0f, "OUT", nulljsfx::toDb(out), value(1) + std::fmin(value(4), 0.0f), -48.0f);
        // short-term loudness of the output (3 s, K-weighted), and what Character / Multiband do
        const float lufs = taps() ? taps()->extra.load(std::memory_order_relaxed) : -200.0f;
        char buf[48];
        label(336.0f, 68.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, "SHORT-TERM", true);
        if (lufs > -70.0f)
            std::snprintf(buf, sizeof(buf), "%.1f LUFS", double(lufs));
        else
            std::snprintf(buf, sizeof(buf), "-- LUFS");
        label(448.0f, 68.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kAccent, buf, true);
        const int character = int(std::lround(value(8)));
        const bool mb = value(9) >= 0.5f;
        std::string st = character == 1 ? "PUNCHY" : character == 2 ? "WARM" : "";
        if (mb)
            st += character != 0 ? " / 3-BAND" : "3-BAND";
        const int dither = int(std::lround(value(6)));
        if (dither != 0)
        {
            if (!st.empty())
                st += " / ";
            st += dither == 1 ? "16-BIT" : "24-BIT";
            st += value(7) >= 0.5f ? " NS" : " TPDF";
        }
        if (!st.empty())
            label(448.0f, 152.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, st.c_str());
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 1 || param == 4)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 10)
            std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        else if (param == 11)
            std::snprintf(buf, n, "%.1f kHz", std::round(v) / 1000.0);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIMaximizer();
}

END_NAMESPACE_DISTRHO
