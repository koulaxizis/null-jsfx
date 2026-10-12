/*
 * NULL JSFX - Reverser (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ReverserDSP.hpp"
#include "ReverserSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginReverser : public NullMultiPlugin
{
public:
    PluginReverser()
        : NullMultiPlugin(nullreverser::kSliders, nullreverser::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Reverser"; }
    const char* getDescription() const override
    {
        return "Realtime reverser: a buffer written forwards and played backwards, with an optional filter and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 'r', 'v'); }

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
        // for the interface: the write and read heads across the buffer
        taps.extra.store(float(fDsp.writeFraction()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.readFraction()), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nullreverser::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(v(0), int(std::lround(v(1))), v(2), v(3)); }

    nullreverser::ReverserProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginReverser)
};

Plugin* createPlugin()
{
    return new PluginReverser();
}

END_NAMESPACE_DISTRHO
