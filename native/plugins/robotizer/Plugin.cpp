/*
 * NULL JSFX - Robotizer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "RobotizerDSP.hpp"
#include "RobotizerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginRobotizer : public NullMultiPlugin
{
public:
    PluginRobotizer()
        : NullMultiPlugin(nullrobotizer::kSliders, nullrobotizer::kNumSliders)
    {
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Robotizer"; }
    const char* getDescription() const override
    {
        return "FFT robot voice: zero-phase pulses at a fixed pitch, with a metallic comb and ring modulation.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '9', 'R', 'b'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        for (uint32_t i = 0; i < frames; ++i)
            taps.push(outputs[0][i], outputs[1][i]);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        setLatency(uint32_t(nullrobotizer::RobotizerProcessor::latency())); // pdc_delay = N
        update();
    }

    void update() { fDsp.setParams(value(0), value(1), value(2), value(3)); }

    nullrobotizer::RobotizerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRobotizer)
};

Plugin* createPlugin()
{
    return new PluginRobotizer();
}

END_NAMESPACE_DISTRHO
