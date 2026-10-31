/*
 * NULL JSFX - Assisted Delay (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AssistedDelayDSP.hpp"
#include "AssistedDelaySliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LEARN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "REPEATS", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "TONE", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "LEVEL", 1, -1, true },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIAssistedDelay : public NullKnobsUI
{
public:
    UIAssistedDelay()
        : NullKnobsUI({ "ASSISTED DELAY", "ASSISTED", 480, 140, nullptr, 2, nullassisteddelay::kSliders,
                        nullassisteddelay::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

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
                for (uint32_t i = 0; p && i < nullassisteddelay::kNumSliders; ++i)
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

    // as in assisted_delay_gfx.jsfx: the echoes, Learn's progress or the settings it chose,
    // and the ducking
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        for (uint32_t i = 0; i < nullassisteddelay::kNumSliders; ++i)
            fDsp.s[i] = nullassisteddelay::sliderValue(i, value(i));
        fDsp.setSampleRate(sr);
        // the tempo the plugin last saw (taps aux[3]), so the echoes sit where they will play
        {
            nulljsfx::Taps* tt = taps();
            const float bpm = tt ? tt->aux[3].load(std::memory_order_relaxed) : 0.0f;
            fDsp.block(bpm > 0.0f ? double(bpm) : 120.0);
        }
        setPlot(64.0f, 66.0f, 112.0f, 112.0f);
        grid(3, 4, false);
        char buf[64];
        for (int i = 0; i < 3; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -20 * i);
            label(fPX - 6.0f, fPY + fPH * float(i) / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        double g = 0;
        const double fbDb = fDsp.feedback() > 0 ? 20 * std::log10(fDsp.feedback()) : -100.0;
        for (int k = 1; k < 64; ++k)
        {
            const float xx = std::floor(linX(float(k * fDsp.timeMs() / 1000), 0.0f, 3.0f)) + 0.5f;
            const float yy = linY(float(g), -40.0f, 0.0f);
            beginPath();
            rect(xx - 0.5f, yy, 2.0f, fPY + fPH - yy);
            fillColor(col(theme::kAccent, fDsp.pingPong() && (k % 2) == 0 ? 0.5f : 1.0f));
            fill();
            g += fbDb;
            if (!((k + 1) * fDsp.timeMs() <= 3000 && g > -40))
                break;
        }
        const float tx = 200.0f;
        nulljsfx::Taps* t = taps();
        const bool listening = t && t->aux[2].load(std::memory_order_relaxed) >= 0.5f;
        const bool learned = nullassisteddelay::sliderValue(8, value(8)) >= 0.5;
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
            std::snprintf(buf, sizeof(buf), "TIME %d ms%s", int(std::floor(fDsp.timeMs() + 0.5)), fDsp.pingPong() ? "   PING-PONG" : "");
            label(tx, 90.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            std::snprintf(buf, sizeof(buf), "REPEATS %.1f   FEEDBACK %d%%", fDsp.repeats(), int(std::floor(fDsp.feedback() * 100 + 0.5)));
            label(tx, 106.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            std::snprintf(buf, sizeof(buf), "LOW CUT %d Hz   HIGH CUT %.1f kHz", int(fDsp.lowCut()), fDsp.highCut() / 1000);
            label(tx, 122.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            if (fDsp.wet() > 0)
                std::snprintf(buf, sizeof(buf), "LEVEL %.1f dB   DUCKING %.1f dB", 20 * std::log10(fDsp.wet()), fDsp.duck());
            else
                std::snprintf(buf, sizeof(buf), "LEVEL OFF   DUCKING %.1f dB", fDsp.duck());
            label(tx, 138.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
        }
        grMeter(tx, 166.0f, 248.0f, 10.0f, "DUCK", t ? std::fabs(t->gr.load(std::memory_order_relaxed)) : 0.0f, 12.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullassisteddelay::sliderValue(param, v);
        if (param == 3)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 4 || param == 5)
            std::snprintf(buf, n, "%+.1f", x);
        else
            std::snprintf(buf, n, "%+.1f dB", x);
    }

private:
    nullassisteddelay::AssistedDelayProcessor fDsp;
    float fGen = 0.0f;
};

UI* createUI()
{
    return new UIAssistedDelay();
}

END_NAMESPACE_DISTRHO
