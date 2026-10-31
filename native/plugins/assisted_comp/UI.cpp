/*
 * NULL JSFX - Assisted Comp (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "AssistedCompDSP.hpp"
#include "AssistedCompSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LEARN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SQUASH", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "SPEED", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIAssistedComp : public NullKnobsUI
{
public:
    UIAssistedComp()
        : NullKnobsUI({ "ASSISTED COMP", "ASSISTED", 480, 140, nullptr, 2, nullassistedcomp::kSliders,
                        nullassistedcomp::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

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
                for (uint32_t i = 0; p && i < nullassistedcomp::kNumSliders; ++i)
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

    // as in assisted_comp_gfx.jsfx: the static curve with the threshold, Learn's progress or
    // the settings it chose, and the gain reduction
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        for (uint32_t i = 0; i < nullassistedcomp::kNumSliders; ++i)
            fDsp.s[i] = nullassistedcomp::sliderValue(i, value(i));
        fDsp.setSampleRate(sr);

        setPlot(64.0f, 66.0f, 112.0f, 112.0f);
        grid(4, 4, false);
        char buf[64];
        for (int i = 0; i < 3; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -30 * i);
            label(fPX - 6.0f, fPY + fPH * float(i) / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();
        if (fDsp.active() && fDsp.threshold() > -60)
            dashedV(linX(float(fDsp.threshold()), -60.0f, 0.0f));
        constexpr int n = 113;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(fDsp.curve(-60.0 + 60.0 * double(i) / double(n - 1))), -60.0f, 0.0f);
        }
        curve(xs, ys, n);

        const float tx = 200.0f;
        nulljsfx::Taps* t = taps();
        const bool listening = t && t->aux[2].load(std::memory_order_relaxed) >= 0.5f;
        const bool learned = nullassistedcomp::sliderValue(8, value(8)) >= 0.5;
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
            if (fDsp.active())
            {
                std::snprintf(buf, sizeof(buf), "THRESHOLD %.1f dB   RATIO %.1f:1", fDsp.threshold(), fDsp.ratio());
                label(tx, 90.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
                std::snprintf(buf, sizeof(buf), "ATTACK %.1f ms   RELEASE %d ms", fDsp.attack(), int(fDsp.release()));
                label(tx, 106.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
                std::snprintf(buf, sizeof(buf), "MAKEUP %+.1f dB", fDsp.makeup());
                label(tx, 122.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            }
            else
                label(tx, 90.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "NO COMPRESSION (AMOUNT 0)");
            if (fDsp.autoLevel() != 0)
            {
                std::snprintf(buf, sizeof(buf), "LOUDNESS %+.1f dB TO %d LUFS", fDsp.autoLevel(), int(fDsp.loudnessTarget()));
                label(tx, 138.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);
            }
        }
        grMeter(tx, 166.0f, 248.0f, 10.0f, "GR", t ? std::fabs(t->gr.load(std::memory_order_relaxed)) : 0.0f, 24.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullassistedcomp::sliderValue(param, v);
        if (param == 3 || param == 6)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 5)
            std::snprintf(buf, n, "%+.1f", x);
        else
            std::snprintf(buf, n, "%+.1f dB", x);
    }

private:
    nullassistedcomp::AssistedCompProcessor fDsp;
    float fGen = 0.0f;
};

UI* createUI()
{
    return new UIAssistedComp();
}

END_NAMESPACE_DISTRHO
