/*
 * NULL JSFX - Auto-Leveler (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 5.2 s history as in auto_leveler_gfx.jsfx: one entry per 20 ms chunk,
 * pushed into the Taps ring (scopeL = input RMS level at the chunk's end, linear; scopeR = the
 * gain in dB). inLevel / outLevel = input / output RMS level (linear), extra = the gain in dB.
 */

#include "NullPlugin.hpp"
#include "AutoLevelerDSP.hpp"
#include "AutoLevelerSliders.hpp"

#include <algorithm>

START_NAMESPACE_DISTRHO

class PluginAutoLeveler : public NullMultiPlugin
{
public:
    PluginAutoLeveler()
        : NullMultiPlugin(nullautoleveler::kSliders, nullautoleveler::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Auto-Leveler"; }
    const char* getDescription() const override
    {
        return "Gain riding: moves the gain slowly toward the Target RMS level, within Max Boost / Max Cut, holding below Gate.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'a', 'l'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        setup(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        setup(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const double c = fDsp.rmsCoef();
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            fOutMs = c * fOutMs + (1.0 - c) * 0.5 * (l * l + r * r);
            if (++fN >= fChunk)
            {
                taps.push(float(std::sqrt(fDsp.meanSquare())), float(fDsp.gainDb()));
                fN = 0;
            }
        }
        taps.inLevel.store(float(std::sqrt(fDsp.meanSquare())), std::memory_order_relaxed);
        taps.outLevel.store(float(std::sqrt(fOutMs)), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.gainDb()), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.02)));
        fN = 0;
        fOutMs = 0.0;
    }

    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4));
    }

    nullautoleveler::AutoLevelerProcessor fDsp;
    uint32_t fChunk = 1, fN = 0;
    double fOutMs = 0.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAutoLeveler)
};

Plugin* createPlugin()
{
    return new PluginAutoLeveler();
}

END_NAMESPACE_DISTRHO
