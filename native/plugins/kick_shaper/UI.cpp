/*
 * NULL JSFX - Kick Shaper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "KickShaperDSP.hpp"
#include "KickShaperSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "ATTACK", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "BODY", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "THUMP", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "CLICK", 0, -1, true },
    { 4, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UIKickShaper : public NullKnobsUI
{
public:
    UIKickShaper()
        : NullKnobsUI({ "KICK SHAPER", "ADVANCED", 480, 120, nullptr, 1, nullkick::kSliders, nullkick::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in kick_shaper_gfx.jsfx: left the frequency response of the bells plus Output Gain,
    // right the level of a hit over 0.5 s (Attack Shape at the start, then Decay)
    void drawDisplay() override
    {
        const nullkick::Params p = nullkick::paramsFrom([this](uint32_t i) { return value(i); });
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        nullkick::Bell thump, body, click;
        thump.set(sr, 55.0, 0.8, p.thumpDb);
        body.set(sr, 120.0, 1.0, p.bodyDb);
        click.set(sr, 4000.0, 0.7, p.clickDb);

        setPlot(50.0f, 66.0f, 256.0f, 84.0f);
        gridFreq(24.0f);
        constexpr int n = 257;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = std::min(20.0 * std::pow(1000.0, double(i) / (n - 1)), 0.49 * sr);
            const double g = thump.mag(f, sr) * body.mag(f, sr) * click.mag(f, sr);
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(20.0 * std::log10(std::max(g, 1e-7)) + p.outDb), -16.0f, 16.0f);
        }
        curve(xs, ys, n);

        setPlot(344.0f, 66.0f, 108.0f, 84.0f);
        grid(2, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+24");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX + fPW * 0.5f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0.25 s");
        constexpr int m = 109;
        float tx[m], ty[m];
        for (int i = 0; i < m; ++i)
        {
            tx[i] = fPX + fPW * float(i) / float(m - 1);
            ty[i] = linY(float(nullkick::tailDb(p, 0.5 * double(i) / (m - 1))), -24.0f, 24.0f);
        }
        curve(tx, ty, m);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4)
            NullKnobsUI::formatValue(param, v, buf, n);
        else
            std::snprintf(buf, n, nullkick::tenths(v) > 0.0 ? "+%.1f dB" : "%.1f dB", nullkick::tenths(v));
    }
};

UI* createUI()
{
    return new UIKickShaper();
}

END_NAMESPACE_DISTRHO
