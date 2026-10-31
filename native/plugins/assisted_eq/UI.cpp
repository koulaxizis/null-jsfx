/*
 * NULL JSFX - Assisted EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AssistedEqDSP.hpp"
#include "AssistedEqSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LEARN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "LOW TRIM", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "MID TRIM", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "HIGH TRIM", 1, -1, true },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIAssistedEq : public NullKnobsUI
{
public:
    UIAssistedEq()
        : NullKnobsUI({ "ASSISTED EQ", "ASSISTED", 480, 140, nullptr, 2, nullassistedeq::kSliders,
                        nullassistedeq::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // When Learn finishes, the plugin moves Learn and the Learned sliders itself (as the JSFX
    // writes them in its code). The host does not know yet: tell it, so automation, undo and
    // the host's own view of the parameters follow.
    void uiIdle() override
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        if (nulljsfx::Taps* t = taps())
        {
            const float gen = t->aux[1].load(std::memory_order_acquire);
            if (gen != fGen)
            {
                fGen = gen;
                auto* p = static_cast<NullMultiPlugin*>(getPluginInstancePointer());
                for (uint32_t i = 0; p && i < nullassistedeq::kNumSliders; ++i)
                {
                    if (i != 0 && i < 8)
                        continue;
                    const float v = p->value(i);
                    if (v != value(i))
                    {
                        editParameter(i, true);
                        setParameterValue(i, v);
                        editParameter(i, false);
                        parameterChanged(i, v);
                    }
                }
            }
        }
#endif
        NullKnobsUI::uiIdle();
    }

    // as in assisted_eq_gfx.jsfx: the EQ's response, Learn's progress, and what it does
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        for (uint32_t i = 0; i < nullassistedeq::kNumSliders; ++i)
            fDsp.s[i] = nullassistedeq::sliderValue(i, value(i));
        fDsp.setSampleRate(sr);

        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 150.0f, fVH - 20.0f);
        gridFreq(24.0f);
        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ys[512];
        const double outDb = 20 * std::log10(fDsp.outGain());
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + float(i);
            const double f = 20.0 * std::pow(1000.0, double(i) / double(fPW));
            ys[i] = linY(float(10 * std::log10(fDsp.mag2(f)) + outDb), -12.0f, 12.0f);
        }
        curve(xs, ys, n, linY(0.0f, -12.0f, 12.0f));

        const float tx = fVX + fVW - 112.0f;
        nulljsfx::Taps* t = taps();
        const bool listening = t && t->gr.load(std::memory_order_relaxed) >= 0.5f;
        const bool learned = nullassistedeq::sliderValue(8, value(8)) >= 0.5;
        label(tx, fVY + 10.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted,
              listening ? "LISTENING" : learned ? "LEARNED" : "NOT LEARNED", true);
        char buf[64];
        if (listening)
        {
            const float prog = t->extra.load(std::memory_order_relaxed);
            beginPath();
            rect(tx, fVY + 22.0f, 104.0f, 4.0f);
            fillColor(col(theme::kBorder));
            fill();
            beginPath();
            rect(tx, fVY + 22.0f, 104.0f * prog, 4.0f);
            fillColor(col(theme::kAccent));
            fill();
            std::snprintf(buf, sizeof(buf), "%d of 15 s", int(std::floor(t->aux[0].load(std::memory_order_relaxed))));
            label(tx, fVY + 38.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
        }
        else
        {
            // the three largest region gains, largest first, then the high-pass
            int used = 0, row = 0;
            for (int k = 0; k < 3; ++k)
            {
                int best = -1;
                double bv = 0.25;
                for (int i = 0; i < 5; ++i)
                {
                    if (!(used & (1 << i)) && std::fabs(fDsp.gain(i)) >= bv)
                    {
                        best = i;
                        bv = std::fabs(fDsp.gain(i));
                    }
                }
                if (best < 0)
                    break;
                used |= 1 << best;
                static const char* const kNames[] = { "LOW", "MUD", "HONK", "PRESENCE", "AIR" };
                if (best == 1)
                    std::snprintf(buf, sizeof(buf), "MUD %d Hz %+.1f dB", int(fDsp.mudFreq()), fDsp.gain(1));
                else
                    std::snprintf(buf, sizeof(buf), "%s %+.1f dB", kNames[best], fDsp.gain(best));
                label(tx, fVY + 30.0f + float(row) * 16.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
                ++row;
            }
            if (fDsp.highPass() > 0 && row < 3)
            {
                std::snprintf(buf, sizeof(buf), "HIGH-PASS %d Hz", int(fDsp.highPass()));
                label(tx, fVY + 30.0f + float(row) * 16.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
                ++row;
            }
            if (row == 0)
                label(tx, fVY + 30.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted,
                      learned ? "NO CHANGES NEEDED" : "PRESS LEARN AND PLAY");
        }
        std::snprintf(buf, sizeof(buf), "LEVEL %+.1f dB", fDsp.autoLevel());
        label(tx, fVY + fVH - 6.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullassistedeq::sliderValue(param, v);
        if (param == 3)
            std::snprintf(buf, n, "%d%%", int(x));
        else
            std::snprintf(buf, n, "%+.1f dB", x);
    }

private:
    nullassistedeq::AssistedEqProcessor fDsp;
    float fGen = 0.0f;
};

UI* createUI()
{
    return new UIAssistedEq();
}

END_NAMESPACE_DISTRHO
