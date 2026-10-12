/*
 * NULL JSFX - Envelope Follower (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "EnvelopeFollowerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "ATTACK", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SENSITIVITY", 0, -1, true },
    { 7, NullKnobsUI::kSelect, "DIRECTION", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "LOW FREQ", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "HIGH FREQ", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "RESONANCE", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIEnvelopeFollower : public NullKnobsUI
{
public:
    UIEnvelopeFollower()
        : NullKnobsUI({ "ENVELOPE FOLLOWER", "ESSENTIALS", 480, 120, nullptr, 2, nullenvfollower::kSliders,
                        nullenvfollower::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 260; // history entries, one per design pixel (10 ms each)

    bool volume() const { return std::fabs(value(0) - 3.0f) < 0.00001f; }

    // as in envelope_follower_gfx.jsfx: left, the last 2.6 s of input level (bars, -48..0 dB) and
    // envelope position (line) on the Low..High Freq axis (or 0..100% volume); right, input and
    // envelope meters, and the cutoff (or volume) now
    void drawDisplay() override
    {
        setPlot(56.0f, 68.0f, 260.0f, 88.0f);
        grid(8, 4, false);
        char buf[24];
        const float lo = value(4), hi = std::fmax(value(5), value(4));
        for (int i = 0; i < 3; ++i)
        {
            if (volume())
                std::snprintf(buf, sizeof(buf), "%d%%", 100 - 50 * i);
            else
            {
                const double f = double(lo) * std::pow(double(hi) / double(lo), 1.0 - double(i) / 2.0);
                if (f >= 1000.0)
                    std::snprintf(buf, sizeof(buf), "%.1fk", f / 1000.0);
                else
                    std::snprintf(buf, sizeof(buf), "%d", int(std::floor(f + 0.5)));
            }
            label(fPX - 6.0f, fPY + fPH * float(i) / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float xs[kLen], ys[kLen], lv[kLen];
        for (int i = 0; i < kLen; ++i)
        {
            float level = 0.0f, e = 0.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                level = t->scopeL[k];
                e = t->scopeR[k];
            }
            lv[i] = fPH * dbNorm(nulljsfx::toDb(level), -48.0f);
            xs[i] = fPX + float(i) + 0.5f;
            ys[i] = linY(e, 0.0f, 1.0f);
        }
        beginPath();
        for (int i = 0; i < kLen; ++i)
            if (lv[i] > 0.0f)
                rect(fPX + float(i), fPY + fPH - lv[i], 1.0f, lv[i]);
        fillColor(col(theme::kAccent, 0.15f));
        fill();
        curve(xs, ys, kLen);

        float in = 0.0f, e = 0.0f, fc = 0.0f;
        if (t)
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            e = t->gr.load(std::memory_order_relaxed);
            fc = t->extra.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(in), -140.0f, -48.0f);
        meter(336.0f, 106.0f, 112.0f, 10.0f, "ENV", -48.0f + 48.0f * e, -140.0f, -48.0f);
        label(336.0f, 135.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, volume() ? "GAIN" : "CUTOFF", true);
        if (volume())
            std::snprintf(buf, sizeof(buf), "%d%%", int(std::floor(e * 100.0f + 0.5f)));
        else if (fc >= 1000.0f)
            std::snprintf(buf, sizeof(buf), "%.2f kHz", fc / 1000.0f);
        else
            std::snprintf(buf, sizeof(buf), "%d Hz", int(std::floor(fc + 0.5f)));
        label(448.0f, 135.0f, ALIGN_RIGHT, 8.5f, 0.0f, theme::kText, buf);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%.1f ms", v);
        else if (param == 3)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if ((param == 4 || param == 5) && v >= 1000.0f)
            std::snprintf(buf, n, "%.2f kHz", v / 1000.0f);
        else if (param == 6)
            std::snprintf(buf, n, "%.1f", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIEnvelopeFollower();
}

END_NAMESPACE_DISTRHO
