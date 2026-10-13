/*
 * NULL JSFX - DeEsser (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DeEsserDSP.hpp"
#include "DeEsserSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "FREQUENCY", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "REDUCTION", 0, -1, false },
};

class UIDeEsser : public NullKnobsUI
{
public:
    UIDeEsser()
        : NullKnobsUI({ "DEESSER", "ESSENTIALS", 480, 120, nullptr, 1, nulldeesser::kSliders, nulldeesser::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // x for f on a 1 kHz .. 20 kHz log axis
    float fx(float f) const { return fPX + fPW * std::log(f / 1000.0f) / std::log(20.0f); }

    void line(float x0, float y0, float x1, float y1, uint32_t rgb, float alpha)
    {
        beginPath();
        moveTo(x0, y0);
        lineTo(x1, y1);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    // as in deesser_gfx.jsfx: left, the response from 1 to 20 kHz (-21..+3 dB) at full Reduction
    // (dim) and now (bright); right, sibilance-band level (threshold marked), band reduction, output
    void drawDisplay() override
    {
        setPlot(56.0f, 66.0f, 260.0f, 78.0f);
        for (float d = 1000.0f; d <= 10000.0f; d *= 10.0f)
            for (int m = 1; m <= 9; ++m)
                if (float(m) * d <= 20000.0f)
                {
                    const float x = std::floor(fx(float(m) * d)) + 0.5f;
                    line(x, fPY, x, fPY + fPH, theme::kBorder, m == 1 ? 1.0f : 0.5f);
                }
        char buf[16];
        for (int i = 0; i < 4; ++i)
        {
            const float y = std::floor(linY(-6.0f * float(i), -21.0f, 3.0f)) + 0.5f;
            line(fPX, y, fPX + fPW, y, i == 0 ? theme::kTrackHover : theme::kBorder, i == 0 ? 1.0f : 0.6f);
            std::snprintf(buf, sizeof(buf), "%d", -6 * i);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        static const float kF[] = { 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        static const char* const kN[] = { "1k", "2k", "5k", "10k", "20k" };
        for (int i = 0; i < 5; ++i)
            label(fx(kF[i]), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, kN[i]);

        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        const nulldeesser::Coefs c = nulldeesser::bandpass(value(0), sr);
        const double maxCut = 1.0 - std::pow(10.0, value(1) / 20.0);
        nulljsfx::Taps* t = taps();
        const double cut = t ? t->extra.load(std::memory_order_relaxed) : 0.0;
        constexpr int n = 131;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 1000.0 * std::pow(20.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nulldeesser::responseDb(c, f, sr, maxCut)), -21.0f, 3.0f);
        }
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kAccent, 0.5f));
        strokeWidth(1.0f);
        stroke();
        for (int i = 0; i < n; ++i)
        {
            const double f = 1000.0 * std::pow(20.0, double(i) / double(n - 1));
            ys[i] = linY(float(nulldeesser::responseDb(c, f, sr, cut)), -21.0f, 3.0f);
        }
        curve(xs, ys, n);

        float env = 0.0f, out = 0.0f;
        if (t)
        {
            env = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
        }
        meter(336.0f, 82.0f, 112.0f, 10.0f, "SIB", nulljsfx::toDb(env), -30.0f, -60.0f);
        grMeter(336.0f, 106.0f, 112.0f, 10.0f, "GR", float(-20.0 * std::log10(std::fmax(1.0 - cut, 0.0000001))), 18.0f);
        meter(336.0f, 130.0f, 112.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
        else
            std::snprintf(buf, n, "%.1f dB", v);
    }
};

UI* createUI()
{
    return new UIDeEsser();
}

END_NAMESPACE_DISTRHO
