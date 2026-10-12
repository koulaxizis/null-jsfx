/*
 * NULL JSFX - Tremolo (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "TremoloDSP.hpp"
#include "TremoloSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTremolo : public NullMultiPlugin
{
public:
    PluginTremolo()
        : NullMultiPlugin(nulltremolo::kSliders, nulltremolo::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Tremolo"; }
    const char* getDescription() const override
    {
        return "Tremolo: a sine LFO moves the level between full and 1 - depth; rate, depth and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 't', 'r'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nulltremolo::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 20.0, 0.1), value(1), value(2));
    }

    nulltremolo::TremoloProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTremolo)
};

Plugin* createPlugin()
{
    return new PluginTremolo();
}

END_NAMESPACE_DISTRHO
