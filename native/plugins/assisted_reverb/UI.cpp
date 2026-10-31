/*
 * NULL JSFX - Assisted Reverb (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AssistedReverbDSP.hpp"
#include "AssistedReverbSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LEARN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "LENGTH", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "TONE", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "LEVEL", 1, -1, true },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIAssistedReverb : public NullKnobsUI
{
public:
    UIAssistedReverb()
        : NullKnobsUI({ "ASSISTED REVERB", "ASSISTED", 480, 140, nullptr, 2, nullassistedreverb::kSliders,
                        nullassistedreverb::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

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
                for (uint32_t i = 0; p && i < nullassistedreverb::kNumSliders; ++i)
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

    // as in assisted_reverb_gfx.jsfx: the decay after the pre-delay, Learn's progress or
    // the settings it chose, and the ducking
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        for (uint32_t i = 0; i < nullassistedreverb::kNumSliders; ++i)
            fDsp.s[i] = nullassistedreverb::sliderValue(i, value(i));
        fDsp.setSampleRate(sr);
        setPlot(64.0f, 66.0f, 112.0f, 112.0f);
        grid(3, 4, false);
        char buf[64];
        for (int i = 0; i < 3; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -30 * i);
            label(fPX - 6.0f, fPY + fPH * float(i) / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float x0 = linX(float(fDsp.predelayMs() / 1000), 0.0f, 3.0f);
        const float x1 = linX(float(std::min(3.0, fDsp.predelayMs() / 1000 + fDsp.decay())), 0.0f, 3.0f);
        const float y1 = linY(float(-60 * std::min(1.0, (3 - fDsp.predelayMs() / 1000) / fDsp.decay())), -60.0f, 0.0f);
        const float xs[4] = { fPX, x0, x0, x1 };
        const float ys[4] = { fPY + fPH, fPY + fPH, fPY, y1 };
        curve(xs, ys, 4);
        const float tx = 200.0f;
        nulljsfx::Taps* t = taps();
        const bool listening = t && t->aux[2].load(std::memory_order_relaxed) >= 0.5f;
        const bool learned = nullassistedreverb::sliderValue(8, value(8)) >= 0.5;
        label(tx, 70.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted,
              listening ? "LISTENING" : learned ? "LEARNED" : "NOT LEARNED", true);
        if (listening)
        {
            const float prog = t->extra.load(std::memory_order_relaxed);
            beginPath();
            rect(tx, 82.0f, 248.0f, 4.0f);
            fillColor(col(theme::kBorder));
            fill();
            beginPath();
            rect(tx, 82.0f, 248.0f * prog, 4.0f);
            fillColor(col(theme::kAccent));
            fill();
            std::snprintf(buf, sizeof(buf), "%d of 15 s", int(std::floor(t->aux[0].load(std::memory_order_relaxed))));
            label(tx, 98.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "DECAY %.1f s   PRE-DELAY %d ms", fDsp.decay(), int(fDsp.predelayMs()));
            label(tx, 90.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            std::snprintf(buf, sizeof(buf), "LOW CUT %d Hz   HIGH CUT %.1f kHz", int(fDsp.lowCut()), fDsp.highCut() / 1000);
            label(tx, 106.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            if (fDsp.wet() > 0)
                std::snprintf(buf, sizeof(buf), "LEVEL %.1f dB", 20 * std::log10(fDsp.wet()));
            else
                std::snprintf(buf, sizeof(buf), "LEVEL OFF");
            label(tx, 122.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            std::snprintf(buf, sizeof(buf), "DUCKING %.1f dB", fDsp.duck());
            label(tx, 138.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
        }
        grMeter(tx, 166.0f, 248.0f, 10.0f, "DUCK", t ? std::fabs(t->gr.load(std::memory_order_relaxed)) : 0.0f, 12.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullassistedreverb::sliderValue(param, v);
        if (param == 3)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 4 || param == 5)
            std::snprintf(buf, n, "%+.1f", x);
        else
            std::snprintf(buf, n, "%+.1f dB", x);
    }

private:
    nullassistedreverb::AssistedReverbProcessor fDsp;
    float fGen = 0.0f;
};

UI* createUI()
{
    return new UIAssistedReverb();
}

END_NAMESPACE_DISTRHO
