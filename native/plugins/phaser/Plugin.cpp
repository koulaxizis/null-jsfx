/*
 * NULL JSFX - Phaser (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PhaserDSP.hpp"
#include "PhaserSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPhaser : public NullMultiPlugin
{
public:
    PluginPhaser()
        : NullMultiPlugin(nullphaser::kSliders, nullphaser::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Phaser"; }
    const char* getDescription() const override
    {
        return "Phaser: 1 to 8 allpass stages swept by a sine LFO between 440 Hz and 1.6 kHz; rate, stages and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'p', 'h'); }

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
        using nullphaser::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 5.0, 0.1), value(1), value(2));
    }

    nullphaser::PhaserProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPhaser)
};

Plugin* createPlugin()
{
    return new PluginPhaser();
}

END_NAMESPACE_DISTRHO
